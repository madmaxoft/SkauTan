#include "BackgroundIO.hpp"

#include <QFile>
#include <QDir>
#include <QSemaphore>





BackgroundIO::BackgroundIO():
	mMtx(QMutex::NonRecursive),
	mShouldTerminate(false)
{
	// Simple redirector that diverts the thread to our internal function
	struct ExecutorThread: public QThread
	{
		BackgroundIO & mParent;

		ExecutorThread(BackgroundIO & aParent):
			mParent(aParent)
		{
		}

		virtual void run() override
		{
			mParent.processRequests();
		}
	};

	// Start the thread:
	mThread = std::make_unique<ExecutorThread>(*this);
	mThread->start();
}





BackgroundIO::~BackgroundIO()
{
	// Tell all executors to terminate:
	qDebug() << "Terminating IO...";
	mShouldTerminate = true;
	mWaitForRequests.wakeAll();

	// Wait for all executors to terminate:
	qDebug() << "Waiting for pending IO...";
	mThread->wait();

	// Abort all tasks left over in the queue:
	qDebug() << "Aborting non-executed file IO.";
	QMutexLocker lock(&mMtx);
	for (const auto & f: mFileReadRequests)
	{
		f->mErrorHandler(tr("Aborted"));
	}
	mFileReadRequests.clear();
	for (const auto & f: mFolderReadRequests)
	{
		f->mErrorHandler(tr("Aborted"));
	}
	mFolderReadRequests.clear();
}





void BackgroundIO::readFile(
	BackgroundIO::Priority aPriority,
	const QString & aFileName,
	BackgroundIO::FileDataHandler aDataHandler,
	BackgroundIO::ErrorHandler aErrorHandler,
	qint64 aStartPos,
	qint64 aLength
)
{
	// If we're being destroyed, throw an exception (the caller didn't take a proper shared_ptr of us):
	if (mShouldTerminate.load())
	{
		throw Exception(tr("Attempting to read file %1 while shutting down").arg(aFileName));
	}

	{
		auto item = std::make_unique<FileReadRequest>(aPriority, aFileName, aDataHandler, aErrorHandler, aStartPos, aLength);
		bool hasInserted = false;
		QMutexLocker lock(&mMtx);
		for (auto itr = mFileReadRequests.begin(), end = mFileReadRequests.end(); itr != end; ++itr)
		{
			if ((*itr)->mPriority > aPriority)
			{
				mFileReadRequests.insert(itr, std::move(item));
				hasInserted = true;
				break;
			}
		}
		if (!hasInserted)
		{
			mFileReadRequests.push_back(std::move(item));
		}
	}
	mWaitForRequests.wakeOne();
}





void BackgroundIO::readFolder(
	BackgroundIO::Priority aPriority,
	const QString & aFolder,
	BackgroundIO::FolderEntriesHandler aEntriesHandler,
	BackgroundIO::ErrorHandler aErrorHandler
)
{
	// If we're being destroyed, throw an exception (the caller didn't take a proper shared_ptr of us):
	if (mShouldTerminate.load())
	{
		throw Exception(tr("Attempting to read folder %1 while shutting down").arg(aFolder));
	}

	{
		auto item = std::make_unique<FolderReadRequest>(aPriority, aFolder, aEntriesHandler, aErrorHandler);
		bool hasInserted = false;
		QMutexLocker lock(&mMtx);
		for (auto itr = mFolderReadRequests.begin(), end = mFolderReadRequests.end(); itr != end; ++itr)
		{
			if ((*itr)->mPriority > aPriority)
			{
				mFolderReadRequests.insert(itr, std::move(item));
				hasInserted = true;
				break;
			}
		}
		if (!hasInserted)
		{
			mFolderReadRequests.push_back(std::move(item));
		}
	}
	mWaitForRequests.wakeOne();
}





QByteArray BackgroundIO::readEntireFileSync(Priority aPriority, const QString & aFileName)
{
	assert(QThread::currentThread() != mThread.get());  // We'll be waiting for mThread, so can't be there
	QSemaphore sem(0);
	QByteArray res;
	QString err;
	readFile(aPriority, aFileName,
		[&sem, &res](const QByteArray & aFileData)  // data handler
		{
			res = aFileData;
			sem.release();
		},
		[&sem, &err](const QString & aErrorMsg)  // error handler
		{
			err = aErrorMsg;
			sem.release();
		}
	);
	sem.acquire();  // Wait for the read
	if (!err.isEmpty())
	{
		throw Exception(err);
	}
	return res;
}





void BackgroundIO::processRequests()
{
	while (!mShouldTerminate.load())
	{
		// Wait for a request:
		QMutexLocker lock(&mMtx);
		if (mShouldTerminate)
		{
			return;
		}
		while (mFileReadRequests.empty() && mFolderReadRequests.empty())
		{
			if (mShouldTerminate)
			{
				return;
			}
			mWaitForRequests.wait(&mMtx);
		}

		// Process the request:
		if (mFileReadRequests.empty())
		{
			// There are only folder requests
			auto req = std::move(mFolderReadRequests.back());
			mFolderReadRequests.pop_back();
			lock.unlock();
			processFolderRequest(*req);
		}
		else if (mFolderReadRequests.empty())
		{
			// There are only file requests
			auto req = std::move(mFileReadRequests.back());
			mFileReadRequests.pop_back();
			lock.unlock();
			processFileRequest(*req);
		}
		else
		{
			// There are both file and folder requests, execute the one with higher priority (or folder, if same):
			if (mFileReadRequests[0]->mPriority > mFolderReadRequests[0]->mPriority)
			{
				auto req = std::move(mFileReadRequests.back());
				mFileReadRequests.pop_back();
				lock.unlock();
				processFileRequest(*req);
			}
			else
			{
				auto req = std::move(mFolderReadRequests.back());
				mFolderReadRequests.pop_back();
				lock.unlock();
				processFolderRequest(*req);
			}
		}
	}  // while (!mShouldTerminate)
}





void BackgroundIO::processFileRequest(const BackgroundIO::FileReadRequest & aRequest)
{
	QFile f(aRequest.mFileName);
	if (!f.open(QFile::ReadOnly))
	{
		aRequest.mErrorHandler(tr("Cannot open file %1 for reading").arg(aRequest.mFileName));
		return;
	}
	if (!f.seek(aRequest.mStartPos))
	{
		aRequest.mErrorHandler(
			tr("Cannot seek to start of data (%1, offset %2)")
			.arg(aRequest.mFileName)
			.arg(aRequest.mStartPos)
		);
		return;
	}
	auto ba = f.read(aRequest.mLength);
	aRequest.mDataHandler(ba);
}





void BackgroundIO::processFolderRequest(const BackgroundIO::FolderReadRequest & aRequest)
{
	QDir dir(aRequest.mFolder + "/");
	if (!dir.exists())
	{
		aRequest.mErrorHandler(tr("Folder %1 doesn't exist").arg(aRequest.mFolder));
		return;
	}
	aRequest.mEntriesHandler(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot));
}

#pragma once

#include <memory>
#include <limits>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QFileInfo>
#include "ComponentCollection.hpp"





/** Serializes the disk IO into a single background thread.
Takes requests to read files and folders, together with actions to do once finished.
Queues those up and executes them one after another.
Supports a simple priority system so that a task may indicate how urgent it is.
Owns and hides the thread on which it executes the IO tasks. */
class BackgroundIO:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckBackgroundIO>
{
	Q_OBJECT

public:

	/** The priorities for the IO tasks. */
	enum class Priority
	{
		Low,     ///< For background tasks
		Normal,  ///< For (background) tasks that the user explicitly initiated (adding / scanning files)
		High,    ///< For tasks that have a UI interaction
	};

	using FileDataHandler = std::function<void (const QByteArray & aFileContents)>;
	using FolderEntriesHandler = std::function<void (const QFileInfoList & aFolderEntries)>;
	using ErrorHandler = std::function<void (const QString & aErrorMsg)>;


	/** Constructs a new instance of this class, with a new thread for scheduling the tasks. */
	BackgroundIO();

	virtual ~BackgroundIO();

	/** Enqueues a file read task - reads the specified portion of the file into an in-memory byte array.
	Calls aDataHandler with the file contents once the file is read.
	Calls aErrorHandler instead if an error occurs.
	If the file is shorter than aStartPos, an error is raised.
	If the file is shorter than aStartPos + aLength, only partial contents is sent to aDataHandler.
	To read the whole file, simply use the default start and length values. */
	void readFile(
		Priority aPriority,
		const QString & aFileName,
		FileDataHandler aDataHandler,
		ErrorHandler aErrorHandler,
		qint64 aStartPos = 0,
		qint64 aLength = std::numeric_limits<qint64>::max()
	);

	/** Enqueues a folder read task - reads all the items in the folder.
	Calls aContentsHandler with the folder contents once the folder is scanned.
	Calls aErrorHandler instead if an error occurs.
	Doesn't report the "." and ".." folders. */
	void readFolder(
		Priority aPriority,
		const QString & aFolder,
		FolderEntriesHandler aEntriesHandler,
		ErrorHandler aErrorHandler
	);

	/** Reads the entire specified file, via the background IO thread.
	Waits for the read to finish.
	Returns the file contents.
	Throws an Exception on error. */
	QByteArray readEntireFileSync(Priority aPriority, const QString & aFileName);

protected:

	/** Simple storage for a single file read request. */
	struct FileReadRequest
	{
		Priority mPriority;
		QString mFileName;
		FileDataHandler mDataHandler;
		ErrorHandler mErrorHandler;
		qint64 mStartPos;
		qint64 mLength;

		FileReadRequest(
			Priority aPriority,
			const QString & aFileName,
			FileDataHandler aDataHandler,
			ErrorHandler aErrorHandler,
			qint64 aStartPos,
			qint64 aLength
		):
			mPriority(aPriority),
			mFileName(aFileName),
			mDataHandler(aDataHandler),
			mErrorHandler(aErrorHandler),
			mStartPos(aStartPos),
			mLength(aLength)
		{
		}
	};


	/** Simple storage for a single folder read request. */
	struct FolderReadRequest
	{
		Priority mPriority;
		QString mFolder;
		FolderEntriesHandler mEntriesHandler;
		ErrorHandler mErrorHandler;

		FolderReadRequest(
			Priority aPriority,
			const QString & aFolder,
			FolderEntriesHandler aEntriesHandler,
			ErrorHandler aErrorHandler
		):
			mPriority(aPriority),
			mFolder(aFolder),
			mEntriesHandler(aEntriesHandler),
			mErrorHandler(aErrorHandler)
		{
		}
	};


	/** The queued requests for reading files, sorted by priority.
	Protected against multithreaded access by mMtx. */
	std::vector<std::unique_ptr<FileReadRequest>> mFileReadRequests;

	/** The queued requests for reading folders, sorted by priority.
	Protected against multithreaded access by mMtx. */
	std::vector<std::unique_ptr<FolderReadRequest>> mFolderReadRequests;

	/** The mutex protecting mFileReadRequests and mFolderReadRequests against multithreaded access. */
	mutable QMutex mMtx;

	/** The waitcondition used to wait for any of the requests to arrive. */
	QWaitCondition mWaitForRequests;

	/** The thread on which the IO operations are serialized. */
	std::unique_ptr<QThread> mThread;

	/** Flag that is set when the entire background processing should terminate as soon as possible. */
	std::atomic<bool> mShouldTerminate;


	/** Waits for requests in mFileReadRequests and mFolderReadRequests and fulfills them.
	The main entrypoint for the request processing thread. */
	void processRequests();

	/** Processes the specified file read request. */
	void processFileRequest(const FileReadRequest & aRequest);

	/** Processes the specified folder read request. */
	void processFolderRequest(const FolderReadRequest & aRequest);
};

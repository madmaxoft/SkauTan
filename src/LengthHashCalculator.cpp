#include "LengthHashCalculator.hpp"
#include <cassert>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include "Audio/AVPP.hpp"
#include "BackgroundTasks.hpp"





LengthHashCalculator::LengthHashCalculator():
	mQueueLength(0)
{
}





std::pair<QByteArray, double> LengthHashCalculator::calculateSongHashAndLength(const QString & aFileName)
{
	auto context = AVPP::Format::createContext(aFileName);
	if (context == nullptr)
	{
		qWarning() << "Cannot open song file for hash calculation: " << aFileName;
		return std::make_pair(QByteArray(), -1);
	}
	QCryptographicHash ch(QCryptographicHash::Sha1);
	double length = 0;
	if (!context->feedRawAudioDataTo([&](const void * aData, int aSize)
		{
			assert(aSize >= 0);
			ch.addData(reinterpret_cast<const char *>(aData), aSize);
		},
		length
	))
	{
		qWarning() << "Cannot read song data for hash calculation: " << aFileName;
		return std::make_pair(QByteArray(), -1);
	}
	return std::make_pair(ch.result(), length);
}





double LengthHashCalculator::calculateSongLength(const QString & aFileName)
{
	auto context = AVPP::Format::createContext(aFileName);
	if (context == nullptr)
	{
		qWarning() << "Cannot open song file for length calculation: " << aFileName;
		return -1;
	}
	double length = 0;
	if (!context->feedRawAudioDataTo([&](const void *, int) {}, length))
	{
		qWarning() << "Cannot read song data for length calculation: " << aFileName;
		return -1;
	}
	return length;
}





void LengthHashCalculator::queueHashFile(const QString & aFileName)
{
	mQueueLength += 1;
	QString fileName(aFileName);
	BackgroundTasks::enqueue(tr("Calculate hash: %1").arg(aFileName), [this, fileName]()
		{
			auto hashAndLength = calculateSongHashAndLength(fileName);
			mQueueLength -= 1;
			if (hashAndLength.first.isEmpty())
			{
				emit this->fileHashFailed(fileName);
				return;
			}
			emit this->fileHashCalculated(fileName, hashAndLength.first, hashAndLength.second);
		}
	);
}




void LengthHashCalculator::queueLengthSong(Song::SharedDataPtr aSharedData)
{
	auto duplicates = aSharedData->duplicates();
	if (duplicates.empty())
	{
		return;
	}
	auto firstFileName = duplicates[0]->fileName();
	mQueueLength += 1;
	BackgroundTasks::enqueue(tr("Calculate length: %1").arg(firstFileName), [this, aSharedData, duplicates]()
		{
			// Pick the first duplicate that exists:
			QString fileName;
			for (const auto & d: duplicates)
			{
				if (QFile::exists(d->fileName()))
				{
					fileName = d->fileName();
					break;
				}
			}
			if (fileName.isEmpty())
			{
				qWarning() << "There is no file representing song " << aSharedData->mHash;
				mQueueLength -= 1;
				emit this->songLengthFailed(aSharedData);
				return;
			}

			// Calculate the length:
			auto length = calculateSongLength(fileName);
			mQueueLength -= 1;
			if (length < 0)
			{
				emit this->songLengthFailed(aSharedData);
				return;
			}
			emit this->songLengthCalculated(aSharedData, length);
		}
	);
}

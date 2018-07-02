#include "LengthHashCalculator.h"
#include <assert.h>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include "AVPP.h"
#include "BackgroundTasks.h"





LengthHashCalculator::LengthHashCalculator():
	m_QueueLength(0)
{
}





std::pair<QByteArray, double> LengthHashCalculator::calculateSongHashAndLength(const QString & a_FileName)
{
	auto context = AVPP::Format::createContext(a_FileName);
	if (context == nullptr)
	{
		qWarning() << ": Cannot open song file for hash calculation: " << a_FileName;
		return std::make_pair(QByteArray(), -1);
	}
	QCryptographicHash ch(QCryptographicHash::Sha1);
	double length = 0;
	if (!context->feedRawAudioDataTo([&](const void * a_Data, int a_Size)
		{
			assert(a_Size >= 0);
			ch.addData(reinterpret_cast<const char *>(a_Data), a_Size);
		},
		length
	))
	{
		qWarning() << ": Cannot read song data for hash calculation: " << a_FileName;
		return std::make_pair(QByteArray(), -1);
	}
	return std::make_pair(ch.result(), length);
}





double LengthHashCalculator::calculateSongLength(const QString & a_FileName)
{
	auto context = AVPP::Format::createContext(a_FileName);
	if (context == nullptr)
	{
		qWarning() << ": Cannot open song file for length calculation: " << a_FileName;
		return -1;
	}
	double length = 0;
	if (!context->feedRawAudioDataTo([&](const void *, int) {}, length))
	{
		qWarning() << ": Cannot read song data for length calculation: " << a_FileName;
		return -1;
	}
	return length;
}





void LengthHashCalculator::queueHashFile(const QString & a_FileName)
{
	m_QueueLength += 1;
	QString fileName(a_FileName);
	BackgroundTasks::enqueue(tr("Calculate hash: %1").arg(a_FileName), [this, fileName]()
		{
			auto hashAndLength = calculateSongHashAndLength(fileName);
			m_QueueLength -= 1;
			if (hashAndLength.first.isEmpty())
			{
				emit this->fileHashFailed(fileName);
				return;
			}
			emit this->fileHashCalculated(fileName, hashAndLength.first, hashAndLength.second);
		}
	);
}




void LengthHashCalculator::queueLengthSong(Song::SharedDataPtr a_SharedData)
{
	auto duplicates = a_SharedData->duplicates();
	if (duplicates.empty())
	{
		return;
	}
	auto firstFileName = duplicates[0]->fileName();
	m_QueueLength += 1;
	BackgroundTasks::enqueue(tr("Calculate length: %1").arg(firstFileName), [this, a_SharedData, duplicates]()
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
				qWarning() << "There is no file representing song " << a_SharedData->m_Hash;
				m_QueueLength -= 1;
				emit this->songLengthFailed(a_SharedData);
				return;
			}

			// Calculate the length:
			auto length = calculateSongLength(fileName);
			m_QueueLength -= 1;
			if (length < 0)
			{
				emit this->songLengthFailed(a_SharedData);
				return;
			}
			emit this->songLengthCalculated(a_SharedData, length);
		}
	);
}

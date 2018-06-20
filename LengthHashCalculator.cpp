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





void LengthHashCalculator::queueHashFile(const QString & a_FileName)
{
	m_QueueLength += 1;
	QString fileName(a_FileName);
	BackgroundTasks::enqueue(tr("Calculate hash: %1").arg(a_FileName), [this, fileName]()
		{
			auto context = AVPP::Format::createContext(fileName);
			if (context == nullptr)
			{
				qWarning() << ": Cannot open song file for hash calculation: " << fileName;
				m_QueueLength -= 1;
				emit this->fileHashFailed(fileName);
				return;
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
				qWarning() << ": Cannot read song data for hash calculation: " << fileName;
				m_QueueLength -= 1;
				emit this->fileHashFailed(fileName);
				return;
			}
			m_QueueLength -= 1;
			emit fileHashCalculated(fileName, ch.result(), length);
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
			auto context = AVPP::Format::createContext(fileName);
			if (context == nullptr)
			{
				qWarning() << ": Cannot open song file for length calculation: " << fileName;
				m_QueueLength -= 1;
				emit this->songLengthFailed(a_SharedData);
				return;
			}
			double length = 0;
			if (!context->feedRawAudioDataTo([&](const void *, int) {}, length))
			{
				qWarning() << ": Cannot read song data for length calculation: " << fileName;
				m_QueueLength -= 1;
				emit this->songLengthFailed(a_SharedData);
				return;
			}
			m_QueueLength -= 1;
			emit songLengthCalculated(a_SharedData, length);
		}
	);
}

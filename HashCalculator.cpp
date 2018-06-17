#include "HashCalculator.h"
#include <assert.h>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include "Song.h"
#include "AVPP.h"
#include "BackgroundTasks.h"





HashCalculator::HashCalculator():
	m_QueueLength(0)
{
}





void HashCalculator::queueHashSong(SongPtr a_Song)
{
	m_QueueLength += 1;
	BackgroundTasks::enqueue(tr("Calculate hash: %1").arg(a_Song->fileName()), [this, a_Song]()
		{
			auto context = AVPP::Format::createContext(a_Song->fileName());
			if (context == nullptr)
			{
				qWarning() << ": Cannot open song file for hash calculation: " << a_Song->fileName();
				m_QueueLength -= 1;
				emit this->songHashFailed(a_Song);
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
				qWarning() << ": Cannot read song data for hash calculation: " << a_Song->fileName();
				m_QueueLength -= 1;
				emit this->songHashFailed(a_Song);
				return;
			}
			a_Song->setHash(ch.result());
			m_QueueLength -= 1;
			emit songHashCalculated(a_Song, length);
		}
	);
}

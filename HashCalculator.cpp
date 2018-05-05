#include "HashCalculator.h"
#include <assert.h>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include "Song.h"
#include "AVPP.h"
#include "BackgroundTasks.h"





HashCalculator::HashCalculator()
{
	// Nothing needed yet
}





void HashCalculator::queueHashSong(SongPtr a_Song)
{
	BackgroundTasks::enqueue([this, a_Song]()
		{
			auto context = AVPP::Format::createContext(a_Song->fileName());
			if (context == nullptr)
			{
				qWarning() << __FUNCTION__ << ": Cannot open song file for hash calculation: " << a_Song->fileName();
				emit this->songHashFailed(a_Song);
				return;
			}
			QCryptographicHash ch(QCryptographicHash::Sha1);
			if (!context->feedRawAudioDataTo([&](const void * a_Data, int a_Size)
				{
					assert(a_Size >= 0);
					ch.addData(reinterpret_cast<const char *>(a_Data), a_Size);
				}
			))
			{
				qWarning() << __FUNCTION__ << ": Cannot read song data for hash calculation: " << a_Song->fileName();
				emit this->songHashFailed(a_Song);
				return;
			}
			a_Song->setHash(ch.result());
			emit this->songHashCalculated(a_Song);
		}
	);
}

#include "HashCalculator.h"
#include <assert.h>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include "Song.h"
#include "AVPP.h"





HashCalculator::HashCalculator(Song & a_Song):
	m_Song(a_Song)
{

}





void HashCalculator::calc(Song & a_Song)
{
	HashCalculator hc(a_Song);
	return hc.calc();
}





void HashCalculator::calc()
{
	auto context = AVPP::Format::createContext(m_Song.fileName());
	if (context == nullptr)
	{
		qWarning() << __FUNCTION__ << ": Cannot open song file for hash calculation: " << m_Song.fileName();
		// TODO: Emit an error signal
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
		qWarning() << __FUNCTION__ << ": Cannot read song data for hash calculation: " << m_Song.fileName();
		// TODO: Emit an error signal
		return;
	}
	m_Song.setHash(ch.result());
	//TODO: Emit a success signal
}

#include "SongDecoder.h"
#include <QtConcurrent/QtConcurrent>
#include "AVPP.h"




std::atomic<bool> g_IsAVInitialized(false);





////////////////////////////////////////////////////////////////////////////////
// SongDecoder:

SongDecoder::SongDecoder(const QAudioFormat & a_OutputFormat, SongPtr a_Song):
	Super(a_OutputFormat),
	m_Song(a_Song),
	m_Termination(0)
{
	QtConcurrent::run(this, &SongDecoder::decode);
}





SongDecoder::~SongDecoder()
{
	abort();
	m_Termination.acquire();
}





void SongDecoder::decode()
{
	decodeInternal();
	m_Termination.release();
}





void SongDecoder::decodeInternal()
{
	// Open the file:
	auto fmtCtx = AVPP::Format::createContext(m_Song->fileName());
	if (fmtCtx == nullptr)
	{
		qDebug() << "Decoding failed for file " << m_Song->fileName();
		abort();
		return;
	}
	fmtCtx->routeAudioTo(this);
	fmtCtx->decode();
}

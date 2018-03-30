#include "SongDecoder.h"
#include <QtConcurrent/QtConcurrent>




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





void SongDecoder::seekTo(double a_Time)
{
	// Seek in the decoder:
	m_FmtCtx->seekTo(a_Time);

	// Seek in the PlaybackBuffer:
	Super::seekTo(a_Time);
}





void SongDecoder::decode()
{
	decodeInternal();
	m_Termination.release();
}





void SongDecoder::decodeInternal()
{
	// Open the file:
	m_FmtCtx = AVPP::Format::createContext(m_Song->fileName());
	if (m_FmtCtx == nullptr)
	{
		qDebug() << "Decoding failed for file " << m_Song->fileName();
		abort();
		return;
	}
	m_FmtCtx->routeAudioTo(this);
	m_FmtCtx->decode();
}

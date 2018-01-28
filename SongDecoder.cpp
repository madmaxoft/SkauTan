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
	// Initialize AV libs:
	static bool isAVInitialized = false;
	if (!isAVInitialized)
	{
		isAVInitialized = true;
		av_register_all();
	}

	// Open the file:
	auto fmtCtx = AVPP::Format::createContext(m_Song->fileName());
	if (fmtCtx == nullptr)
	{
		return;
	}
	fmtCtx->routeAudioTo(this);
	fmtCtx->decode();
}

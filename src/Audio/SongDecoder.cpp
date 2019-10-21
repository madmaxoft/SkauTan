#include "SongDecoder.hpp"
#include <QtConcurrent/QtConcurrent>





////////////////////////////////////////////////////////////////////////////////
// SongDecoder:

SongDecoder::SongDecoder(const QAudioFormat & aOutputFormat, SongPtr aSong):
	Super(aOutputFormat),
	mSong(aSong),
	mTermination(0)
{
	QtConcurrent::run(this, &SongDecoder::decode);
	if (aSong->skipStart().isPresent())
	{
		qDebug() << "Skip-start active: " << aSong->skipStart().value();
		seekToFrame(aOutputFormat.framesForDuration(static_cast<qint64>(aSong->skipStart().value() * 1000000)));
	}
}





SongDecoder::~SongDecoder()
{
	abort();
	mTermination.acquire();
}





void SongDecoder::decode()
{
	decodeInternal();
	mTermination.release();
}





void SongDecoder::decodeInternal()
{
	// Open the file:
	mFmtCtx = AVPP::Format::createContext(mSong->fileName());
	if (mFmtCtx == nullptr)
	{
		qDebug() << "Decoding failed for file " << mSong->fileName();
		abortWithError();
		return;
	}
	mFmtCtx->routeAudioTo(this);
	mFmtCtx->decode();
	qDebug() << "Decoding has finished file " << mSong->fileName();
	decodedEOF();
}

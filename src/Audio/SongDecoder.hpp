#pragma once

#include <QSemaphore>
#include "../Song.hpp"
#include "PlaybackBuffer.hpp"
#include "AVPP.hpp"





class SongDecoder:
	public PlaybackBuffer
{
	using Super = PlaybackBuffer;

public:
	SongDecoder(const QAudioFormat & aOutputFormat, SongPtr aSong);

	virtual ~SongDecoder() override;


protected:

	/** The song that should be decoded. */
	SongPtr mSong;

	/** Semaphore used to coordinate termination.
	The asynchronous decoder signals this semaphore when it finishes. */
	QSemaphore mTermination;

	/** The LibAV handle to the decoder. */
	AVPP::FormatPtr mFmtCtx;


	/** Decodes the song data asynchronously. */
	void decode();

	/** Internal entrypoint for the decoding. */
	void decodeInternal();
};

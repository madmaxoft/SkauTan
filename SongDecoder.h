#ifndef SONGDECODER_H
#define SONGDECODER_H





#include <QSemaphore>
#include "PlaybackBuffer.h"
#include "Song.h"





class SongDecoder:
	public PlaybackBuffer
{
	using Super = PlaybackBuffer;

public:
	SongDecoder(const QAudioFormat & a_OutputFormat, SongPtr a_Song);

	virtual ~SongDecoder();


protected:

	/** The song that should be decoded. */
	SongPtr m_Song;

	/** Semaphore used to coordinate termination.
	The asynchronous decoder signals this semaphore when it finishes. */
	QSemaphore m_Termination;


	/** Decodes the song data asynchronously. */
	void decode();

	/** Internal entrypoint for the decoding. */
	void decodeInternal();
};





#endif // SONGDECODER_H

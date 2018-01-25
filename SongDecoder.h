#ifndef DECODER_H
#define DECODER_H





#include <memory>
#include <QThread>
#include <QAudioDecoder>





// fwd:
class PlaybackBuffer;
class Song;





/** Background song audio data decoder.
Reads the song file and decodes its audio into a PlaybackBuffer in a background thread. */
class SongDecoder:
	QObject
{
	Q_OBJECT

public:
	SongDecoder();

	/** Starts decoding in a background thread.
	Returns the buffer into which the decoding will place the output data.
	Will block until the decoder parses the song format and metadata, so that it can create an appropriate buffer. */
	std::shared_ptr<PlaybackBuffer> start(const QAudioFormat & a_Format, const Song & a_Song);

	/** Stops decoding. */
	void stop();

protected:

	/** The background thread in which the decoder runs. */
	QThread m_DecoderThread;

	/** The Qt audio decoder. */
	QAudioDecoder m_Decoder;

	/** The output buffer where the decoded data gets written. */
	std::shared_ptr<PlaybackBuffer> m_Output;
};





#endif  // DECODER_H

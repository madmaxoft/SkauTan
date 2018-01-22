#ifndef PLAYBACKBUFFER_H
#define PLAYBACKBUFFER_H





#include <QAudioFormat>
#include <QByteArray>
#include <QIODevice>





/** Implements the buffer that is used for playing back audio.
Each playlist item creates an instance of this buffer and fills it in async;
the main player reads the data using the QIODevice interface and plays it back.
The main player can seek in the audio data; in such a case, the reading operation blocks until the async decoding
reaches the seeking point. */
class PlaybackBuffer:
	public QIODevice
{
	using Super = QIODevice;
	Q_OBJECT

public:

	/** Creates a new instance that has the specified audio format. */
	PlaybackBuffer(const QAudioFormat & a_Format, int a_ByteSize);

	/** Returns the audio format of the contained data. */
	const QAudioFormat & audioFormat() const { return m_AudioFormat; }

	/** Writes the specified audio data into the buffer at current write-position. */
	void write(const char * a_Data, int a_Size);

	/** Sets the pointer to a decoder that is remembered for this buffer.
	The decoder pointer is used by clients of this class to bind it to the decoder producing the audio data.
	This class has no use of the decoder, it only stores it.*/
	void setDecoder(QObject * a_Decoder) { m_Decoder = a_Decoder; }

	/** Returns the decoder object remembered previously with setDecoder(). */
	QObject * decoder() const { return m_Decoder; }

	/** Fades the audio data from current read pos out, signals data end upon reaching the end of the fadeout. */
	void fadeOut();


	// QIODevice overrides:
	virtual qint64 readData(char * a_Data, qint64 a_MaxLen) override;
	virtual qint64 writeData(const char * a_Data, qint64 a_Len) override;


protected:

	/** The format of the contained audio. */
	QAudioFormat m_AudioFormat;

	/** The actual audio data. */
	QByteArray m_Audio;

	/** The current position in m_Audio where next data is to be read from for the player. */
	int m_CurrentReadPos;

	/** The decoder object, set by the client of this class.
	Not used in this class' implementation, but stored for convenience. */
	QObject * m_Decoder;


signals:

	/** Emitted when data is available for output. */
	void dataReady();
};





#endif // PLAYBACKBUFFER_H

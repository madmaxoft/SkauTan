#ifndef PLAYBACKBUFFER_H
#define PLAYBACKBUFFER_H





#include <atomic>

#include <QIODevice>
#include <QAudioFormat>
#include <QWaitCondition>
#include <QMutex>





/** Buffer for the decoded audio data waiting to be sent to the audio output.
The audio decoder uses the writeDecodedAudio() function to add more data (possibly blocking).
The audio output uses the QIODevice interface to read the output data.
Once there is no more data to output, the QIODevice signals an end-of-file.
The audio decoder is expected to provide a class derived from this in the IPlaylistItem->startPlaying() function. */
class PlaybackBuffer:
	public QIODevice
{
public:
	PlaybackBuffer(const QAudioFormat & a_OutputFormat);

	// Force a virtual destructor in descendants
	virtual ~PlaybackBuffer() {}

	/** Signals that the operations should abort, instead of waiting for async operations to finish.
	Used mainly by destructors. */
	void abort();

	/** Returns true iff the decoder should terminate (abort has been called). */
	bool shouldAbort() const { return m_ShouldAbort.load(); }

	/** Fades the data out and sets the termination flag, so that the output is terminated after the fadeout. */
	void fadeOut(int a_Msec);

	/** Waits until decoded data is available.
	Returns true if data has arrived, false if aborted. */
	bool waitForData();

	/** Writes the specified data to the buffer.
	Blocks until all the data can fit into the buffer.
	Wakes up the output from its initial pause, if needed.
	Returns true if the decoder should continue, false if it should abort. */
	bool writeDecodedAudio(const void * a_Data, size_t a_Len);

	const QAudioFormat & format() const { return m_OutputFormat; }

protected:

	/** The audio format of the output data. */
	QAudioFormat m_OutputFormat;

	/** If set to true, operations on this instance abort without waiting. */
	std::atomic<bool> m_ShouldAbort;


	// QIODevice overrides:
	virtual qint64 readData(char *a_Data, qint64 a_MaxLen) override;
	virtual qint64 writeData(const char *a_Data, qint64 a_Len) override;


private:

	/** The size of the internal audio buffer.
	The actual max data amount is one byte less. */
	static const size_t AUDIO_BUFFER_SIZE = 512 * 1024;

	/** Mutex used to prevent multithreaded access to the internal buffer description. */
	QMutex m_Mtx;

	/** The wait condition used together with m_Mtx for signalling incoming decoded data. */
	QWaitCondition m_CVHasData;

	/** The wait condition used together with m_Mtx for signalling newly freed buffer space. */
	QWaitCondition m_CVHasFreeSpace;

	/** The internal audio buffer.
	The part from m_CurrentReadPos until m_CurrentWritePos - 1 is valid decoded data.
	The part from m_CurrentWritePos until m_CurrentReadPos - 1 is free space for writing.
	If m_CurrentReadPos == m_CurrentReadPos, then the buffer is empty.
	Protected against multithreaded access by m_Mtx. */
	char m_Buffer[AUDIO_BUFFER_SIZE];

	/** Position in m_Buffer where the next read operation will take place.
	Protected against multithreaded access by m_Mtx. */
	size_t m_CurrentReadPos;

	/** Position in m_Buffer where the next write operation will take place.
	Protected against multithreaded access by m_Mtx. */
	size_t m_CurrentWritePos;


	/** Writes a single block of data that is guaranteed to fit into the current free space.
	Assumes m_Mtx is locked and there is enough free space for the write operation.
	Wraps the write around the ringbuffer end. */
	void singleWrite(const void * a_Data, size_t a_Len);

	/** Returns the number of bytes that can be written currently.
	Assumes m_Mtx is locked. */
	size_t numAvailWrite();

	/** Returns the number of bytes that can be read currently.
	Assumes m_Mtx is locked. */
	size_t numAvailRead();
};

#endif // PLAYBACKBUFFER_H

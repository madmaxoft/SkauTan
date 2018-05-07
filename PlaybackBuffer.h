#ifndef PLAYBACKBUFFER_H
#define PLAYBACKBUFFER_H





#include <atomic>

#include <QIODevice>
#include <QAudioFormat>
#include <QMutex>
#include <QWaitCondition>

#include "AudioDataSource.h"





/** Buffer for the decoded audio data waiting to be sent to the audio output.
The audio decoder uses the writeDecodedAudio() function to add more data (possibly blocking).
The audio output uses the AudioDataSource interface to read the output data.
The audio decoder is expected to provide a class derived from this in the IPlaylistItem->startPlaying() function. */
class PlaybackBuffer:
	public AudioDataSource
{
public:
	PlaybackBuffer(const QAudioFormat & a_OutputFormat);

	// AudioDataSource overrides:
	virtual void abort() override;
	virtual bool shouldAbort() const override { return m_ShouldAbort.load(); }
	virtual void fadeOut(int a_Msec) override { Q_UNUSED(a_Msec); }
	virtual const QAudioFormat & format() const override { return m_OutputFormat; }
	virtual double currentSongPosition() const override;
	virtual void seekTo(double a_Time) override;
	virtual void clear() override {}  // Ignored
	virtual bool waitForData() override;
	virtual size_t read(void * a_Dest, size_t a_Len) override;
	virtual void setTempo(double a_Tempo) override { Q_UNUSED(a_Tempo); }

	/** Writes the specified data to the buffer.
	Blocks until all the data can fit into the buffer.
	Wakes up the output from its initial pause (waitForData()), if needed.
	Returns true if the decoder should continue, false if it should abort. */
	bool writeDecodedAudio(const void * a_Data, size_t a_Len);

	/** Sets the buffer duration, in seconds.
	Allocates the audio buffer. */
	void setDuration(double a_DurationSec);

	/** Returns the whole internal buffer for audio data from the decoder.
	Note that this may be accessed from other threads only after the duration has been set. */
	const std::vector<char> & audioData() const { return m_AudioData; }

	/** Returns the current read position within the buffer.
	Used by WaveformDisplay to draw the current position indicator. */
	size_t readPos() const { return m_ReadPos; }

	/** Returns the current write position within the buffer.
	Used by the WaveformDisplay to know where to finish rendering. */
	size_t writePos() const { return m_WritePos; }

	/** Returns the total size of the audio data buffer.
	Used by the WaveformDisplay to know how many samples to process in total. */
	size_t bufferLimit() const { return m_BufferLimit; }

	bool isDataComplete() const { return (m_WritePos.load() == m_BufferLimit.load()); }


protected:

	/** The audio format of the output data. */
	QAudioFormat m_OutputFormat;

	/** Indicates that there is no more data coming in from the decoder.
	Marks the audio data as complete. */
	void decodedEOF();


private:

	/** The mutex protecting all the variables against multithreaded access. */
	QMutex m_Mtx;

	/** A condition variable that can be waited upon and is set when the decoder produces the first data. */
	QWaitCondition m_CVHasData;

	/** The audio data coming from the decoder, the whole song. */
	std::vector<char> m_AudioData;

	/** Position in m_AudioData where the next write operation should start. */
	std::atomic<size_t> m_WritePos;

	/** Position in m_AudioData where the next read operation should start. */
	std::atomic<size_t> m_ReadPos;

	/** The maximum data position in m_AudioData.
	Normally set to m_AudioData size, but if the decoder encounters an early EOF, it will lower the limit
	to the current write position. */
	std::atomic<size_t> m_BufferLimit;

	/** A flag specifying if the object should abort its work. */
	std::atomic<bool> m_ShouldAbort;

	/** A flag that specifies that an error has occured while decoding. */
	bool m_HasError;
};

using PlaybackBufferPtr = std::shared_ptr<PlaybackBuffer>;

Q_DECLARE_METATYPE(PlaybackBufferPtr);





#endif // PLAYBACKBUFFER_H

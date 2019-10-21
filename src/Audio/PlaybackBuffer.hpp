#ifndef PLAYBACKBUFFER_H
#define PLAYBACKBUFFER_H





#include <atomic>

#include <QIODevice>
#include <QAudioFormat>
#include <QMutex>
#include <QWaitCondition>

#include "AudioDataSource.hpp"





/** Buffer for the decoded audio data waiting to be sent to the audio output.
The audio decoder uses the writeDecodedAudio() function to add more data (possibly blocking).
The audio output uses the AudioDataSource interface to read the output data.
The audio decoder is expected to provide a class derived from this in the IPlaylistItem->startPlaying() function. */
class PlaybackBuffer:
	public AudioDataSource
{
public:
	PlaybackBuffer(const QAudioFormat & aOutputFormat);

	// AudioDataSource overrides:
	virtual void abort() override;
	virtual void abortWithError() override;
	virtual bool shouldAbort() const override { return mShouldAbort.load(); }
	virtual void fadeOut(int aMsec) override { Q_UNUSED(aMsec); }
	virtual const QAudioFormat & format() const override { return mOutputFormat; }
	virtual double currentSongPosition() const override;
	virtual double remainingTime() const override;
	virtual void seekTo(double aTime) override;
	virtual void clear() override {}  // Ignored
	virtual bool waitForData() override;
	virtual size_t read(void * aDest, size_t aLen) override;
	virtual void setTempo(double aTempo) override { Q_UNUSED(aTempo); }

	/** Writes the specified data to the buffer.
	Blocks until all the data can fit into the buffer.
	Wakes up the output from its initial pause (waitForData()), if needed.
	Returns true if the decoder should continue, false if it should abort. */
	bool writeDecodedAudio(const void * aData, size_t aLen);

	/** Sets the buffer duration, in seconds.
	Allocates the audio buffer. */
	void setDuration(double aDurationSec);

	/** Returns the whole internal buffer for audio data from the decoder.
	Note that this may be accessed from other threads only after the duration has been set. */
	const std::vector<char> & audioData() const { return mAudioData; }

	/** Returns the current read position within the buffer.
	Used by WaveformDisplay to draw the current position indicator. */
	size_t readPos() const { return mReadPos; }

	/** Returns the current write position within the buffer.
	Used by the WaveformDisplay to know where to finish rendering. */
	size_t writePos() const { return mWritePos; }

	/** Returns the total size of the audio data buffer.
	Used by the WaveformDisplay to know how many samples to process in total. */
	size_t bufferLimit() const { return mBufferLimit; }

	/** Seeks to the specified audio frame.
	Sets the read position for the next read operation to read the specified frame. */
	void seekToFrame(int aFrame);

	/** Returns the total duration of the stored audio data, in fractional seconds. */
	double duration() const;


protected:

	/** The audio format of the output data. */
	QAudioFormat mOutputFormat;

	/** Indicates that there is no more data coming in from the decoder.
	Marks the audio data as complete. */
	void decodedEOF();


private:

	/** The mutex protecting all the variables against multithreaded access. */
	QMutex mMtx;

	/** A condition variable that can be waited upon and is set when the decoder produces the first data. */
	QWaitCondition mCVHasData;

	/** The audio data coming from the decoder, the whole song. */
	std::vector<char> mAudioData;

	/** Position in mAudioData where the next write operation should start. */
	std::atomic<size_t> mWritePos;

	/** Position in mAudioData where the next read operation should start. */
	std::atomic<size_t> mReadPos;

	/** The maximum data position in mAudioData.
	Normally set to mAudioData size, but if the decoder encounters an early EOF, it will lower the limit
	to the current write position. */
	std::atomic<size_t> mBufferLimit;

	/** A flag specifying if the object should abort its work. */
	std::atomic<bool> mShouldAbort;

	/** A flag that specifies that an error has occured while decoding. */
	bool mHasError;

	/** Initial value of mReadPos, to be copied once the decoder provides the BufferLimit.
	Used for seeking before the decoder provides any data (skip-start). */
	size_t mReadPosInit;
};

using PlaybackBufferPtr = std::shared_ptr<PlaybackBuffer>;

Q_DECLARE_METATYPE(PlaybackBufferPtr);





#endif // PLAYBACKBUFFER_H

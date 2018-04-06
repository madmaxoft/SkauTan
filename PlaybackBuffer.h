#ifndef PLAYBACKBUFFER_H
#define PLAYBACKBUFFER_H





#include <atomic>

#include <QIODevice>
#include <QAudioFormat>

#include "RingBuffer.h"
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
	virtual bool shouldAbort() const override { return m_RingBuffer.shouldAbort(); }
	virtual void fadeOut(int a_Msec) override;
	virtual const QAudioFormat & format() const override { return m_OutputFormat; }
	virtual double currentSongPosition() const override;
	virtual void seekTo(double a_Time) override;
	virtual void clear() override;
	virtual bool waitForData() override { return m_RingBuffer.waitForData(); }
	virtual size_t read(void * a_Dest, size_t a_Len) override;

	/** Writes the specified data to the buffer.
	Blocks until all the data can fit into the buffer.
	Wakes up the output from its initial pause, if needed.
	Returns true if the decoder should continue, false if it should abort. */
	bool writeDecodedAudio(const void * a_Data, size_t a_Len);


protected:

	/** The audio format of the output data. */
	QAudioFormat m_OutputFormat;


private:

	/** The RingBuffer that stores the decoded audio data. */
	RingBuffer m_RingBuffer;

	/** The playback position, in frames.
	Updated at the time the audiodata is read by the player, so it is a bit ahead of the actual playback. */
	std::atomic<size_t> m_CurrentSongPosition;

	/** Set to true if FadeOut was requested.
	If true, data being read will get faded out until the end of FadeOut is reached, at which point no more data will
	be returned by the readData() function.
	Protected against multithreaded access by m_Mtx. */
	bool m_IsFadingOut;

	/** Total number of samples across which the FadeOut should be done.
	Only used when m_IsFadingOut is true.
	Protected against multithreaded access by m_Mtx. */
	int m_FadeOutTotalSamples;

	/** Number of samples that will be faded out, before reaching zero volume.
	Each read operation after a fadeout will decrease this numer by the number of samples read, until it reaches
	zero, at which point the readData() function will abort decoding and return no more data.
	Protected against multithreaded access by m_Mtx. */
	int m_FadeOutRemaining;


	/** Applies FadeOut, if appropriate, to the specified data, and updates the FadeOut progress.
	a_Data is the data to be returned from the read operation, it is assumed to be in complete samples. */
	void applyFadeOut(void * a_Data, size_t a_NumBytes);
};

#endif // PLAYBACKBUFFER_H

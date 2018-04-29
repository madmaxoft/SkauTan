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
	virtual void fadeOut(int a_Msec) override { Q_UNUSED(a_Msec); }
	virtual const QAudioFormat & format() const override { return m_OutputFormat; }
	virtual double currentSongPosition() const override;
	virtual void seekTo(double a_Time) override;
	virtual void clear() override;
	virtual bool waitForData() override { return m_RingBuffer.waitForData(); }
	virtual size_t read(void * a_Dest, size_t a_Len) override;
	virtual void setTempo(double a_Tempo) override { Q_UNUSED(a_Tempo); }

	/** Writes the specified data to the buffer.
	Blocks until all the data can fit into the buffer.
	Wakes up the output from its initial pause (waitForData()), if needed.
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
};





#endif // PLAYBACKBUFFER_H

#include "PlaybackBuffer.h"
#include <assert.h>
#include <QDebug>





PlaybackBuffer::PlaybackBuffer(const QAudioFormat & a_OutputFormat):
	m_OutputFormat(a_OutputFormat),
	m_RingBuffer(512 * 1024),
	m_CurrentSongPosition(0),
	m_IsFadingOut(false),
	m_FadeOutTotalSamples(0),
	m_FadeOutRemaining(0)
{
}





void PlaybackBuffer::abort()
{
	qDebug() << __FUNCTION__ << ": Aborting playback buffer processing.";
	m_RingBuffer.abort();
	qDebug() << __FUNCTION__ << ": Playback buffer abort signalled.";
}





void PlaybackBuffer::fadeOut(int a_Msec)
{
	m_FadeOutTotalSamples = m_OutputFormat.channelCount() * a_Msec * m_OutputFormat.sampleRate() / 1000;
	m_FadeOutRemaining = m_FadeOutTotalSamples;
	m_IsFadingOut = true;
}





bool PlaybackBuffer::writeDecodedAudio(const void * a_Data, size_t a_Len)
{
	auto numBytesWritten = m_RingBuffer.writeData(reinterpret_cast<const char *>(a_Data), a_Len);
	if (numBytesWritten != a_Len)
	{
		qDebug() << __FUNCTION__ << ": Aborted.";
		return false;
	}
	return true;
}





double PlaybackBuffer::currentSongPosition() const
{
	return static_cast<double>(m_CurrentSongPosition) / m_OutputFormat.sampleRate();
}





void PlaybackBuffer::seekTo(double a_Time)
{
	m_CurrentSongPosition = static_cast<size_t>(a_Time * m_OutputFormat.sampleRate());
}





void PlaybackBuffer::clear()
{
	m_RingBuffer.clear();
}





size_t PlaybackBuffer::read(void * a_Data, size_t a_MaxLen)
{
	assert(a_MaxLen >= 0);
	if (m_IsFadingOut && (m_FadeOutRemaining <= 0))
	{
		qDebug() << __FUNCTION__ << ": Faded out completely, signaling end of stream.";
		abort();
		return 0;
	}
	auto numBytesToRead = static_cast<size_t>(a_MaxLen & ~0x03U);  // Only read in multiples of 4 bytes
	auto numBytesRead = m_RingBuffer.readData(a_Data, numBytesToRead);
	applyFadeOut(a_Data, numBytesRead);
	return numBytesRead;
}





void PlaybackBuffer::applyFadeOut(void * a_Data, size_t a_NumBytes)
{
	assert(a_NumBytes % 2 == 0);  // The data comes in complete samples

	if (!m_IsFadingOut)
	{
		return;
	}
	if (m_FadeOutRemaining <= 0)
	{
		// Reached the end of fadeout, just zero out all remaining data
		qDebug() << __FUNCTION__ << ": Reached end of fadeout.";
		memset(a_Data, 0, static_cast<size_t>(a_NumBytes));
		return;
	}
	auto numSamples = a_NumBytes / 2;
	auto samples = reinterpret_cast<int16_t *>(a_Data);
	// Despite being technically incorrect, we can get away with processing the whole datastream as a single channel
	// The difference is so miniscule that it is unimportant
	for (size_t s = 0; s < numSamples; ++s)
	{
		samples[s] = static_cast<int16_t>(static_cast<int>(samples[s]) * m_FadeOutRemaining / m_FadeOutTotalSamples);
		m_FadeOutRemaining -= 1;
		if (m_FadeOutRemaining <= 0)
		{
			qDebug() << __FUNCTION__ << ": Reached end of fadeout, zeroing out the rest.";
			for (size_t s2 = s + 1; s2 < numSamples; ++s2)
			{
				samples[s2] = 0;
			}
			return;
		}
	}
}

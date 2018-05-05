#include "PlaybackBuffer.h"
#include <assert.h>
#include <QDebug>





PlaybackBuffer::PlaybackBuffer(const QAudioFormat & a_OutputFormat):
	m_OutputFormat(a_OutputFormat),
	m_RingBuffer(512 * 1024),
	m_CurrentSongPosition(0)
{
}





void PlaybackBuffer::abort()
{
	qDebug() << ": Aborting playback buffer processing.";
	m_RingBuffer.abort();
	qDebug() << ": Playback buffer abort signalled.";
}





bool PlaybackBuffer::writeDecodedAudio(const void * a_Data, size_t a_Len)
{
	auto numBytesWritten = m_RingBuffer.writeData(reinterpret_cast<const char *>(a_Data), a_Len);
	if (numBytesWritten != a_Len)
	{
		qDebug() << ": Aborted.";
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
	auto numBytesToRead = a_MaxLen & ~0x03u;  // Only read in multiples of 4 bytes
	auto numBytesRead = m_RingBuffer.readData(a_Data, numBytesToRead);

	// If there's non-multiple of 4 at the end of the data, trim it off:
	if ((numBytesRead & 0x03u) != 0)
	{
		qDebug() << ": Dropping non-aligned data at the end of the buffer";
		numBytesRead = numBytesRead & ~0x03u;
	}

	// Advance the internal position:
	m_CurrentSongPosition += numBytesRead / 4;

	return numBytesRead;
}






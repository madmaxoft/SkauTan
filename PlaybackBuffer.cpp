#include "PlaybackBuffer.h"
#include <assert.h>
#include <QDebug>





PlaybackBuffer::PlaybackBuffer(const QAudioFormat & a_OutputFormat):
	m_OutputFormat(a_OutputFormat),
	m_WritePos(0),
	m_ReadPos(0),
	m_BufferLimit(0),
	m_ShouldAbort(false),
	m_HasError(false)
{
}





void PlaybackBuffer::abort()
{
	m_ShouldAbort = true;
	m_CVHasData.wakeAll();  // If anyone is stuck in waitForData(), wake them up
}





bool PlaybackBuffer::writeDecodedAudio(const void * a_Data, size_t a_Len)
{
	QMutexLocker lock(&m_Mtx);
	assert(!m_AudioData.empty());  // Has duration been set?
	auto numBytesLeft = m_AudioData.size() - m_WritePos;
	auto numBytesToWrite = std::min(a_Len, numBytesLeft);
	memcpy(m_AudioData.data() + static_cast<int>(m_WritePos), a_Data, numBytesToWrite);
	m_WritePos += numBytesToWrite;
	m_CVHasData.wakeAll();
	if (m_ShouldAbort.load())
	{
		return false;
	}
	return true;
}





void PlaybackBuffer::setDuration(double a_DurationSec)
{
	QMutexLocker lock(&m_Mtx);
	assert(m_AudioData.empty());  // Can be called only once
	m_BufferLimit = static_cast<size_t>(m_OutputFormat.bytesForDuration(static_cast<qint64>(a_DurationSec * 1000000)));
	m_AudioData.resize(m_BufferLimit);
}





void PlaybackBuffer::seekToFrame(int a_Frame)
{
	assert(a_Frame >= 0);
	m_ReadPos = std::min(static_cast<size_t>(a_Frame * m_OutputFormat.bytesPerFrame()), m_BufferLimit.load());
}





void PlaybackBuffer::decodedEOF()
{
	m_BufferLimit = m_WritePos.load();
}





double PlaybackBuffer::currentSongPosition() const
{
	return static_cast<double>(m_OutputFormat.durationForBytes(static_cast<qint32>(m_ReadPos))) / 1000000;
}





double PlaybackBuffer::remainingTime() const
{
	auto numBytesLeft = static_cast<qint32>(m_BufferLimit - m_ReadPos);
	return static_cast<double>(m_OutputFormat.durationForBytes(numBytesLeft)) / 1000000;
}





void PlaybackBuffer::seekTo(double a_Time)
{
	m_ReadPos = static_cast<size_t>(m_OutputFormat.bytesForDuration(static_cast<qint64>(a_Time * 1000000)));
}





bool PlaybackBuffer::waitForData()
{
	QMutexLocker lock(&m_Mtx);
	while ((m_WritePos == 0) && !m_HasError && !m_ShouldAbort)
	{
		m_CVHasData.wait(&m_Mtx);
	}
	return !m_HasError;
}





size_t PlaybackBuffer::read(void * a_Data, size_t a_MaxLen)
{
	QMutexLocker lock(&m_Mtx);
	assert(m_ReadPos <= m_AudioData.size());
	auto numBytesLeft = m_AudioData.size() - m_ReadPos;
	auto numBytesToRead = a_MaxLen & ~0x03u;  // Only read in multiples of 4 bytes
	numBytesToRead = std::min(numBytesLeft, numBytesToRead);
	memcpy(a_Data, m_AudioData.data() + m_ReadPos.load(), numBytesToRead);
	m_ReadPos += numBytesToRead;
	return numBytesToRead;
}






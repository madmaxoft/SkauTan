#include "PlaybackBuffer.h"
#include <assert.h>
#include <QDebug>





PlaybackBuffer::PlaybackBuffer(const QAudioFormat & a_OutputFormat):
	m_OutputFormat(a_OutputFormat),
	m_ShouldAbort(false),
	m_Mtx(QMutex::NonRecursive),
	m_CurrentReadPos(0),
	m_CurrentWritePos(0)
{
	open(QIODevice::ReadOnly);
}





void PlaybackBuffer::abort()
{
	qDebug() << __FUNCTION__ << ": Aborting playback buffer processing.";
	m_ShouldAbort = true;
	m_CVHasData.wakeAll();
	m_CVHasFreeSpace.wakeAll();
	qDebug() << __FUNCTION__ << ": Playback buffer abort signalled.";
}





bool PlaybackBuffer::waitForData()
{
	QMutexLocker lock(&m_Mtx);
	while (m_CurrentReadPos == m_CurrentWritePos)
	{
		m_CVHasData.wait(&m_Mtx);
		if (m_ShouldAbort)
		{
			qDebug() << __FUNCTION__ << ": Aborted.";
			return false;
		}
	}
	return true;
}





bool PlaybackBuffer::writeDecodedAudio(const void * a_Data, size_t a_Len)
{
	auto data = reinterpret_cast<const char *>(a_Data);
	QMutexLocker lock(&m_Mtx);
	while (a_Len > 0)
	{
		while (numAvailWrite() == 0)
		{
			m_CVHasFreeSpace.wait(&m_Mtx);
			if (m_ShouldAbort)
			{
				qDebug() << __FUNCTION__ << ": Aborted.";
				return false;
			}
		}
		auto maxWrite = std::min(numAvailWrite(), a_Len);
		singleWrite(data, maxWrite);
		data += maxWrite;
		a_Len -= maxWrite;
	}
	m_CVHasData.wakeAll();
	return true;
}





qint64 PlaybackBuffer::readData(char * a_Data, qint64 a_MaxLen)
{
	assert(a_MaxLen >= 0);
	QMutexLocker lock(&m_Mtx);
	auto numBytesToRead = std::min(static_cast<size_t>(a_MaxLen), numAvailRead());
	size_t numBytesRead = 0;
	if (m_CurrentWritePos < m_CurrentReadPos)
	{
		// The data spans across the ringbuffer end, copy the part until the boundary:
		auto tillEnd = sizeof(m_Buffer) - m_CurrentReadPos;
		auto numToCopy = std::min(tillEnd, numBytesToRead);
		memcpy(a_Data, m_Buffer + m_CurrentReadPos, numToCopy);
		a_Data += numToCopy;
		numBytesToRead -= numToCopy;
		numBytesRead = numToCopy;
		m_CurrentReadPos = (m_CurrentReadPos + numToCopy) % sizeof(m_Buffer);
		if (numBytesToRead == 0)
		{
			m_CVHasFreeSpace.wakeAll();
			return numToCopy;
		}
	}
	// Now the data is guaranteed to be contiguous, copy the rest:
	auto numToCopy = std::min(numBytesToRead, m_CurrentWritePos);
	if (numToCopy > 0)
	{
		memcpy(a_Data, m_Buffer + m_CurrentReadPos, numToCopy);
		m_CurrentReadPos += numToCopy;
		numBytesRead += numToCopy;
	}
	m_CVHasFreeSpace.wakeAll();
	return numBytesRead;
}





qint64 PlaybackBuffer::writeData(const char * a_Data, qint64 a_Len)
{
	Q_UNUSED(a_Data);
	Q_UNUSED(a_Len);

	assert(!"Unsupported operation");
	return -1;
}





void PlaybackBuffer::singleWrite(const void * a_Data, size_t a_Len)
{
	assert(a_Len <= numAvailWrite());
	#if (defined(_DEBUG) || !defined(_MSC_VER))  // Clang has an "assert" implementation that uses its arguments even in Release mode
		auto curFreeSpace = numAvailWrite();
		auto curReadableSpace = numAvailRead();
	#endif
	assert(sizeof(m_Buffer) >= m_CurrentWritePos);
	auto tillEnd = sizeof(m_Buffer) - m_CurrentWritePos;
	auto data = reinterpret_cast<const char *>(a_Data);
	size_t writtenBytes = 0;
	if (tillEnd <= a_Len)
	{
		// Need to wrap around the ringbuffer end
		if (tillEnd > 0)
		{
			memcpy(m_Buffer + m_CurrentWritePos, data, tillEnd);
			data += tillEnd;
			a_Len -= tillEnd;
			writtenBytes = tillEnd;
		}
		m_CurrentWritePos = 0;
	}

	// We're guaranteed that we'll fit in a single write op now
	if (a_Len > 0)
	{
		memcpy(m_Buffer + m_CurrentWritePos, data, a_Len);
		m_CurrentWritePos += a_Len;
		writtenBytes += a_Len;
	}

	assert(numAvailWrite() == curFreeSpace - writtenBytes);
	assert(numAvailRead() == curReadableSpace + writtenBytes);
}




size_t PlaybackBuffer::numAvailWrite()
{
	assert(m_CurrentReadPos  < sizeof(m_Buffer));
	assert(m_CurrentWritePos < sizeof(m_Buffer));
	if (m_CurrentWritePos >= m_CurrentReadPos)
	{
		// The free space is fragmented across the ringbuffer boundary:
		assert(sizeof(m_Buffer) >= m_CurrentWritePos);
		assert((sizeof(m_Buffer) - m_CurrentWritePos + m_CurrentReadPos) >= 1);
		return sizeof(m_Buffer) - m_CurrentWritePos + m_CurrentReadPos - 1;
	}
	// The free space is contiguous:
	assert(sizeof(m_Buffer) >= m_CurrentWritePos);
	assert(sizeof(m_Buffer) - m_CurrentWritePos >= 1);
	return m_CurrentReadPos - m_CurrentWritePos - 1;
}





size_t PlaybackBuffer::numAvailRead()
{
	assert(m_CurrentReadPos  < sizeof(m_Buffer));
	assert(m_CurrentWritePos < sizeof(m_Buffer));
	if (m_CurrentReadPos > m_CurrentWritePos)
	{
		// Wrap around the buffer end:
		assert(sizeof(m_Buffer) >= m_CurrentReadPos);
		return sizeof(m_Buffer) - m_CurrentReadPos + m_CurrentWritePos;
	}
	// Single readable space partition:
	assert(m_CurrentWritePos >= m_CurrentReadPos);
	return m_CurrentWritePos - m_CurrentReadPos;
}

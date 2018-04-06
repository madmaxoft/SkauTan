#include "RingBuffer.h"
#include <assert.h>
#include <QDebug>
#include "Stopwatch.h"





RingBuffer::RingBuffer(size_t a_Size):
	m_Mtx(QMutex::NonRecursive),
	m_Buffer(new char[a_Size]),
	m_CurrentReadPos(0),
	m_CurrentWritePos(0),
	m_ShouldAbort(false)
{
}





RingBuffer::~RingBuffer()
{
	delete[] m_Buffer;
}





size_t RingBuffer::readData(void * a_Dest, size_t a_MaxLen)
{
	// STOPWATCH;
	auto numBytesToRead = a_MaxLen;
	auto dest = reinterpret_cast<char *>(a_Dest);
	size_t numBytesRead = 0;
	QMutexLocker lock(&m_Mtx);
	while (numBytesToRead > 0)
	{
		// Wait until there's some data in the buffer:
		while (lockedAvailRead() == 0)
		{
			m_CVHasData.wait(&m_Mtx);
			if (m_ShouldAbort.load())
			{
				qDebug() << __FUNCTION__ << ": Read aborted, returning " << numBytesRead << " bytes";
				return numBytesRead;
			}
		}

		// Read as much data as possible:
		if (m_CurrentWritePos < m_CurrentReadPos)
		{
			// The data spans across the ringbuffer end, copy the part until the boundary:
			auto tillEnd = sizeof(m_Buffer) - m_CurrentReadPos;
			auto numToCopy = std::min(tillEnd, std::min(numBytesToRead, lockedAvailRead()));
			memcpy(dest, m_Buffer + m_CurrentReadPos, numToCopy);
			dest += numToCopy;
			numBytesToRead -= numToCopy;
			numBytesRead += numToCopy;
			m_CurrentReadPos = (m_CurrentReadPos + numToCopy) % sizeof(m_Buffer);
		}
		else
		{
			// The data is contiguous in the buffer, copy as much as possible:
			auto numToCopy = std::min(numBytesToRead, lockedAvailRead());
			memcpy(dest, m_Buffer + m_CurrentReadPos, numToCopy);
			dest += numToCopy;
			numBytesToRead -= numToCopy;
			numBytesRead += numToCopy;
			m_CurrentReadPos += numToCopy;
		}
		m_CVHasFreeSpace.wakeAll();
	}
	return numBytesRead;
}





size_t RingBuffer::writeData(const char * a_Data, size_t a_Len)
{
	// STOPWATCH;
	auto data = a_Data;
	size_t numWritten = 0;
	QMutexLocker lock(&m_Mtx);
	while (a_Len > 0)
	{
		while (lockedAvailWrite() == 0)
		{
			m_CVHasFreeSpace.wait(&m_Mtx);
			if (m_ShouldAbort)
			{
				qDebug() << __FUNCTION__ << ": Write aborted, written " << (data - a_Data) << " bytes";
				return static_cast<size_t>(data - a_Data);
			}
		}
		auto maxWrite = std::min(lockedAvailWrite(), a_Len);
		singleWrite(data, maxWrite);
		data += maxWrite;
		a_Len -= maxWrite;
		numWritten += maxWrite;
		m_CVHasData.wakeAll();
	}
	return numWritten;
}





void RingBuffer::abort()
{
	m_ShouldAbort = true;
	m_CVHasData.wakeAll();
	m_CVHasFreeSpace.wakeAll();
}





bool RingBuffer::waitForData()
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





void RingBuffer::clear()
{
	QMutexLocker lock(&m_Mtx);
	qDebug() << "Clearing buffer, current availRead = " << numAvailRead() << ", numAvailWrite = " << numAvailWrite();
	assert(lockedAvailRead() % 4 == 0);  // Needs to be frame-aligned
	m_CurrentReadPos = 0;
	m_CurrentWritePos = 0;
	assert(lockedAvailRead() == 0);
	m_CVHasFreeSpace.notify_one();
}





size_t RingBuffer::numAvailWrite()
{
	QMutexLocker lock(&m_Mtx);
	return lockedAvailWrite();
}





size_t RingBuffer::numAvailRead()
{
	QMutexLocker lock(&m_Mtx);
	return lockedAvailRead();
}





void RingBuffer::singleWrite(const void * a_Data, size_t a_Len)
{
	assert(a_Len <= lockedAvailWrite());
	#if (defined(_DEBUG) || !defined(_MSC_VER))  // Unlike MSVC, Clang has an "assert" implementation that uses its arguments even in Release mode
		auto curFreeSpace = lockedAvailWrite();
		auto curReadableSpace = lockedAvailRead();
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

	assert(lockedAvailWrite() == curFreeSpace - writtenBytes);
	assert(lockedAvailRead() == curReadableSpace + writtenBytes);
}





size_t RingBuffer::lockedAvailWrite()
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





size_t RingBuffer::lockedAvailRead()
{
	assert(m_CurrentReadPos  < sizeof(m_Buffer));
	assert(m_CurrentWritePos < sizeof(m_Buffer));
	if (m_CurrentReadPos == m_CurrentWritePos)
	{
		return 0;
	}
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

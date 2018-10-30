#include "RingBuffer.hpp"
#include <cassert>
#include <QDebug>





RingBuffer::RingBuffer(size_t a_Size):
	m_Mtx(QMutex::NonRecursive),
	m_Buffer(new char[a_Size]),
	m_BufferSize(a_Size),
	m_CurrentReadPos(0),
	m_CurrentWritePos(0),
	m_ShouldAbort(false),
	m_IsEOF(false)
{
}





RingBuffer::~RingBuffer()
{
	delete[] m_Buffer;
}





size_t RingBuffer::readData(void * a_Dest, size_t a_MaxLen)
{
	auto numBytesToRead = a_MaxLen;
	auto dest = reinterpret_cast<char *>(a_Dest);
	size_t numBytesRead = 0;
	QMutexLocker lock(&m_Mtx);
	while (numBytesToRead > 0)
	{
		// Wait until there's some data in the buffer:
		while (lockedAvailRead() == 0)
		{
			if (m_IsEOF.load())
			{
				qDebug() << "Reached EOF while reading.";
				return numBytesRead;
			}
			m_CVHasData.wait(&m_Mtx);
			if (m_ShouldAbort.load())
			{
				qDebug() << "Read aborted, returning " << numBytesRead << " bytes";
				return numBytesRead;
			}
		}

		// Read as much data as possible:
		if (m_CurrentWritePos < m_CurrentReadPos)
		{
			// The data spans across the ringbuffer end, copy the part until the boundary:
			auto tillEnd = m_BufferSize - m_CurrentReadPos;
			auto numToCopy = std::min(tillEnd, std::min(numBytesToRead, lockedAvailRead()));
			memcpy(dest, m_Buffer + m_CurrentReadPos, numToCopy);
			dest += numToCopy;
			numBytesToRead -= numToCopy;
			numBytesRead += numToCopy;
			m_CurrentReadPos = (m_CurrentReadPos + numToCopy) % m_BufferSize;
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
				qDebug() << "Write aborted, written " << (data - a_Data) << " bytes";
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





void RingBuffer::writeEOF()
{
	m_IsEOF = true;
	m_CVHasData.wakeAll();
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
			qDebug() << "Aborted.";
			return false;
		}
	}
	return true;
}





void RingBuffer::clear()
{
	QMutexLocker lock(&m_Mtx);
	qDebug() << "Clearing buffer, current availRead = " << lockedAvailRead() << ", numAvailWrite = " << lockedAvailWrite();
	assert(lockedAvailRead() % 4 == 0);  // Needs to be frame-aligned
	m_CurrentReadPos = 0;
	m_CurrentWritePos = 0;
	assert(lockedAvailRead() == 0);
	m_CVHasFreeSpace.wakeAll();
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
	assert(m_BufferSize >= m_CurrentWritePos);
	auto tillEnd = m_BufferSize - m_CurrentWritePos;
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
	assert(m_CurrentReadPos  < m_BufferSize);
	assert(m_CurrentWritePos < m_BufferSize);
	if (m_CurrentWritePos >= m_CurrentReadPos)
	{
		// The free space is fragmented across the ringbuffer boundary:
		assert(m_BufferSize >= m_CurrentWritePos);
		assert((m_BufferSize - m_CurrentWritePos + m_CurrentReadPos) >= 1);
		return m_BufferSize - m_CurrentWritePos + m_CurrentReadPos - 1;
	}
	// The free space is contiguous:
	assert(m_BufferSize >= m_CurrentWritePos);
	assert(m_BufferSize - m_CurrentWritePos >= 1);
	return m_CurrentReadPos - m_CurrentWritePos - 1;
}





size_t RingBuffer::lockedAvailRead()
{
	assert(m_CurrentReadPos  < m_BufferSize);
	assert(m_CurrentWritePos < m_BufferSize);
	if (m_CurrentReadPos == m_CurrentWritePos)
	{
		return 0;
	}
	if (m_CurrentReadPos > m_CurrentWritePos)
	{
		// Wrap around the buffer end:
		assert(m_BufferSize >= m_CurrentReadPos);
		return m_BufferSize - m_CurrentReadPos + m_CurrentWritePos;
	}
	// Single readable space partition:
	assert(m_CurrentWritePos >= m_CurrentReadPos);
	return m_CurrentWritePos - m_CurrentReadPos;
}

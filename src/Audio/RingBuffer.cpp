#include "RingBuffer.hpp"
#include <cassert>
#include <QDebug>





RingBuffer::RingBuffer(size_t aSize):
	mMtx(QMutex::NonRecursive),
	mBuffer(new char[aSize]),
	mBufferSize(aSize),
	mCurrentReadPos(0),
	mCurrentWritePos(0),
	mShouldAbort(false),
	mIsEOF(false)
{
}





RingBuffer::~RingBuffer()
{
	delete[] mBuffer;
}





size_t RingBuffer::readData(void * aDest, size_t aMaxLen)
{
	auto numBytesToRead = aMaxLen;
	auto dest = reinterpret_cast<char *>(aDest);
	size_t numBytesRead = 0;
	QMutexLocker lock(&mMtx);
	while (numBytesToRead > 0)
	{
		// Wait until there's some data in the buffer:
		while (lockedAvailRead() == 0)
		{
			if (mIsEOF.load())
			{
				qDebug() << "Reached EOF while reading.";
				return numBytesRead;
			}
			mCVHasData.wait(&mMtx);
			if (mShouldAbort.load())
			{
				qDebug() << "Read aborted, returning " << numBytesRead << " bytes";
				return numBytesRead;
			}
		}

		// Read as much data as possible:
		if (mCurrentWritePos < mCurrentReadPos)
		{
			// The data spans across the ringbuffer end, copy the part until the boundary:
			auto tillEnd = mBufferSize - mCurrentReadPos;
			auto numToCopy = std::min(tillEnd, std::min(numBytesToRead, lockedAvailRead()));
			memcpy(dest, mBuffer + mCurrentReadPos, numToCopy);
			dest += numToCopy;
			numBytesToRead -= numToCopy;
			numBytesRead += numToCopy;
			mCurrentReadPos = (mCurrentReadPos + numToCopy) % mBufferSize;
		}
		else
		{
			// The data is contiguous in the buffer, copy as much as possible:
			auto numToCopy = std::min(numBytesToRead, lockedAvailRead());
			memcpy(dest, mBuffer + mCurrentReadPos, numToCopy);
			dest += numToCopy;
			numBytesToRead -= numToCopy;
			numBytesRead += numToCopy;
			mCurrentReadPos += numToCopy;
		}
		mCVHasFreeSpace.wakeAll();
	}
	return numBytesRead;
}





size_t RingBuffer::writeData(const char * aData, size_t aLen)
{
	auto data = aData;
	size_t numWritten = 0;
	QMutexLocker lock(&mMtx);
	while (aLen > 0)
	{
		while (lockedAvailWrite() == 0)
		{
			mCVHasFreeSpace.wait(&mMtx);
			if (mShouldAbort)
			{
				qDebug() << "Write aborted, written " << (data - aData) << " bytes";
				return static_cast<size_t>(data - aData);
			}
		}
		auto maxWrite = std::min(lockedAvailWrite(), aLen);
		singleWrite(data, maxWrite);
		data += maxWrite;
		aLen -= maxWrite;
		numWritten += maxWrite;
		mCVHasData.wakeAll();
	}
	return numWritten;
}





void RingBuffer::writeEOF()
{
	mIsEOF = true;
	mCVHasData.wakeAll();
}





void RingBuffer::abort()
{
	mShouldAbort = true;
	mCVHasData.wakeAll();
	mCVHasFreeSpace.wakeAll();
}





bool RingBuffer::waitForData()
{
	QMutexLocker lock(&mMtx);
	while (mCurrentReadPos == mCurrentWritePos)
	{
		mCVHasData.wait(&mMtx);
		if (mShouldAbort)
		{
			qDebug() << "Aborted.";
			return false;
		}
	}
	return true;
}





void RingBuffer::clear()
{
	QMutexLocker lock(&mMtx);
	qDebug() << "Clearing buffer, current availRead = " << lockedAvailRead() << ", numAvailWrite = " << lockedAvailWrite();
	assert(lockedAvailRead() % 4 == 0);  // Needs to be frame-aligned
	mCurrentReadPos = 0;
	mCurrentWritePos = 0;
	assert(lockedAvailRead() == 0);
	mCVHasFreeSpace.wakeAll();
}





size_t RingBuffer::numAvailWrite()
{
	QMutexLocker lock(&mMtx);
	return lockedAvailWrite();
}





size_t RingBuffer::numAvailRead()
{
	QMutexLocker lock(&mMtx);
	return lockedAvailRead();
}





void RingBuffer::singleWrite(const void * aData, size_t aLen)
{
	assert(aLen <= lockedAvailWrite());
	#if (defined(_DEBUG) || !defined(_MSC_VER))  // Unlike MSVC, Clang has an "assert" implementation that uses its arguments even in Release mode
		auto curFreeSpace = lockedAvailWrite();
		auto curReadableSpace = lockedAvailRead();
	#endif
	assert(mBufferSize >= mCurrentWritePos);
	auto tillEnd = mBufferSize - mCurrentWritePos;
	auto data = reinterpret_cast<const char *>(aData);
	size_t writtenBytes = 0;
	if (tillEnd <= aLen)
	{
		// Need to wrap around the ringbuffer end
		if (tillEnd > 0)
		{
			memcpy(mBuffer + mCurrentWritePos, data, tillEnd);
			data += tillEnd;
			aLen -= tillEnd;
			writtenBytes = tillEnd;
		}
		mCurrentWritePos = 0;
	}

	// We're guaranteed that we'll fit in a single write op now
	if (aLen > 0)
	{
		memcpy(mBuffer + mCurrentWritePos, data, aLen);
		mCurrentWritePos += aLen;
		writtenBytes += aLen;
	}

	assert(lockedAvailWrite() == curFreeSpace - writtenBytes);
	assert(lockedAvailRead() == curReadableSpace + writtenBytes);
}





size_t RingBuffer::lockedAvailWrite()
{
	assert(mCurrentReadPos  < mBufferSize);
	assert(mCurrentWritePos < mBufferSize);
	if (mCurrentWritePos >= mCurrentReadPos)
	{
		// The free space is fragmented across the ringbuffer boundary:
		assert(mBufferSize >= mCurrentWritePos);
		assert((mBufferSize - mCurrentWritePos + mCurrentReadPos) >= 1);
		return mBufferSize - mCurrentWritePos + mCurrentReadPos - 1;
	}
	// The free space is contiguous:
	assert(mBufferSize >= mCurrentWritePos);
	assert(mBufferSize - mCurrentWritePos >= 1);
	return mCurrentReadPos - mCurrentWritePos - 1;
}





size_t RingBuffer::lockedAvailRead()
{
	assert(mCurrentReadPos  < mBufferSize);
	assert(mCurrentWritePos < mBufferSize);
	if (mCurrentReadPos == mCurrentWritePos)
	{
		return 0;
	}
	if (mCurrentReadPos > mCurrentWritePos)
	{
		// Wrap around the buffer end:
		assert(mBufferSize >= mCurrentReadPos);
		return mBufferSize - mCurrentReadPos + mCurrentWritePos;
	}
	// Single readable space partition:
	assert(mCurrentWritePos >= mCurrentReadPos);
	return mCurrentWritePos - mCurrentReadPos;
}

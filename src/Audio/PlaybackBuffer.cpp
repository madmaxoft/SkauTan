#include "PlaybackBuffer.hpp"
#include <cassert>
#include <QDebug>





PlaybackBuffer::PlaybackBuffer(const QAudioFormat & aOutputFormat):
	mOutputFormat(aOutputFormat),
	mWritePos(0),
	mReadPos(0),
	mBufferLimit(0),
	mShouldAbort(false),
	mHasError(false),
	mReadPosInit(0)
{
}





void PlaybackBuffer::abort()
{
	mShouldAbort = true;
	mCVHasData.wakeAll();  // If anyone is stuck in waitForData(), wake them up
}





void PlaybackBuffer::abortWithError()
{
	mHasError = true;
	abort();
}





bool PlaybackBuffer::writeDecodedAudio(const void * aData, size_t aLen)
{
	QMutexLocker lock(&mMtx);
	assert(!mAudioData.empty());  // Has duration been set?
	auto numBytesLeft = mAudioData.size() - mWritePos;
	auto numBytesToWrite = std::min(aLen, numBytesLeft);
	memcpy(mAudioData.data() + static_cast<int>(mWritePos), aData, numBytesToWrite);
	mWritePos += numBytesToWrite;
	if (mWritePos > mReadPos)
	{
		mCVHasData.wakeAll();
	}
	if (mShouldAbort.load())
	{
		return false;
	}
	return true;
}





void PlaybackBuffer::setDuration(double aDurationSec)
{
	QMutexLocker lock(&mMtx);
	assert(mAudioData.empty());  // Can be called only once
	mBufferLimit = static_cast<size_t>(mOutputFormat.bytesForDuration(static_cast<qint64>(aDurationSec * 1000000)));
	mAudioData.resize(mBufferLimit);
	mReadPos = std::min(mReadPosInit, mBufferLimit.load());
}





void PlaybackBuffer::seekToFrame(int aFrame)
{
	assert(aFrame >= 0);
	QMutexLocker lock(&mMtx);
	if (mBufferLimit.load() == 0)
	{
		// The decoder hasn't provided any audiodata yet
		// Instead of immediate seeking, store the new ReadPos so that it is set once the decoder kicks in:
		mReadPosInit = static_cast<size_t>(aFrame * mOutputFormat.bytesPerFrame());
		return;
	}
	mReadPos = std::min(static_cast<size_t>(aFrame * mOutputFormat.bytesPerFrame()), mBufferLimit.load());
}





double PlaybackBuffer::duration() const
{
	return static_cast<double>(mOutputFormat.durationForBytes(static_cast<qint32>(mBufferLimit))) / 1000000;
}





void PlaybackBuffer::decodedEOF()
{
	mBufferLimit = mWritePos.load();
}





double PlaybackBuffer::currentSongPosition() const
{
	return static_cast<double>(mOutputFormat.durationForBytes(static_cast<qint32>(mReadPos))) / 1000000;
}





double PlaybackBuffer::remainingTime() const
{
	auto numBytesLeft = static_cast<qint32>(mBufferLimit - mReadPos);
	return static_cast<double>(mOutputFormat.durationForBytes(numBytesLeft)) / 1000000;
}





void PlaybackBuffer::seekTo(double aTime)
{
	mReadPos = static_cast<size_t>(mOutputFormat.bytesForDuration(static_cast<qint64>(aTime * 1000000)));
}





bool PlaybackBuffer::waitForData()
{
	QMutexLocker lock(&mMtx);
	while ((mWritePos == 0) && !mHasError && !mShouldAbort)
	{
		mCVHasData.wait(&mMtx);
	}
	return !mHasError;
}





size_t PlaybackBuffer::read(void * aData, size_t aMaxLen)
{
	QMutexLocker lock(&mMtx);
	assert(mReadPos <= mAudioData.size());
	auto numBytesLeft = mAudioData.size() - mReadPos;
	auto numBytesToRead = aMaxLen & ~0x03u;  // Only read in multiples of 4 bytes
	numBytesToRead = std::min(numBytesLeft, numBytesToRead);
	memcpy(aData, mAudioData.data() + mReadPos.load(), numBytesToRead);
	mReadPos += numBytesToRead;
	return numBytesToRead;
}






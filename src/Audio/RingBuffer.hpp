#ifndef RINGBUFFER_H
#define RINGBUFFER_H






#include <atomic>
#include <QWaitCondition>
#include <QMutex>





class RingBuffer
{
public:
	RingBuffer(size_t aSize);
	~RingBuffer();
	RingBuffer(const RingBuffer &) = delete;
	RingBuffer(RingBuffer &&) = delete;

	/** Reads aMaxLen bytes from the buffer.
	Blocks until aMaxLen bytes have been read from the buffer, or the buffer is abort()-ed.
	aMaxLen may be larger than the buffer size.
	Returns the number of bytes read. */
	size_t readData(void * aDest, size_t aMaxLen);

	/** Writes the specified data into the buffer.
	Blocks until all the data is written, or the buffer is abort()-ed.
	aLen may be larger than the buffer size.
	Returns the number of bytes written. */
	size_t writeData(const char * aData, size_t aLen);

	/** Sets a flag that no more data will be written into the buffer.
	Subsequent reads will return existing data and then report EOF. */
	void writeEOF();

	/** Signals that the read / write / wait operations should abort, instead of waiting for async operations to finish.
	Used mainly by destructors. */
	void abort();

	/** Returns true iff the decoder should terminate (abort has been called). */
	bool shouldAbort() const { return mShouldAbort.load(); }

	/** Waits until data is available for reading.
	Returns true if data has arrived, false if aborted. */
	bool waitForData();

	void clear();

	/** Returns the number of bytes that can be written currently.
	Assumes mMtx is unlocked. */
	size_t numAvailWrite();

	/** Returns the number of bytes that can be read currently.
	Assumes mMtx is unlocked. */
	size_t numAvailRead();


protected:

	/** Mutex used to prevent multithreaded access to the internal buffer description. */
	QMutex mMtx;

	/** The wait condition used together with mMtx for signalling incoming decoded data. */
	QWaitCondition mCVHasData;

	/** The wait condition used together with mMtx for signalling newly freed buffer space. */
	QWaitCondition mCVHasFreeSpace;

	/** The internal buffer.
	The part from mCurrentReadPos until mCurrentWritePos - 1 is valid data available for reading.
	The part from mCurrentWritePos until mCurrentReadPos - 1 is free space for writing.
	If mCurrentReadPos == mCurrentReadPos, then the buffer is empty.
	Protected against multithreaded access by mMtx. */
	char * mBuffer;

	/** Size of mBuffer, in bytes. */
	const size_t mBufferSize;

	/** Position in mBuffer where the next read operation will take place.
	Protected against multithreaded access by mMtx. */
	size_t mCurrentReadPos;

	/** Position in mBuffer where the next write operation will take place.
	Protected against multithreaded access by mMtx. */
	size_t mCurrentWritePos;

	/** If set to true, operations on this instance abort without waiting. */
	std::atomic<bool> mShouldAbort;

	/** If true, there's no more incoming data
	Subsequent reads should only return any remaining data in the buffer and then report EOF. */
	std::atomic<bool> mIsEOF;


	/** Writes a single block of data that is guaranteed to fit into the current free space.
	Assumes mMtx is locked and there is enough free space for the write operation.
	Wraps the write around the ringbuffer end. */
	void singleWrite(const void * aData, size_t aLen);

	/** Returns the number of bytes that can be written currently.
	Assumes mMtx is locked. */
	size_t lockedAvailWrite();

	/** Returns the number of bytes that can be read currently.
	Assumes mMtx is locked. */
	size_t lockedAvailRead();
};





#endif // RINGBUFFER_H

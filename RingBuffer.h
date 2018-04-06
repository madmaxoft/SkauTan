#ifndef RINGBUFFER_H
#define RINGBUFFER_H






#include <QWaitCondition>
#include <QMutex>





class RingBuffer
{
public:
	RingBuffer(size_t a_Size);
	~RingBuffer();
	RingBuffer(const RingBuffer &) = delete;
	RingBuffer(RingBuffer &&) = delete;

	/** Reads a_MaxLen bytes from the buffer.
	Blocks until a_MaxLen bytes have been read from the buffer, or the buffer is abort()-ed.
	a_MaxLen may be larger than the buffer size.
	Returns the number of bytes read. */
	size_t readData(void * a_Dest, size_t a_MaxLen);

	/** Writes the specified data into the buffer.
	Blocks until all the data is written, or the buffer is abort()-ed.
	a_Len may be larger than the buffer size.
	Returns the number of bytes written. */
	size_t writeData(const char * a_Data, size_t a_Len);

	/** Signals that the read / write / wait operations should abort, instead of waiting for async operations to finish.
	Used mainly by destructors. */
	void abort();

	/** Returns true iff the decoder should terminate (abort has been called). */
	bool shouldAbort() const { return m_ShouldAbort.load(); }

	/** Waits until data is available for reading.
	Returns true if data has arrived, false if aborted. */
	bool waitForData();

	void clear();

	/** Returns the number of bytes that can be written currently.
	Assumes m_Mtx is unlocked. */
	size_t numAvailWrite();

	/** Returns the number of bytes that can be read currently.
	Assumes m_Mtx is unlocked. */
	size_t numAvailRead();


protected:

	/** Mutex used to prevent multithreaded access to the internal buffer description. */
	QMutex m_Mtx;

	/** The wait condition used together with m_Mtx for signalling incoming decoded data. */
	QWaitCondition m_CVHasData;

	/** The wait condition used together with m_Mtx for signalling newly freed buffer space. */
	QWaitCondition m_CVHasFreeSpace;

	/** The internal buffer.
	The part from m_CurrentReadPos until m_CurrentWritePos - 1 is valid data available for reading.
	The part from m_CurrentWritePos until m_CurrentReadPos - 1 is free space for writing.
	If m_CurrentReadPos == m_CurrentReadPos, then the buffer is empty.
	Protected against multithreaded access by m_Mtx. */
	char * m_Buffer;

	/** Size of m_Buffer, in bytes. */
	const size_t m_BufferSize;

	/** Position in m_Buffer where the next read operation will take place.
	Protected against multithreaded access by m_Mtx. */
	size_t m_CurrentReadPos;

	/** Position in m_Buffer where the next write operation will take place.
	Protected against multithreaded access by m_Mtx. */
	size_t m_CurrentWritePos;

	/** If set to true, operations on this instance abort without waiting. */
	std::atomic<bool> m_ShouldAbort;


	/** Writes a single block of data that is guaranteed to fit into the current free space.
	Assumes m_Mtx is locked and there is enough free space for the write operation.
	Wraps the write around the ringbuffer end. */
	void singleWrite(const void * a_Data, size_t a_Len);

	/** Returns the number of bytes that can be written currently.
	Assumes m_Mtx is locked. */
	size_t lockedAvailWrite();

	/** Returns the number of bytes that can be read currently.
	Assumes m_Mtx is locked. */
	size_t lockedAvailRead();
};





#endif // RINGBUFFER_H

#ifndef AUDIODATASOURCE_H
#define AUDIODATASOURCE_H





#include <assert.h>
#include <memory>
#include <QIODevice>
#include <QDebug>
#include "Stopwatch.h"





// fwd:
class QAudioFormat;





/** Base for classes that can provide audio data.
Descendants include audio decoder, effects etc. */
class AudioDataSource
{
public:
	// Force a virtual destructor in descendants
	virtual ~AudioDataSource() {}

	/** Reads the audio data of the specified length into the provided destination.
	Blocks until all data read, or aborted(). Decoding past the source end automatically aborts.
	Returns the number of bytes read. */
	virtual size_t read(void * a_Dest, size_t a_MaxLen) = 0;

	/** Seeks to the specified time. */
	virtual void seekTo(double a_Seconds) = 0;

	/** Clears the audio data that may be buffered in this object.
	Used when seeking or when setting the effect params, to improve the "reaction" time. */
	virtual void clear() = 0;

	/** Signals that the operations should abort, instead of waiting for async operations to finish.
	Used mainly by destructors. */
	virtual void abort() = 0;

	/** Returns true iff the source should terminate (abort has been called). */
	virtual bool shouldAbort() const = 0;

	/** Returns the QAudioFormat produced by this source. */
	virtual const QAudioFormat & format() const = 0;

	/** Blocks until audio data is available for reading, or abort() is called.
	Returns true if data available, false if aborted. */
	virtual bool waitForData() = 0;

	/** Fades the data out and sets the termination flag, so that the output is terminated after the fadeout. */
	virtual void fadeOut(int a_Msec) = 0;

	/** Returns the position in the playback, as fractional seconds. */
	virtual double currentSongPosition() const = 0;

	/** Sets the relative tempo of the playback.
	Tempo == 1 for normal, 0.5 for 2x slowed down and 2 for 2x sped up. */
	virtual void setTempo(double a_Tempo) = 0;
};





/** Base for AudioDataSource that chains from another ("lower") source.
By default all calls are forwarded to the lower source; descendants are expected to override only methods they need. */
class AudioDataSourceChain:
	public AudioDataSource
{
public:
	AudioDataSourceChain(std::unique_ptr<AudioDataSource> && a_Lower):
		m_Lower(std::move(a_Lower))
	{
	}


	/** Creates a new chain object, takes ownership of the Lower source. */
	AudioDataSourceChain(AudioDataSource * a_Lower):
		m_Lower(a_Lower)
	{
	}


	virtual size_t read(void * a_Dest, size_t a_MaxLen) override
	{
		return m_Lower->read(a_Dest, a_MaxLen);
	}

	virtual void seekTo(double a_Seconds) override
	{
		return m_Lower->seekTo(a_Seconds);
	}

	virtual void clear() override
	{
		return m_Lower->clear();
	}

	virtual void abort() override
	{
		return m_Lower->abort();
	}

	virtual bool shouldAbort() const override
	{
		return m_Lower->shouldAbort();
	}

	virtual const QAudioFormat & format() const override
	{
		return m_Lower->format();
	}

	virtual bool waitForData() override
	{
		return m_Lower->waitForData();
	}

	virtual void fadeOut(int a_Msec) override
	{
		return m_Lower->fadeOut(a_Msec);
	}

	virtual double currentSongPosition() const override
	{
		return m_Lower->currentSongPosition();
	}

	virtual void setTempo(double a_Tempo) override
	{
		return m_Lower->setTempo(a_Tempo);
	}


protected:

	/** The "lower" source to which the calls are redirected. */
	std::unique_ptr<AudioDataSource> m_Lower;
};





/** Adapter that allows using an AudioDataSource through a QIODevice interface (as used by Qt) */
class AudioDataSourceIO:
	public QIODevice,
	public AudioDataSourceChain
{
public:
	AudioDataSourceIO(std::unique_ptr<AudioDataSource> && a_Source):
		AudioDataSourceChain(std::move(a_Source))
	{
		QIODevice::open(QIODevice::ReadOnly);
	}

	// QIODevice overrides:
	virtual qint64 readData(char * a_Data, qint64 a_MaxLen) override
	{
		assert(a_MaxLen >= 0);

		// Skip zero-length reads:
		if (a_MaxLen <= 0)
		{
			return 0;
		}

		auto res = static_cast<qint64>(AudioDataSourceChain::read(a_Data, static_cast<size_t>(a_MaxLen)));
		#ifdef _DEBUG
			static auto lastMsec = TimeSinceStart::msecElapsed();
			auto msecNow = TimeSinceStart::msecElapsed();
			qDebug() << ": Requested " << a_MaxLen << " bytes, got " << res << " bytes; since last: "
				<< msecNow - lastMsec << " msec";
			lastMsec = msecNow;
		#endif  // _DEBUG
		return res;
	}


	virtual qint64 writeData(const char * a_Data, qint64 a_Len) override
	{
		Q_UNUSED(a_Data);
		Q_UNUSED(a_Len);
		assert(!"Writing data is forbidden");  // AudioDataSource is a read-only device
		return -1;
	}
};





#endif // AUDIODATASOURCE_H

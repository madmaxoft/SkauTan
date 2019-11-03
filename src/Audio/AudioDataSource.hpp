#pragma once

#include <cassert>
#include <memory>
#include <QIODevice>
#include <QDebug>
#include "../Stopwatch.hpp"





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
	virtual size_t read(void * aDest, size_t aMaxLen) = 0;

	/** Seeks to the specified time. */
	virtual void seekTo(double aSeconds) = 0;

	/** Clears the audio data that may be buffered in this object.
	Used when seeking or when setting the effect params, to improve the "reaction" time. */
	virtual void clear() = 0;

	/** Signals that the operations should abort, instead of waiting for async operations to finish.
	Used mainly by destructors. */
	virtual void abort() = 0;

	/** Signals that the operations should abort because an error was encountered.
	Used mainly by async decoder to signal invalid data. */
	virtual void abortWithError() = 0;

	/** Returns true iff the source should terminate (abort has been called). */
	virtual bool shouldAbort() const = 0;

	/** Returns the QAudioFormat produced by this source. */
	virtual const QAudioFormat & format() const = 0;

	/** Blocks until audio data is available for reading, or abort() is called.
	Returns true if data available, false if aborted. */
	virtual bool waitForData() = 0;

	/** Fades the data out and sets the termination flag, so that the output is terminated after the fadeout. */
	virtual void fadeOut(int aMsec) = 0;

	/** Returns the position in the playback, as fractional seconds. */
	virtual double currentSongPosition() const = 0;

	/** Returns the number of seconds remaining till the end of playback. */
	virtual double remainingTime() const = 0;

	/** Sets the relative tempo of the playback.
	Tempo == 1 for normal, 0.5 for 2x slowed down and 2 for 2x sped up. */
	virtual void setTempo(double aTempo) = 0;
};

using AudioDataSourcePtr = std::shared_ptr<AudioDataSource>;

Q_DECLARE_METATYPE(AudioDataSourcePtr)





/** Base for AudioDataSource that chains from another ("lower") source.
By default all calls are forwarded to the lower source; descendants are expected to override only methods they need. */
class AudioDataSourceChain:
	public AudioDataSource
{
public:
	AudioDataSourceChain(std::shared_ptr<AudioDataSource> aLower):
		mLower(aLower)
	{
	}


	virtual size_t read(void * aDest, size_t aMaxLen) override
	{
		return mLower->read(aDest, aMaxLen);
	}

	virtual void seekTo(double aSeconds) override
	{
		return mLower->seekTo(aSeconds);
	}

	virtual void clear() override
	{
		return mLower->clear();
	}

	virtual void abort() override
	{
		return mLower->abort();
	}

	virtual void abortWithError() override
	{
		return mLower->abortWithError();
	}

	virtual bool shouldAbort() const override
	{
		return mLower->shouldAbort();
	}

	virtual const QAudioFormat & format() const override
	{
		return mLower->format();
	}

	virtual bool waitForData() override
	{
		return mLower->waitForData();
	}

	virtual void fadeOut(int aMsec) override
	{
		return mLower->fadeOut(aMsec);
	}

	virtual double currentSongPosition() const override
	{
		return mLower->currentSongPosition();
	}

	virtual double remainingTime() const override
	{
		return mLower->remainingTime();
	}

	virtual void setTempo(double aTempo) override
	{
		return mLower->setTempo(aTempo);
	}


protected:

	/** The "lower" source to which the calls are redirected. */
	std::shared_ptr<AudioDataSource> mLower;
};





/** Adapter that allows using an AudioDataSource through a QIODevice interface (as used by Qt) */
class AudioDataSourceIO:
	public QIODevice,
	public AudioDataSourceChain
{
public:
	AudioDataSourceIO(std::shared_ptr<AudioDataSource> aSource):
		AudioDataSourceChain(aSource)
	{
		QIODevice::open(QIODevice::ReadOnly);
	}

	// QIODevice overrides:
	virtual qint64 readData(char * aData, qint64 aMaxLen) override
	{
		assert(aMaxLen >= 0);

		// Skip zero-length reads:
		if (aMaxLen <= 0)
		{
			return 0;
		}

		auto res = static_cast<qint64>(AudioDataSourceChain::read(aData, static_cast<size_t>(aMaxLen)));
		/*
		#ifdef _DEBUG
			static auto lastMsec = TimeSinceStart::msecElapsed();
			auto msecNow = TimeSinceStart::msecElapsed();
			qDebug() << "Requested " << aMaxLen << " bytes, got " << res << " bytes; since last: "
				<< msecNow - lastMsec << " msec";
			lastMsec = msecNow;
		#endif  // _DEBUG
		*/
		return res;
	}


	virtual qint64 writeData(const char * aData, qint64 aLen) override
	{
		Q_UNUSED(aData);
		Q_UNUSED(aLen);
		assert(!"Writing data is forbidden");  // AudioDataSource is a read-only device
		return -1;
	}
};

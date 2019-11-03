#pragma once

#include <atomic>
#include "AudioDataSource.hpp"




// fwd:
namespace AVPP
{
	class Resampler;
};





/** Filter that fades the audio out and then terminates on given signal. */
class AudioFadeOut:
	public AudioDataSourceChain
{
	using Super = AudioDataSourceChain;

public:
	AudioFadeOut(std::shared_ptr<AudioDataSource> aLower);

	// AudioDataSource overrides:
	virtual void fadeOut(int aMsec) override;
	virtual size_t read(void * aDest, size_t aMaxLen) override;


private:

	/** Set to true if a FadeOut was requested.
	If true, data being read will get faded out until the end of FadeOut is reached, at which point no more data will
	be returned by the read() function. */
	std::atomic<bool> mIsFadingOut;

	/** Total number of samples across which the FadeOut should be done.
	Only used when mIsFadingOut is true.
	Protected against multithreaded access by mMtx. */
	qint64 mFadeOutTotalSamples;

	/** Number of samples that will be faded out, before reaching zero volume.
	Each read operation after a fadeout will decrease this numer by the number of samples read, until it reaches
	zero, at which point the readData() function will abort decoding and return no more data.
	Protected against multithreaded access by mMtx. */
	qint64 mFadeOutRemaining;


	/** Applies FadeOut, if appropriate, to the specified data, and updates the FadeOut progress.
	aData is the data to be returned from the read operation, it is assumed to be in complete samples. */
	void applyFadeOut(void * aData, size_t aNumBytes);
};





/** Filter that changes the tempo (and pitch) by resampling the audio into a different samplerate. */
class AudioTempoChange:
	public AudioDataSourceChain
{
	using Super = AudioDataSourceChain;

public:
	AudioTempoChange(std::shared_ptr<AudioDataSource> aLower);

	virtual ~AudioTempoChange() override;

	// AudioDataSource overrides:
	virtual size_t read(void * aDest, size_t aMaxLen) override;
	virtual void setTempo(double aTempo) override;
	virtual double currentSongPosition() const override;
	virtual double remainingTime() const override;


protected:

	/** The resampler that does the actual tempo change. */
	std::unique_ptr<AVPP::Resampler> mResampler;

	/** The sample rate that is requested for the conversion.
	Read in the player thread, set in the UI thread, hence the atomicity requirement. */
	std::atomic<int> mDestSampleRate;

	/** The sample rate currently used in mResampler's dst.
	If this doesn't queal to mDestSampleRate on next read, the resampler will be reinitialized. */
	int mCurrentSampleRate;
};

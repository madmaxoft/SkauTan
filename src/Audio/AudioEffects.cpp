#include "AudioEffects.hpp"
#include <QAudioFormat>
#include <QDebug>
#include "../Stopwatch.hpp"
#include "AVPP.hpp"





AudioFadeOut::AudioFadeOut(std::shared_ptr<AudioDataSource> aLower):
	Super(aLower),
	mIsFadingOut(false),
	mFadeOutTotalSamples(0),
	mFadeOutRemaining(0)
{
}





void AudioFadeOut::fadeOut(int aMsec)
{
	mFadeOutTotalSamples = format().channelCount() * aMsec * format().sampleRate() / 1000;
	mFadeOutRemaining = mFadeOutTotalSamples;
	mIsFadingOut = true;
}





size_t AudioFadeOut::read(void * aDest, size_t aMaxLen)
{
	// STOPWATCH(__FUNCTION__);

	// If the Fade-out is completed, return end-of-stream:
	if (mIsFadingOut && (mFadeOutRemaining <= 0))
	{
		abort();
		return 0;
	}

	auto numBytesToRead = aMaxLen & ~0x03u;  // Only read in multiples of 4 bytes
	auto numBytesRead = Super::read(aDest, numBytesToRead);

	// If there's non-multiple of 4 at the end of the data, trim it off:
	if ((numBytesRead & 0x03u) != 0)
	{
		numBytesRead = numBytesRead & ~0x03u;
	}

	// Fade-out:
	applyFadeOut(aDest, numBytesRead);

	return numBytesRead;
}





void AudioFadeOut::applyFadeOut(void * aData, size_t aNumBytes)
{
	Q_UNUSED(aData);

	// STOPWATCH(__FUNCTION__);
	assert(aNumBytes % 2 == 0);  // The data comes in complete samples

	if (!mIsFadingOut)
	{
		return;
	}
	if (mFadeOutRemaining <= 0)
	{
		// Reached the end of fadeout, just zero out all remaining data
		memset(aData, 0, static_cast<size_t>(aNumBytes));
		abort();
		return;
	}
	auto numSamples = aNumBytes / 2;
	auto samples = reinterpret_cast<int16_t *>(aData);
	// Despite being technically incorrect, we can get away with processing the whole datastream as a single channel
	// The difference is so miniscule that it is unimportant
	for (size_t s = 0; s < numSamples; ++s)
	{
		samples[s] = static_cast<int16_t>(static_cast<qint64>(samples[s]) * mFadeOutRemaining / mFadeOutTotalSamples);
		mFadeOutRemaining -= 1;
		if (mFadeOutRemaining <= 0)
		{
			for (size_t s2 = s + 1; s2 < numSamples; ++s2)
			{
				samples[s2] = 0;
			}
			return;
		}
	}
}





////////////////////////////////////////////////////////////////////////////////
// AudioTempoChange:

AudioTempoChange::AudioTempoChange(std::shared_ptr<AudioDataSource> aLower):
	Super(aLower),
	mDestSampleRate(aLower->format().sampleRate()),
	mCurrentSampleRate(-1)
{
}





AudioTempoChange::~AudioTempoChange()
{
	// Nothing explicit needed, but needs to be in CPP file due to mResampler's destructor
}





size_t AudioTempoChange::read(void * aDest, size_t aMaxLen)
{
	// If the resampler params have changed, re-initialize the resampler:
	if ((mResampler == nullptr) || (mDestSampleRate != mCurrentSampleRate))
	{
		try
		{
			auto dstSampleRate = mDestSampleRate.load();
			mResampler.reset(AVPP::Resampler::create(
				mLower->format().sampleRate(),
				dstSampleRate,
				AVPP::Resampler::channelLayoutFromChannelCount(mLower->format().channelCount()),
				AVPP::Resampler::sampleFormatFromSampleType(mLower->format().sampleType())
			));
			mCurrentSampleRate = dstSampleRate;
		}
		catch (const std::exception & exc)
		{
			qWarning() << "Cannot reinitialize resampler: " << exc.what();
			return 0;
		}
	}

	// Read from mLower, then pass through Resampler:
	auto numToRead = static_cast<size_t>(static_cast<long long>(aMaxLen) * mLower->format().sampleRate() / mCurrentSampleRate);
	if (numToRead > aMaxLen)
	{
		numToRead = aMaxLen;  // aDest is only aMaxLen bytes long, cannot read more
	}
	auto numRead = mLower->read(aDest, numToRead & ~0x03UL);
	if (numRead == 0)
	{
		return 0;
	}
	auto bytesPerFrame = static_cast<size_t>(mLower->format().bytesPerFrame());
	auto out = mResampler->convert(
		const_cast<const uint8_t **>(reinterpret_cast<uint8_t **>(&aDest)),
		static_cast<int>(numRead / bytesPerFrame)
	);
	if (out.second > 0)
	{
		memcpy(aDest, out.first, out.second * bytesPerFrame);
	}
	return out.second * bytesPerFrame;
}





void AudioTempoChange::setTempo(double aTempo)
{
	mDestSampleRate = static_cast<int>(mLower->format().sampleRate() / aTempo);
	// The resampler will be reinitialized in the read() function to avoid threading issues
}





double AudioTempoChange::currentSongPosition() const
{
	return mLower->currentSongPosition() * mDestSampleRate / mLower->format().sampleRate();
}





double AudioTempoChange::remainingTime() const
{
	return mLower->remainingTime() * mDestSampleRate / mLower->format().sampleRate();
}

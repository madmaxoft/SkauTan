#include "AudioEffects.hpp"
#include <QAudioFormat>
#include <QDebug>
#include "../Stopwatch.hpp"
#include "AVPP.hpp"





AudioFadeOut::AudioFadeOut(std::shared_ptr<AudioDataSource> a_Lower):
	Super(a_Lower),
	m_IsFadingOut(false),
	m_FadeOutTotalSamples(0),
	m_FadeOutRemaining(0)
{
}





void AudioFadeOut::fadeOut(int a_Msec)
{
	m_FadeOutTotalSamples = format().channelCount() * a_Msec * format().sampleRate() / 1000;
	m_FadeOutRemaining = m_FadeOutTotalSamples;
	m_IsFadingOut = true;
}





size_t AudioFadeOut::read(void * a_Dest, size_t a_MaxLen)
{
	// STOPWATCH(__FUNCTION__);

	// If the Fade-out is completed, return end-of-stream:
	if (m_IsFadingOut && (m_FadeOutRemaining <= 0))
	{
		abort();
		return 0;
	}

	auto numBytesToRead = a_MaxLen & ~0x03u;  // Only read in multiples of 4 bytes
	auto numBytesRead = Super::read(a_Dest, numBytesToRead);

	// If there's non-multiple of 4 at the end of the data, trim it off:
	if ((numBytesRead & 0x03u) != 0)
	{
		numBytesRead = numBytesRead & ~0x03u;
	}

	// Fade-out:
	applyFadeOut(a_Dest, numBytesRead);

	return numBytesRead;
}





void AudioFadeOut::applyFadeOut(void * a_Data, size_t a_NumBytes)
{
	Q_UNUSED(a_Data);

	// STOPWATCH(__FUNCTION__);
	assert(a_NumBytes % 2 == 0);  // The data comes in complete samples

	if (!m_IsFadingOut)
	{
		return;
	}
	if (m_FadeOutRemaining <= 0)
	{
		// Reached the end of fadeout, just zero out all remaining data
		memset(a_Data, 0, static_cast<size_t>(a_NumBytes));
		abort();
		return;
	}
	auto numSamples = a_NumBytes / 2;
	auto samples = reinterpret_cast<int16_t *>(a_Data);
	// Despite being technically incorrect, we can get away with processing the whole datastream as a single channel
	// The difference is so miniscule that it is unimportant
	for (size_t s = 0; s < numSamples; ++s)
	{
		samples[s] = static_cast<int16_t>(static_cast<qint64>(samples[s]) * m_FadeOutRemaining / m_FadeOutTotalSamples);
		m_FadeOutRemaining -= 1;
		if (m_FadeOutRemaining <= 0)
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

AudioTempoChange::AudioTempoChange(std::shared_ptr<AudioDataSource> a_Lower):
	Super(a_Lower),
	m_DestSampleRate(a_Lower->format().sampleRate()),
	m_CurrentSampleRate(-1)
{
}





AudioTempoChange::~AudioTempoChange()
{
	// Nothing explicit needed, but needs to be in CPP file due to m_Resampler's destructor
}





size_t AudioTempoChange::read(void * a_Dest, size_t a_MaxLen)
{
	// If the resampler params have changed, re-initialize the resampler:
	if ((m_Resampler == nullptr) || (m_DestSampleRate != m_CurrentSampleRate))
	{
		try
		{
			auto dstSampleRate = m_DestSampleRate.load();
			m_Resampler.reset(AVPP::Resampler::create(
				m_Lower->format().sampleRate(),
				dstSampleRate,
				AVPP::Resampler::channelLayoutFromChannelCount(m_Lower->format().channelCount()),
				AVPP::Resampler::sampleFormatFromSampleType(m_Lower->format().sampleType())
			));
			m_CurrentSampleRate = dstSampleRate;
		}
		catch (const std::exception & exc)
		{
			qWarning() << "Cannot reinitialize resampler: " << exc.what();
			return 0;
		}
	}

	// Read from m_Lower, then pass through Resampler:
	auto numToRead = static_cast<size_t>(static_cast<long long>(a_MaxLen) * m_Lower->format().sampleRate() / m_CurrentSampleRate);
	if (numToRead > a_MaxLen)
	{
		numToRead = a_MaxLen;  // a_Dest is only a_MaxLen bytes long, cannot read more
	}
	auto numRead = m_Lower->read(a_Dest, numToRead & ~0x03UL);
	if (numRead == 0)
	{
		return 0;
	}
	auto bytesPerFrame = static_cast<size_t>(m_Lower->format().bytesPerFrame());
	auto out = m_Resampler->convert(
		const_cast<const uint8_t **>(reinterpret_cast<uint8_t **>(&a_Dest)),
		static_cast<int>(numRead / bytesPerFrame)
	);
	if (out.second > 0)
	{
		memcpy(a_Dest, out.first, out.second * bytesPerFrame);
	}
	return out.second * bytesPerFrame;
}





void AudioTempoChange::setTempo(double a_Tempo)
{
	m_DestSampleRate = static_cast<int>(m_Lower->format().sampleRate() / a_Tempo);
	// The resampler will be reinitialized in the read() function to avoid threading issues
}





double AudioTempoChange::currentSongPosition() const
{
	return m_Lower->currentSongPosition() * m_DestSampleRate / m_Lower->format().sampleRate();
}





double AudioTempoChange::remainingTime() const
{
	return m_Lower->remainingTime() * m_DestSampleRate / m_Lower->format().sampleRate();
}

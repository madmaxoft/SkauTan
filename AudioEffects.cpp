#include "AudioEffects.h"
#include <QAudioFormat>
#include <QDebug>
#include "Stopwatch.h"
#include "AVPP.h"





AudioFadeOut::AudioFadeOut(std::unique_ptr<AudioDataSource> && a_Lower):
	Super(std::move(a_Lower)),
	m_IsFadingOut(false),
	m_FadeOutTotalSamples(0),
	m_FadeOutRemaining(0)
{
}





AudioFadeOut::AudioFadeOut(AudioDataSource * a_Lower):
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
	qDebug() << __FUNCTION__ << ": Starting fadeout";
}





size_t AudioFadeOut::read(void * a_Dest, size_t a_MaxLen)
{
	// STOPWATCH(__FUNCTION__);

	// If the Fade-out is completed, return end-of-stream:
	if (m_IsFadingOut && (m_FadeOutRemaining <= 0))
	{
		qDebug() << __FUNCTION__ << ": Faded out completely, signaling end of stream.";
		abort();
		return 0;
	}

	auto numBytesToRead = a_MaxLen & ~0x03u;  // Only read in multiples of 4 bytes
	auto numBytesRead = Super::read(a_Dest, numBytesToRead);

	// If there's non-multiple of 4 at the end of the data, trim it off:
	if ((numBytesRead & 0x03u) != 0)
	{
		qDebug() << __FUNCTION__ << ": Dropping non-aligned data at the end of the buffer";
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
		qDebug() << __FUNCTION__ << ": Reached end of fadeout.";
		memset(a_Data, 0, static_cast<size_t>(a_NumBytes));
		abort();
		return;
	}
	qDebug() << __FUNCTION__ << ": Fading out " << a_NumBytes << "bytes starting at " << 100 * m_FadeOutRemaining / m_FadeOutTotalSamples << "%";
	auto numSamples = a_NumBytes / 2;
	auto samples = reinterpret_cast<int16_t *>(a_Data);
	// Despite being technically incorrect, we can get away with processing the whole datastream as a single channel
	// The difference is so miniscule that it is unimportant
	for (size_t s = 0; s < numSamples; ++s)
	{
		samples[s] = static_cast<int16_t>(static_cast<int>(samples[s]) * m_FadeOutRemaining / m_FadeOutTotalSamples);
		m_FadeOutRemaining -= 1;
		if (m_FadeOutRemaining <= 0)
		{
			qDebug() << __FUNCTION__ << ": Reached end of fadeout, zeroing out the rest.";
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

AudioTempoChange::AudioTempoChange(std::unique_ptr<AudioDataSource> && a_Lower):
	Super(std::move(a_Lower)),
	m_DestSampleRate(a_Lower->format().sampleRate()),
	m_CurrentSampleRate(-1)
{
}





AudioTempoChange::AudioTempoChange(AudioDataSource * a_Lower):
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
			qDebug() << __FUNCTION__ << ": Cannot reinitialize resampler: " << exc.what();
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

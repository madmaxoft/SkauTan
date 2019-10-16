#include "SongTempoDetector.hpp"
#include <QAudioFormat>
#include "Audio/AVPP.hpp"
#include "Audio/PlaybackBuffer.hpp"
#include "BackgroundTasks.hpp"
#include "Song.hpp"
#include "Utils.hpp"





/** Outputs a debug file containing the Samples data in one channel and beats indication in the other channel. */
static void debugBeatsInAudioData(
	const PlaybackBuffer & aAudioData,
	const SongTempoDetector::Options & aOptions,
	const std::vector<std::pair<size_t, Int32>> & aBeats
)
{
	if (aOptions.mDebugAudioBeatsFileName.isEmpty())
	{
		return;
	}
	QFile f(aOptions.mDebugAudioBeatsFileName);
	if (!f.open(QFile::WriteOnly))
	{
			qDebug() << "Cannot open file " << aOptions.mDebugAudioBeatsFileName << " for writing: " << f.errorString();
		return;
	}

	// Find the max beat weight, for normalization:
	Int32 maxBeatWeight = 0;
	for (const auto & beat: aBeats)
	{
		if (beat.second > maxBeatWeight)
		{
			maxBeatWeight = beat.second;
		}
	}
	float levelCoeff = 32767.0f / maxBeatWeight;

	size_t lastBeatIdx = 0;
	size_t maxIdx = aBeats.back().first;
	Int16 mixStrength = 0;
	std::vector<Int16> interlaced;
	interlaced.resize(aOptions.mStride * 2);
	for (size_t i = 0; i < maxIdx; ++i)
	{
		if (i == aBeats[lastBeatIdx].first)
		{
			mixStrength = static_cast<Int16>(aBeats[lastBeatIdx].second * levelCoeff);
			lastBeatIdx += 1;
		}
		auto samples = reinterpret_cast<const Int16 *>(aAudioData.audioData().data()) + i * aOptions.mStride;
		for (size_t j = 0; j < aOptions.mStride; ++j)
		{
			interlaced[2 * j] = samples[j];
			auto m = static_cast<int>(mixStrength * sin(static_cast<float>(j + aOptions.mStride * i) * 2000 / aOptions.mSampleRate));
			interlaced[2 * j + 1]  = static_cast<Int16>(Utils::clamp<int>(m, -32768, 32767));
		}
		mixStrength = static_cast<Int16>(mixStrength * 0.8);
		f.write(
			reinterpret_cast<const char *>(interlaced.data()),
			static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
		);
	}
}




/** Outputs a debug file with one channel of the original mSamples data and the other channel
of the detected levels. */
static void debugLevelsInAudioData(
	const PlaybackBuffer & aAudioData,
	const SongTempoDetector::Options & aOptions,
	const std::vector<Int32> & aLevels
)
{
	if (aOptions.mDebugAudioLevelsFileName.isEmpty())
	{
		return;
	}
	QFile f(aOptions.mDebugAudioLevelsFileName);
	if (!f.open(QFile::WriteOnly))
	{
		qDebug() << "Cannot open file " << aOptions.mDebugAudioLevelsFileName << " for writing: " << f.errorString();
		return;
	}

	// Find the max level, for normalization:
	size_t maxIdx = aLevels.size();
	Int32 maxLevel = 1;
	for (const auto & lev: aLevels)
	{
		if (lev > maxLevel)
		{
			maxLevel = lev;
		}
	}

	auto samples = reinterpret_cast<const Int16 *>(aAudioData.audioData().data());
	std::vector<Int16> interlaced;
	interlaced.resize(aOptions.mStride * 2);
	for (size_t i = 0; i < maxIdx; ++i)
	{
		float outF = static_cast<float>(aLevels[i]) / maxLevel;
		Int16 outLevel = static_cast<Int16>(Utils::clamp(static_cast<Int32>(outF * 65535) - 32768, -32768, 32767));
		for (size_t j = 0; j < aOptions.mStride; ++j)
		{
			interlaced[2 * j] = samples[i * aOptions.mStride + j];
			interlaced[2 * j + 1] = outLevel;
		}
		f.write(
			reinterpret_cast<const char *>(interlaced.data()),
			static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
		);
	}
}




////////////////////////////////////////////////////////////////////////////////
// SongTempoDetector:

SongTempoDetector::SongTempoDetector():
	m_QueueLength(0)
{
}





TempoDetector::ResultPtr SongTempoDetector::scanSong(SongPtr aSong, const SongTempoDetector::Options & aOptions)
{
	qDebug() << "Detecting tempo in " << aSong->fileName();

	// Decode the entire file into memory:
	auto context = AVPP::Format::createContext(aSong->fileName());
	if (context == nullptr)
	{
		qWarning() << "Cannot open file " << aSong->fileName();
		return nullptr;
	}
	QAudioFormat fmt;
	fmt.setSampleRate(aOptions.mSampleRate);
	fmt.setChannelCount(1);
	fmt.setSampleSize(16);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
	fmt.setCodec("audio/pcm");
	PlaybackBuffer buf(fmt);
	if (!context->routeAudioTo(&buf))
	{
		qWarning() << "Cannot route audio from file " << aSong->fileName();
		return nullptr;
	}
	context->decode();
	auto samples = reinterpret_cast<const Int16 *>(buf.audioData().data());
	auto numSamples = buf.audioData().size() / sizeof(Int16);
	auto result = TempoDetector::scan(samples, numSamples, aOptions);
	debugBeatsInAudioData(buf, aOptions, result->mBeats);
	debugLevelsInAudioData(buf, aOptions, result->mLevels);
	emit songScanned(aSong, result);
	return result;
}





void SongTempoDetector::queueScanSong(SongPtr aSong, const SongTempoDetector::Options & aOptions)
{
	SongTempoDetector::Options options(aOptions);
	m_QueueLength += 1;
	BackgroundTasks::enqueue(tr("Detect tempo: %1").arg(aSong->fileName()), [this, aSong, options]()
		{
			m_QueueLength -= 1;
			scanSong(aSong, options);
		}
	);
}

#include "TempoDetector.hpp"
#include <QAudioFormat>
#include "Audio/AVPP.hpp"
#include "Audio/PlaybackBuffer.hpp"
#include "BackgroundTasks.hpp"
#include "Song.hpp"
#include "Utils.hpp"





class Detector
{
public:
	Detector(SongPtr a_Song, const TempoDetector::Options & a_Options):
		m_Song(a_Song),
		m_Options(a_Options)
	{
	}





	/** Processes the song - detects its tempo. */
	std::shared_ptr<TempoDetector::Result> process()
	{
		qDebug() << "Detecting tempo in " << m_Song->fileName();

		// Decode the entire file into memory:
		auto context = AVPP::Format::createContext(m_Song->fileName());
		if (context == nullptr)
		{
			qWarning() << "Cannot open file " << m_Song->fileName();
			return nullptr;
		}
		QAudioFormat fmt;
		fmt.setSampleRate(m_Options.m_SampleRate);
		fmt.setChannelCount(1);
		fmt.setSampleSize(16);
		fmt.setSampleType(QAudioFormat::SignedInt);
		fmt.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
		fmt.setCodec("audio/pcm");
		PlaybackBuffer buf(fmt);
		if (!context->routeAudioTo(&buf))
		{
			qWarning() << "Cannot route audio from file " << m_Song->fileName();
			return nullptr;
		}
		context->decode();

		auto res = std::make_shared<TempoDetector::Result>();
		res->m_Options = m_Options;
		res->m_Levels = calcLevels(buf);
		if (m_Options.m_ShouldNormalizeLevels)
		{
			res->m_Levels = normalizeLevels(res->m_Levels);
		}
		debugLevelsInAudioData(buf, res->m_Levels);
		res->m_Beats = detectBeats(res->m_Levels);
		debugBeatsInAudioData(buf, res->m_Beats);

		std::tie(res->m_Tempo, res->m_Confidence) = detectTempoFromBeats(res->m_Beats, res->m_Levels);

		return res;
	}





	/** Calculates the best match for the tempo, and its confidence, based on the detected beats.
	Uses self-similarity matching to calculate the confidence for each tempo value.
	The returned confidence ranges from 0 to 100. */
	std::pair<double, double> detectTempoFromBeats(
		std::vector<std::pair<size_t, qint32>> & a_Beats,
		std::vector<qint32> & a_Levels
	)
	{
		// Calculate the similarities:
		qint32 maxLevel = 0;
		auto beats = prepareBeats(a_Beats, a_Levels, maxLevel);
		auto minOfs = Utils::floor<size_t>(mpmToBeatStrides(m_Options.m_MaxTempo));
		auto maxOfs = Utils::ceil<size_t> (mpmToBeatStrides(m_Options.m_MinTempo));
		std::vector<std::pair<double, double>> similarities;  // {similarity, mpm}
		for (size_t ofs = minOfs; ofs <= maxOfs; ++ofs)
		{
			auto sim = calcSimilarityBeatsWeight(a_Beats, beats, ofs, maxLevel);
			auto mpm = beatStridesToMpm(ofs);
			similarities.push_back({sim, mpm});
		}
		if (similarities.empty())
		{
			return {0, 0};
		}

		// Find the second-best MPM (that isn't close enough to the best MPM):
		std::sort(similarities.begin(), similarities.end(),
			[](auto a_Sim1, auto a_Sim2)
			{
				return (a_Sim1.first > a_Sim2.first);
			}
		);
		auto bestMpm = similarities[0].second;
		auto bestSimilarity = similarities[0].first;
		for (const auto & sim: similarities)
		{
			if (!isCloseEnoughMpm(sim.second, bestMpm))
			{
				auto confidence = 100 - 100 * sim.first / bestSimilarity;
				return {bestMpm, Utils::clamp<double>(confidence, 0, 100)};
			}
		}
		return {0, 0};
	}




	/** Returns true if the two MPMs are close enough to be considered a single match. */
	static bool isCloseEnoughMpm(double a_Mpm1, double a_Mpm2)
	{
		return (std::abs(a_Mpm1 - a_Mpm2) <= 1.5);
	}





	/** Calculates the self-similarity on a_Beats using the specified offset.
	a_BeatMap is the helper map for quick beat lookups, created by prepareBeats().
	The similarity is a number that can be compared to other similarities. */
	double calcSimilarityBeatsWeight(
		const std::vector<std::pair<size_t, qint32>> & a_Beats,
		const std::map<size_t, qint32> & a_BeatMap,
		size_t a_Offset,
		qint32 a_MaxLevel
	)
	{
		double res = 0;
		for (const auto & beat: a_Beats)
		{
			auto idx = beat.first + a_Offset;
			auto itr = a_BeatMap.find(idx);
			if (itr != a_BeatMap.cend())
			{
				res += a_MaxLevel - std::abs(beat.second - itr->second);
				continue;
			}
			itr = a_BeatMap.find(idx + 1);
			if (itr != a_BeatMap.cend())
			{
				res += a_MaxLevel - std::abs(beat.second - itr->second) / 2;
				continue;
			}
			itr = a_BeatMap.find(idx - 1);
			if (itr != a_BeatMap.cend())
			{
				res += a_MaxLevel - std::abs(beat.second - itr->second) / 2;
				continue;
			}
		}
		return res;
	}





	/** Prepares a lookup-table of beats based on the input calculated beats.
	Returns a map of beatIndex -> soundLevel for each detected beat. */
	std::map<size_t, qint32> prepareBeats(
		const std::vector<std::pair<size_t, qint32>> & a_Beats,
		const std::vector<qint32> & a_Levels,
		qint32 & a_OutMaxLevel
	)
	{
		std::map<size_t, qint32> res;
		qint32 maxLevel = 0;
		for (const auto & beat: a_Beats)
		{
			auto level = std::abs(a_Levels[beat.first]);
			res[beat.first] = level;
			if (level > maxLevel)
			{
				maxLevel = level;
			}
		}
		a_OutMaxLevel = maxLevel;
		return res;
	}





	/** Converts the number of strides (across calculated beats) that make up a time interval,
	into the corresponding MPM, rounded to 2 decimal places.
	The inverse function is mpmToBeatStrides() */
	double beatStridesToMpm(double a_BeatStrides)
	{
		auto mpm = (m_Options.m_SampleRate * 60 / static_cast<int>(m_Options.m_Stride)) / a_BeatStrides;
		return std::floor(mpm * 100 + 0.5) / 100;
	}





	/** Converts the MPM to the stride length (across calculated beats) representing the tempo. */
	double mpmToBeatStrides(double a_Mpm)
	{
		return ((m_Options.m_SampleRate * 60 / static_cast<int>(m_Options.m_Stride)) / a_Mpm);
	}





	/** Calculates levels of the audio data.
	Uses the appropriate algorithm according to m_Options. */
	std::vector<qint32> calcLevels(const PlaybackBuffer & a_Buffer)
	{
		switch (m_Options.m_LevelAlgorithm)
		{
			case TempoDetector::laSumDist:               return calcLevelsSumDist(a_Buffer);
			case TempoDetector::laDiscreetSineTransform: return calcLevelsDST(a_Buffer);
			case TempoDetector::laMinMax:                return calcLevelsMinMax(a_Buffer);
			case TempoDetector::laSumDistMinMax:         return calcLevelsSumDistMinMax(a_Buffer);
		}
		assert(!"Invalid level algorithm");
		qWarning() << "Invalid level algorithm";
		return {};
	}





	/** Calculates the loudness levels from the input audio data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<qint32> calcLevelsSumDist(const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / m_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate the first window:
		qint32 current = 0;
		for (size_t i = 0; i < m_Options.m_WindowSize; ++i)
		{
			current += std::abs(audio[i] - audio[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = numSamples - m_Options.m_WindowSize - m_Options.m_Stride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += m_Options.m_Stride)
		{
			for (size_t i = 0; i < m_Options.m_Stride; ++i)
			{
				current += std::abs(audio[curPos + i + m_Options.m_WindowSize] - audio[curPos + i + m_Options.m_WindowSize + 1]);
				current -= std::abs(audio[curPos + i] - audio[curPos + i + 1]);
			}
			res.push_back(current);
		}
		return res;
	}




	/** Calculates the loudness levels from the input audio data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<qint32> calcLevelsSumDistMinMax(const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / m_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate the first window:
		qint32 current = 0;
		for (size_t i = 0; i < m_Options.m_WindowSize; ++i)
		{
			current += std::abs(audio[i] - audio[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = numSamples - m_Options.m_WindowSize - m_Options.m_Stride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += m_Options.m_Stride)
		{
			for (size_t i = 0; i < m_Options.m_Stride; ++i)
			{
				current += std::abs(audio[curPos + i + m_Options.m_WindowSize] - audio[curPos + i + m_Options.m_WindowSize + 1]);
				current -= std::abs(audio[curPos + i] - audio[curPos + i + 1]);
			}
			auto minVal = audio[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < m_Options.m_WindowSize; ++i)
			{
				auto val = audio[curPos + i];
				if (val > maxVal)
				{
					maxVal = val;
				}
				if (val < minVal)
				{
					minVal = val;
				}
			}
			res.push_back((current + maxVal - minVal) / 2);
		}
		return res;
	}




	/** Calculates the loudness levels from the input audio data.
	The level is calculated as the difference between the max and min value in the sample range. */
	std::vector<qint32> calcLevelsMinMax(const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / m_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate each window:
		size_t maxPos = numSamples - m_Options.m_WindowSize - m_Options.m_Stride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += m_Options.m_Stride)
		{
			auto minVal = audio[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < m_Options.m_WindowSize; ++i)
			{
				auto val = audio[curPos + i];
				if (val > maxVal)
				{
					maxVal = val;
				}
				if (val < minVal)
				{
					minVal = val;
				}
			}
			res.push_back(maxVal - minVal);
		}
		return res;
	}




	/** Calculates the loudness levels from the input audio data, using discreet sine transform.
	The idea is that a change in what we think of as levels should be across all frequencies.
	So we sample a few frequency bands and calculate the levels from those.
	Doesn't seem to work so well as the Simple method. */
	std::vector<qint32> calcLevelsDST(const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / m_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate each window's level:
		size_t maxPos = numSamples - m_Options.m_WindowSize - m_Options.m_Stride - 1;
		static const float freq[] = {
			600,  //   80 Hz
			250,  //  192 Hz
			109,  //  440 Hz
			48,   // 1000 Hz
		};
		static const size_t NUM_FREQ = sizeof(freq) / sizeof(*freq);
		float qint32Min = std::numeric_limits<qint32>::min();
		float qint32Max = std::numeric_limits<qint32>::max();
		for (size_t curPos = 0; curPos < maxPos; curPos += m_Options.m_Stride)
		{
			float freqCoeff[NUM_FREQ] = {};
			for (size_t i = 0; i < m_Options.m_WindowSize; ++i)
			{
				auto sampleIndex = i + curPos;
				auto sampleIndexF = static_cast<float>(i + curPos);
				auto audioSample = audio[sampleIndex];
				for (size_t f = 0; f < NUM_FREQ; ++f)
				{
					freqCoeff[f] += audioSample * sin(sampleIndexF / freq[f]);
				}
			}
			float level = 0;
			for (auto f: freqCoeff)
			{
				level += std::abs(f);
			}
			res.push_back(static_cast<qint32>(Utils::clamp(level, qint32Min, qint32Max)));
		}
		return res;
	}




	/** Outputs a debug audio file with one channel of the original audio data and the other channel
	of the detected levels. */
	void debugLevelsInAudioData(
		const PlaybackBuffer & a_Buf,
		const std::vector<qint32> & a_Levels
	)
	{
		if (m_Options.m_DebugAudioLevelsFileName.isEmpty())
		{
			return;
		}
		QFile f(m_Options.m_DebugAudioLevelsFileName);
		if (!f.open(QIODevice::WriteOnly))
		{
			qDebug() << "Cannot open debug file " << m_Options.m_DebugAudioLevelsFileName;
			return;
		}
		size_t maxIdx = a_Levels.size();
		qint32 maxLevel = 1;
		for (const auto & lev: a_Levels)
		{
			if (lev > maxLevel)
			{
				maxLevel = lev;
			}
		}
		auto audio = reinterpret_cast<const qint16 *>(a_Buf.audioData().data());
		std::vector<qint16> interlaced;
		interlaced.resize(m_Options.m_Stride * 2);
		for (size_t i = 0; i < maxIdx; ++i)
		{
			float outF = static_cast<float>(a_Levels[i]) / maxLevel;
			qint16 outLevel = static_cast<qint16>(Utils::clamp<qint32>(static_cast<qint32>(outF * 65535) - 32768, -32768, 32767));
			for (size_t j = 0; j < m_Options.m_Stride; ++j)
			{
				interlaced[2 * j] = audio[i * m_Options.m_Stride + j];
				interlaced[2 * j + 1] = outLevel;
			}
			f.write(
				reinterpret_cast<const char *>(interlaced.data()),
				static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
			);
		}
	}




	/** Normalizes the input levels across the m_NormalizeLevelsWindowSize neighbors. */
	std::vector<qint32> normalizeLevels(
		const std::vector<qint32> & a_Levels
	)
	{
		// Normalize against the local min / max
		std::vector<qint32> normalizedLevels;
		auto count = a_Levels.size();
		normalizedLevels.resize(count);
		auto win = m_Options.m_NormalizeLevelsWindowSize;
		for (size_t i = 0; i < count; ++i)
		{
			size_t startIdx = (i < win) ? 0 : i - win;
			size_t endIdx = std::min(i + win, count);
			auto lMin = a_Levels[startIdx];
			auto lMax = a_Levels[startIdx];
			for (size_t j = startIdx + 1; j < endIdx; ++j)
			{
				if (a_Levels[j] < lMin)
				{
					lMin = a_Levels[j];
				}
				else if (a_Levels[j] > lMax)
				{
					lMax = a_Levels[j];
				}
			}
			normalizedLevels[i] = static_cast<qint32>(65536.0f * (a_Levels[i] - lMin) / (lMax - lMin + 1));
		}
		return normalizedLevels;

		/*
		// Normalize against the local average
		std::vector<qint32> normalizedLevels;
		normalizedLevels.resize(a_Levels.size());
		qint32 sum = 0;
		for (size_t i = 0; i < m_Options.m_NormalizeLevelsWindowSize; ++i)
		{
			sum += a_Levels[i];
			normalizedLevels[i] = 0;
		}
		size_t maxIdx = a_Levels.size() - m_Options.m_NormalizeLevelsWindowSize;
		size_t halfSize = m_Options.m_NormalizeLevelsWindowSize / 2;
		for (size_t i = 0; i < maxIdx; ++i)
		{
			normalizedLevels[i + halfSize] = static_cast<qint16>(Utils::clamp<qint32>(
				static_cast<qint32>(327670.0f * a_Levels[i + halfSize] / sum), -32768, 32767
			));
			sum = sum + a_Levels[i + m_Options.m_NormalizeLevelsWindowSize] - a_Levels[i];
		}
		for (size_t i = maxIdx; i < a_Levels.size(); ++i)
		{
			normalizedLevels[i] = 0;
		}
		return normalizedLevels;
		*/
	}




	/** Detects the beats within the provided audio levels.
	Returns a vector containing the indices (into a_Levels) where beats have been located. */
	std::vector<std::pair<size_t, qint32>> detectBeats(const std::vector<qint32> & a_Levels)
	{
		// Calculate the climbing tendencies in a_Levels:
		std::vector<qint32> climbs;
		auto count = a_Levels.size();
		climbs.reserve(count);
		climbs.push_back(0);
		qint32 maxClimb = 0;
		auto maxLevel = a_Levels[0];
		for (size_t i = 1; i < count; ++i)
		{
			auto c = std::max(0, a_Levels[i] - a_Levels[i - 1]);
			climbs.push_back(c);
			if (c > maxClimb)
			{
				maxClimb = c;
			}
			if (a_Levels[i] > maxLevel)
			{
				maxLevel = a_Levels[i];
			}
		}

		// Calculate beats and their weight:
		std::vector<std::pair<size_t, qint32>> weightedBeats;
		auto maxIdx = a_Levels.size() - m_Options.m_LocalMaxDistance;
		for (size_t i = m_Options.m_LocalMaxDistance; i < maxIdx; ++i)
		{
			bool isLocalMaximum = true;
			for (size_t j = i - m_Options.m_LocalMaxDistance; j < i + m_Options.m_LocalMaxDistance; ++j)
			{
				if ((i != j) && (climbs[j] >= climbs[i]))
				{
					isLocalMaximum = false;
					break;
				}
			}
			if (!isLocalMaximum)
			{
				continue;
			}
			weightedBeats.emplace_back(i, climbs[i]);
		}

		// Prune the beats according to their weight, so that we only have 240 BPM on average:
		using BeatType = decltype(weightedBeats[0]);
		std::sort(weightedBeats.begin(), weightedBeats.end(),
			[](BeatType b1, BeatType b2)
			{
				return (b1.second > b2.second);
			}
		);
		size_t songLengthSec = a_Levels.size() * m_Options.m_Stride / static_cast<size_t>(m_Options.m_SampleRate);
		size_t wantedNumBeats = 240 * songLengthSec / 60;
		if (weightedBeats.size() > wantedNumBeats)
		{
			weightedBeats.resize(wantedNumBeats);
		}
		std::sort(weightedBeats.begin(), weightedBeats.end(),
			[](BeatType b1, BeatType b2)
			{
				return (b1.first < b2.first);
			}
		);
		return weightedBeats;
	}





	/** Calculates the histogram of beat distances.
	Returns a map of bpm -> number of occurences. */
	std::map<int, size_t> beatsToHistogram(
		const std::vector<std::pair<size_t, qint32>> & a_Beats
	)
	{
		std::map<int, size_t> beatDiffMap;  // Maps beat time difference -> number of occurrences of such differrence
		auto numBeats = a_Beats.size();
		for (size_t i = 0; i < numBeats; ++i)
		{
			auto start = i - std::min<size_t>(i, 2);
			auto end = std::min(i + 2, numBeats);
			auto currBeat = a_Beats[i].first;
			for (size_t j = start; j < end; ++j)
			{
				if (i == j)
				{
					continue;
				}
				auto dist = (i > j) ? (currBeat - a_Beats[j].first) : (a_Beats[j].first - currBeat);
				auto bpm = static_cast<int>(std::round(distToBPM(dist)));
				beatDiffMap[bpm] += 1;
			}
		}
		return beatDiffMap;
	}





	/** Converts the distance between levels to a BPM value. */
	double distToBPM(size_t a_Dist)
	{
		return 60.0 / (m_Options.m_Stride * static_cast<double>(a_Dist) / m_Options.m_SampleRate);
	}




	/** Outputs the debug audio data with mixed-in beats. */
	void debugBeatsInAudioData(
		const PlaybackBuffer & a_Buf,
		const std::vector<std::pair<size_t, qint32>> & a_Beats
	)
	{
		if (m_Options.m_DebugAudioBeatsFileName.isEmpty())
		{
			return;
		}
		QFile f(m_Options.m_DebugAudioBeatsFileName);
		if (!f.open(QIODevice::WriteOnly))
		{
			qDebug() << "Cannot open debug file " << m_Options.m_DebugAudioBeatsFileName;
			return;
		}
		qint32 maxBeatWeight = 0;
		for (const auto & beat: a_Beats)
		{
			if (beat.second > maxBeatWeight)
			{
				maxBeatWeight = beat.second;
			}
		}
		float levelCoeff = 32767.0f / maxBeatWeight;
		size_t lastBeatIdx = 0;
		size_t maxIdx = a_Beats.back().first;
		qint16 mixStrength = 0;
		std::vector<qint16> interlaced;
		interlaced.resize(m_Options.m_Stride * 2);
		for (size_t i = 0; i < maxIdx; ++i)
		{
			if (i == a_Beats[lastBeatIdx].first)
			{
				mixStrength = static_cast<qint16>(a_Beats[lastBeatIdx].second * levelCoeff);
				lastBeatIdx += 1;
			}
			auto audio = reinterpret_cast<const qint16 *>(a_Buf.audioData().data()) + i * m_Options.m_Stride;
			for (size_t j = 0; j < m_Options.m_Stride; ++j)
			{
				interlaced[2 * j] = audio[j];
				auto m = static_cast<int>(mixStrength * sin(static_cast<float>(j + m_Options.m_Stride * i) * 2000 / m_Options.m_SampleRate));
				interlaced[2 * j + 1]  = static_cast<qint16>(Utils::clamp<int>(m, -32768, 32767));
			}
			mixStrength = static_cast<qint16>(mixStrength * 0.8);
			f.write(
				reinterpret_cast<const char *>(interlaced.data()),
				static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
			);
		}
	}




	/** Calculates the confidence in the detected tempo, from the histogram. */
	int calcConfidence(const std::vector<std::pair<int, size_t>> & a_BeatHistogram)
	{
		if (a_BeatHistogram.empty())
		{
			return 0;
		}
		auto finalGuess = a_BeatHistogram[0].first;
		size_t pro = 0, against = 0;
		for (const auto & h: a_BeatHistogram)
		{
			if (isCompatibleTempo(finalGuess, h.first))
			{
				pro += h.second;
			}
			else
			{
				against = h.second;
			}
		}
		return static_cast<int>(100 * pro / (pro + against));
	}





	/** Returns true if the two tempos are "compatible" - they are the same up to a factor. */
	static bool isCompatibleTempo(int a_Tempo1, int a_Tempo2)
	{
		for (const auto & m: {1, 2, 3, 4, 6, 8, 12, 16})
		{
			if (
				(std::abs(a_Tempo1 - a_Tempo2 * m) < m) ||
				(std::abs(a_Tempo1 * m - a_Tempo2) < m)
			)
			{
				return true;
			}
		}
		return false;
	}





protected:

	/** The song to scan. */
	SongPtr m_Song;

	/** The options for the detection. */
	const TempoDetector::Options & m_Options;
};





////////////////////////////////////////////////////////////////////////////////
// TempoDetector:

TempoDetector::TempoDetector():
	m_QueueLength(0)
{

}





std::shared_ptr<TempoDetector::Result> TempoDetector::scanSong(SongPtr a_Song, const Options & a_Options)
{
	Detector d(a_Song, a_Options);
	auto result = d.process();
	emit songScanned(a_Song, result);
	return result;
}





void TempoDetector::queueScanSong(SongPtr a_Song, const TempoDetector::Options & a_Options)
{
	TempoDetector::Options options(a_Options);
	m_QueueLength += 1;
	BackgroundTasks::enqueue(tr("Detect tempo: %1").arg(a_Song->fileName()), [this, a_Song, options]()
		{
			Detector d(a_Song, options);
			auto result = d.process();
			m_QueueLength -= 1;
			emit songScanned(a_Song, result);
		}
	);
}





TempoDetector::Options::Options():
	m_SampleRate(500),
	m_LevelAlgorithm(laSumDistMinMax),
	m_WindowSize(11),
	m_Stride(8),
	m_LocalMaxDistance(13),
	m_ShouldNormalizeLevels(true),
	m_NormalizeLevelsWindowSize(31),
	m_MaxTempo(200),
	m_MinTempo(15)
{
}





bool TempoDetector::Options::operator <(const TempoDetector::Options & a_Other)
{
	#define COMPARE(val) \
		if (val < a_Other.val) \
		{ \
			return true; \
		} \
		if (val > a_Other.val) \
		{ \
			return false; \
		}

	COMPARE(m_SampleRate)
	COMPARE(m_LevelAlgorithm)
	COMPARE(m_WindowSize)
	COMPARE(m_Stride)
	COMPARE(m_LocalMaxDistance)
	COMPARE(m_ShouldNormalizeLevels)
	if (m_ShouldNormalizeLevels)
	{
		COMPARE(m_NormalizeLevelsWindowSize);
	}
	COMPARE(m_MaxTempo);
	COMPARE(m_MinTempo);
	return false;
}





bool TempoDetector::Options::operator ==(const TempoDetector::Options & a_Other)
{
	return (
		(m_SampleRate == a_Other.m_SampleRate) &&
		(m_LevelAlgorithm == a_Other.m_LevelAlgorithm) &&
		(m_WindowSize == a_Other.m_WindowSize) &&
		(m_Stride == a_Other.m_Stride) &&
		(m_LocalMaxDistance == a_Other.m_LocalMaxDistance) &&
		(m_ShouldNormalizeLevels == a_Other.m_ShouldNormalizeLevels) &&
		(m_NormalizeLevelsWindowSize == a_Other.m_NormalizeLevelsWindowSize) &&
		(m_MaxTempo == a_Other.m_MaxTempo) &&
		(m_MinTempo == a_Other.m_MinTempo)
	);
}

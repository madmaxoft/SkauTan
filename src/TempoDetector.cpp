#include "TempoDetector.hpp"
#include <QAudioFormat>
#include "Audio/AVPP.hpp"
#include "Audio/PlaybackBuffer.hpp"
#include "BackgroundTasks.hpp"
#include "Song.hpp"
#include "Utils.hpp"





/** If defined the pre-aggregation tempos will be output to qDebug(). */
// #define DEBUG_OUTPUT_AGGREGATION





/** Returns a vector of TempoDetector Options to be used for detecting in the specified tempo range. */
static std::vector<TempoDetector::Options> optimalOptions(const std::pair<int, int> & a_TempoRange)
{
	TempoDetector::Options opt;
	opt.m_SampleRate = 500;
	opt.m_LevelAlgorithm = TempoDetector::laSumDistMinMax;
	opt.m_MinTempo = a_TempoRange.first;
	opt.m_MaxTempo = a_TempoRange.second;
	opt.m_ShouldNormalizeLevels = true;
	opt.m_NormalizeLevelsWindowSize = 31;
	opt.m_Stride = 8;
	opt.m_LocalMaxDistance = 13;
	std::vector<TempoDetector::Options> opts;
	for (auto ws: {11, 13, 15, 17, 19})
	{
		opt.m_WindowSize = static_cast<size_t>(ws);
		opts.push_back(opt);
	}
	return opts;
}





////////////////////////////////////////////////////////////////////////////////
// Detector:

/** The actual tempo detector. The TempoDetector interface class uses this to detect the tempo. */
class Detector
{
public:
	Detector(SongPtr a_Song, const std::vector<TempoDetector::Options> & a_Options):
		m_Song(a_Song),
		m_Options(a_Options)
	{
	}





	/** Processes the song - detects its tempo. */
	std::shared_ptr<TempoDetector::Result> process()
	{
		if (m_Options.empty())
		{
			qWarning() << "Tempo detection started with no options given.";
			assert(!"Tempo detection started with no options given.");
			return nullptr;
		}

		// Decode the entire file into memory:
		auto context = AVPP::Format::createContext(m_Song->fileName());
		if (context == nullptr)
		{
			qWarning() << "Cannot open file " << m_Song->fileName();
			return nullptr;
		}
		QAudioFormat fmt;
		fmt.setSampleRate(m_Options[0].m_SampleRate);
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
		if (buf.shouldAbort())
		{
			qDebug() << "Decoding audio failed: " << m_Song->fileName();
			return nullptr;
		}

		// Run the audio through the detector, once per each options item:
		auto res = std::make_shared<TempoDetector::Result>();
		res->m_Options = m_Options;
		for (const auto & opt: m_Options)
		{
			res->m_Levels = calcLevels(opt, buf);
			if (opt.m_ShouldNormalizeLevels)
			{
				res->m_Levels = normalizeLevels(opt, res->m_Levels);
			}
			debugLevelsInAudioData(opt, buf, res->m_Levels);
			res->m_Beats = detectBeats(opt, res->m_Levels);
			debugBeatsInAudioData(opt, buf, res->m_Beats);
			if (!res->m_Beats.empty())
			{
				res->m_Tempos.push_back(detectTempoFromBeats(opt, res->m_Beats, res->m_Levels));
			}
		}
		if (res->m_Tempos.empty())
		{
			return nullptr;
		}
		std::tie(res->m_Tempo, res->m_Confidence) = aggregateDetectedTempos(res->m_Tempos);

		return res;
	}





	/** Processes the (potentionally multiple) tempos detected from different options, and produces
	the final tempo and confidence result. */
	std::pair<double, double> aggregateDetectedTempos(const std::vector<std::pair<double, double>> & a_Tempos)
	{
		/*
		// DEBUG:
		qDebug() << "Aggregating detected tempos:";
		for (const auto & tempo: a_Tempos)
		{
			qDebug() << "  tempo: " << tempo.first << "; confidence: " << tempo.second;
		}
		*/

		assert(!a_Tempos.empty());
		// If there's just one result, use it directly:
		if (a_Tempos.size() == 1)
		{
			return a_Tempos[0];
		}

		// For multiple results, aggregate the compatible tempos and their confidences:
		std::vector<std::pair<double, double>> processedTempos;
		for (const auto & tin: a_Tempos)
		{
			bool hasFound = false;
			for (auto & tout: processedTempos)
			{
				if (isCompatibleTempo(tout.first, tin.first))
				{
					// There's a compatible tempo already in the processedTempos, add my confidence to it:
					tout.second += tin.second;
					hasFound = true;
					break;
				}
			}
			if (!hasFound)
			{
				processedTempos.push_back(tin);
			}
		}  // for tin: a_Tempos[]
		assert(!processedTempos.empty());
		std::sort(processedTempos.begin(), processedTempos.end(),
			[](auto a_Tempo1, auto a_Tempo2)
			{
				return (a_Tempo1.second > a_Tempo2.second);
			}
		);

		#ifdef DEBUG_OUTPUT_AGGREGATION
			qDebug() << "Processed aggregated tempos:";
			for (const auto & tempo: processedTempos)
			{
				qDebug() << "  tempo: " << tempo.first << "; confidence: " << tempo.second;
			}
		#endif  // DEBUG_OUTPUT_AGGREGATION

		if (processedTempos.size() == 1)
		{
			return processedTempos[0];
		}

		// Pick the tempos with the highest sum of confidence, then use the confidences as weights for averaging the contributing tempos:
		auto pickedTempo = processedTempos[0];
		double sumTempo = 0, sumConfidence = 0;
		double sumNegConfidence = 0;  // Sum of confidences for tempos NOT matching
		for (const auto & tin: a_Tempos)
		{
			if (isCompatibleTempo(tin.first, pickedTempo.first))
			{
				sumTempo += tin.first * tin.second;
				sumConfidence += tin.second;
			}
			else
			{
				sumNegConfidence += tin.second;
			}
		}
		auto res = std::floor(sumTempo / sumConfidence * 10 + 0.5) / 10;  // keep only 1 decimal digit
		return {res, 100 * sumConfidence / (sumConfidence + sumNegConfidence)};
	}





	/** Returns true if the two tempos are compatible - close enough to each other. */
	static bool isCompatibleTempo(double a_Tempo1, double a_Tempo2)
	{
		return (std::abs(a_Tempo1 - a_Tempo2) < 1.2);
	}





	/** Calculates the best match for the tempo, and its confidence, based on the detected beats.
	Uses self-similarity matching to calculate the confidence for each tempo value.
	The returned confidence ranges from 0 to 100. */
	std::pair<double, double> detectTempoFromBeats(
		const TempoDetector::Options & a_Options,
		std::vector<std::pair<size_t, qint32>> & a_Beats,
		std::vector<qint32> & a_Levels
	)
	{
		// Calculate the similarities:
		qint32 maxLevel = 0;
		auto beats = prepareBeats(a_Beats, a_Levels, maxLevel);
		auto minOfs = Utils::floor<size_t>(mpmToBeatStrides(a_Options, a_Options.m_MaxTempo));
		auto maxOfs = Utils::ceil<size_t> (mpmToBeatStrides(a_Options, a_Options.m_MinTempo));
		std::vector<std::pair<double, double>> similarities;  // {similarity, mpm}
		for (size_t ofs = minOfs; ofs <= maxOfs; ++ofs)
		{
			auto sim = calcSimilarityBeatsWeight(a_Beats, beats, ofs, maxLevel);
			auto mpm = beatStridesToMpm(a_Options, ofs);
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
		if (bestSimilarity < 1)
		{
			qDebug() << "Bad similarity: " << bestSimilarity;
		}
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
	double beatStridesToMpm(
		const TempoDetector::Options & a_Options,
		double a_BeatStrides
	)
	{
		auto mpm = (a_Options.m_SampleRate * 60 / static_cast<int>(a_Options.m_Stride)) / a_BeatStrides;
		return std::floor(mpm * 100 + 0.5) / 100;
	}





	/** Converts the MPM to the stride length (across calculated beats) representing the tempo. */
	double mpmToBeatStrides(const TempoDetector::Options & a_Options, double a_Mpm)
	{
		return ((a_Options.m_SampleRate * 60 / static_cast<int>(a_Options.m_Stride)) / a_Mpm);
	}





	/** Calculates levels of the audio data.
	Uses the appropriate algorithm according to m_Options. */
	std::vector<qint32> calcLevels(const TempoDetector::Options & a_Options, const PlaybackBuffer & a_Buffer)
	{
		switch (a_Options.m_LevelAlgorithm)
		{
			case TempoDetector::laSumDist:               return calcLevelsSumDist      (a_Options, a_Buffer);
			case TempoDetector::laDiscreetSineTransform: return calcLevelsDST          (a_Options, a_Buffer);
			case TempoDetector::laMinMax:                return calcLevelsMinMax       (a_Options, a_Buffer);
			case TempoDetector::laSumDistMinMax:         return calcLevelsSumDistMinMax(a_Options, a_Buffer);
		}
		assert(!"Invalid level algorithm");
		qWarning() << "Invalid level algorithm";
		return {};
	}





	/** Calculates the loudness levels from the input audio data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<qint32> calcLevelsSumDist(const TempoDetector::Options & a_Options, const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / a_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate the first window:
		qint32 current = 0;
		for (size_t i = 0; i < a_Options.m_WindowSize; ++i)
		{
			current += std::abs(audio[i] - audio[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = numSamples - a_Options.m_WindowSize - a_Options.m_Stride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += a_Options.m_Stride)
		{
			for (size_t i = 0; i < a_Options.m_Stride; ++i)
			{
				current += std::abs(audio[curPos + i + a_Options.m_WindowSize] - audio[curPos + i + a_Options.m_WindowSize + 1]);
				current -= std::abs(audio[curPos + i] - audio[curPos + i + 1]);
			}
			res.push_back(current);
		}
		return res;
	}




	/** Calculates the loudness levels from the input audio data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<qint32> calcLevelsSumDistMinMax(const TempoDetector::Options & a_Options, const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / a_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate the first window:
		qint32 current = 0;
		for (size_t i = 0; i < a_Options.m_WindowSize; ++i)
		{
			current += std::abs(audio[i] - audio[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = numSamples - a_Options.m_WindowSize - a_Options.m_Stride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += a_Options.m_Stride)
		{
			for (size_t i = 0; i < a_Options.m_Stride; ++i)
			{
				current += std::abs(audio[curPos + i + a_Options.m_WindowSize] - audio[curPos + i + a_Options.m_WindowSize + 1]);
				current -= std::abs(audio[curPos + i] - audio[curPos + i + 1]);
			}
			auto minVal = audio[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < a_Options.m_WindowSize; ++i)
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
	std::vector<qint32> calcLevelsMinMax(const TempoDetector::Options & a_Options, const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / a_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate each window:
		size_t maxPos = numSamples - a_Options.m_WindowSize - a_Options.m_Stride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += a_Options.m_Stride)
		{
			auto minVal = audio[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < a_Options.m_WindowSize; ++i)
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
	std::vector<qint32> calcLevelsDST(const TempoDetector::Options & a_Options, const PlaybackBuffer & a_Buffer)
	{
		std::vector<qint32> res;
		auto numSamples = a_Buffer.bufferLimit() / sizeof(qint16);
		res.reserve(numSamples / a_Options.m_Stride);
		auto audio = reinterpret_cast<const qint16 *>(a_Buffer.audioData().data());

		// Calculate each window's level:
		size_t maxPos = numSamples - a_Options.m_WindowSize - a_Options.m_Stride - 1;
		static const float freq[] = {
			600,  //   80 Hz
			250,  //  192 Hz
			109,  //  440 Hz
			48,   // 1000 Hz
		};
		static const size_t NUM_FREQ = sizeof(freq) / sizeof(*freq);
		float qint32Min = std::numeric_limits<qint32>::min();
		float qint32Max = std::numeric_limits<qint32>::max();
		for (size_t curPos = 0; curPos < maxPos; curPos += a_Options.m_Stride)
		{
			float freqCoeff[NUM_FREQ] = {};
			for (size_t i = 0; i < a_Options.m_WindowSize; ++i)
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
		const TempoDetector::Options & a_Options,
		const PlaybackBuffer & a_Buf,
		const std::vector<qint32> & a_Levels
	)
	{
		if (a_Options.m_DebugAudioLevelsFileName.isEmpty())
		{
			return;
		}
		QFile f(a_Options.m_DebugAudioLevelsFileName);
		if (!f.open(QIODevice::WriteOnly))
		{
			qDebug() << "Cannot open debug file " << a_Options.m_DebugAudioLevelsFileName;
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
		interlaced.resize(a_Options.m_Stride * 2);
		for (size_t i = 0; i < maxIdx; ++i)
		{
			float outF = static_cast<float>(a_Levels[i]) / maxLevel;
			qint16 outLevel = static_cast<qint16>(Utils::clamp<qint32>(static_cast<qint32>(outF * 65535) - 32768, -32768, 32767));
			for (size_t j = 0; j < a_Options.m_Stride; ++j)
			{
				interlaced[2 * j] = audio[i * a_Options.m_Stride + j];
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
		const TempoDetector::Options & a_Options,
		const std::vector<qint32> & a_Levels
	)
	{
		// Normalize against the local min / max
		std::vector<qint32> normalizedLevels;
		auto count = a_Levels.size();
		normalizedLevels.resize(count);
		auto win = a_Options.m_NormalizeLevelsWindowSize;
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
	}




	/** Detects the beats within the provided audio levels.
	Returns a vector containing the indices (into a_Levels) where beats have been located. */
	std::vector<std::pair<size_t, qint32>> detectBeats(
		const TempoDetector::Options & a_Options,
		const std::vector<qint32> & a_Levels
	)
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
		auto maxIdx = a_Levels.size() - a_Options.m_LocalMaxDistance;
		for (size_t i = a_Options.m_LocalMaxDistance; i < maxIdx; ++i)
		{
			bool isLocalMaximum = true;
			for (size_t j = i - a_Options.m_LocalMaxDistance; j < i + a_Options.m_LocalMaxDistance; ++j)
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
		size_t songLengthSec = a_Levels.size() * a_Options.m_Stride / static_cast<size_t>(a_Options.m_SampleRate);
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





	/** Outputs the debug audio data with mixed-in beats. */
	void debugBeatsInAudioData(
		const TempoDetector::Options & a_Options,
		const PlaybackBuffer & a_Buf,
		const std::vector<std::pair<size_t, qint32>> & a_Beats
	)
	{
		if (a_Options.m_DebugAudioBeatsFileName.isEmpty())
		{
			return;
		}
		QFile f(a_Options.m_DebugAudioBeatsFileName);
		if (!f.open(QIODevice::WriteOnly))
		{
			qDebug() << "Cannot open debug file " << a_Options.m_DebugAudioBeatsFileName;
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
		interlaced.resize(a_Options.m_Stride * 2);
		for (size_t i = 0; i < maxIdx; ++i)
		{
			if (i == a_Beats[lastBeatIdx].first)
			{
				mixStrength = static_cast<qint16>(a_Beats[lastBeatIdx].second * levelCoeff);
				lastBeatIdx += 1;
			}
			auto audio = reinterpret_cast<const qint16 *>(a_Buf.audioData().data()) + i * a_Options.m_Stride;
			for (size_t j = 0; j < a_Options.m_Stride; ++j)
			{
				interlaced[2 * j] = audio[j];
				auto m = static_cast<int>(mixStrength * sin(static_cast<float>(j + a_Options.m_Stride * i) * 2000 / a_Options.m_SampleRate));
				interlaced[2 * j + 1]  = static_cast<qint16>(Utils::clamp<int>(m, -32768, 32767));
			}
			mixStrength = static_cast<qint16>(mixStrength * 0.8);
			f.write(
				reinterpret_cast<const char *>(interlaced.data()),
				static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
			);
		}
	}




protected:

	/** The song to scan. */
	SongPtr m_Song;

	/** The options for the detection. */
	const std::vector<TempoDetector::Options> & m_Options;
};





////////////////////////////////////////////////////////////////////////////////
// TempoDetector:

TempoDetector::TempoDetector():
	m_ScanQueueLength(0)
{
}





std::shared_ptr<TempoDetector::Result> TempoDetector::scanSong(SongPtr a_Song, const std::vector<Options> & a_Options)
{
	Detector d(a_Song, a_Options);
	auto result = d.process();
	emit songScanned(a_Song, result);
	return result;
}





void TempoDetector::queueScanSong(SongPtr a_Song, const std::vector<Options> & a_Options)
{
	auto options = a_Options;  // Make a copy for the background thread
	m_ScanQueueLength += 1;
	BackgroundTasks::enqueue(tr("Detect tempo: %1").arg(a_Song->fileName()),
		[this, a_Song, options]()
		{
			Detector d(a_Song, options);
			auto result = d.process();
			m_ScanQueueLength -= 1;
			emit songScanned(a_Song, result);
		}
	);
}





QString TempoDetector::createTaskName(Song::SharedDataPtr a_SongSD)
{
	assert(a_SongSD != nullptr);
	auto duplicates = a_SongSD->duplicates();
	if (duplicates.empty())
	{
		return tr("Detect tempo: unknown song");
	}
	else
	{
		return tr("Detect tempo: %1").arg(duplicates[0]->fileName());
	}
}





bool TempoDetector::detect(Song::SharedDataPtr a_SongSD)
{
	// Prepare the detection options, esp. the tempo range, if genre is known:
	auto tempoRange = Song::detectionTempoRangeForGenre("unknown");
	auto duplicates = a_SongSD->duplicates();
	for (auto s: duplicates)
	{
		auto genre = s->primaryGenre();
		if (genre.isPresent())
		{
			tempoRange = Song::detectionTempoRangeForGenre(genre.value());
			break;
		}
	}
	auto opts = optimalOptions(tempoRange);

	// Detect from the first available duplicate:
	STOPWATCH("Detect tempo")
	for (auto s: duplicates)
	{
		auto res = scanSong(s->shared_from_this(), opts);
		if (res == nullptr)
		{
			// Reading the audio data failed, try another file (perhaps the file was removed):
			continue;
		}
		if (res->m_Tempo <= 0)
		{
			// Tempo detection failed, reading another file won't help
			break;
		}
		qDebug() << "Detected tempo in " << s->fileName() << ": " << res->m_Tempo;
		a_SongSD->m_DetectedTempo = res->m_Tempo;
		emit songTempoDetected(a_SongSD);
		return true;
	}
	qWarning() << "Cannot detect tempo in song hash " << a_SongSD->m_Hash << ", none of the files provided data for analysis.";
	return false;
}





void TempoDetector::queueDetect(Song::SharedDataPtr a_SongSD)
{
	assert(a_SongSD != nullptr);
	BackgroundTasks::enqueue(
		createTaskName(a_SongSD),
		[a_SongSD, this]()
		{
			detect(a_SongSD);
		}
	);
}





////////////////////////////////////////////////////////////////////////////////
// TempoDetector::Options:

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

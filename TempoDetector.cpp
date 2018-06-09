#include "TempoDetector.h"
#include <QAudioFormat>
#include "BackgroundTasks.h"
#include "Song.h"
#include "AVPP.h"
#include "PlaybackBuffer.h"
#include "Utils.h"





class Detector
{
public:
	Detector(SongPtr a_Song, const TempoDetector::Options & a_Options):
		m_Song(a_Song),
		m_Options(a_Options)
	{
	}





	/** Processes the song - detects its tempo.
	Returns a vector of pairs, tempo -> confidence, for the top tempos detected in the song.
	The caller should process the confidences on their own. */
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
		fmt.setSampleRate(48000);
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
		res->m_Levels = calcLevels(buf);
		debugLevelsInAudioData(buf, res->m_Levels);
		res->m_Beats = detectBeats(res->m_Levels);
		debugBeatsInAudioData(buf, res->m_Beats);
		res->m_Histogram = beatsToHistogram(res->m_Beats);
		if (m_Options.m_ShouldFoldHistogram)
		{
			foldHistogram(res->m_Histogram);
		}
		auto bestTen = sortHistogram(res->m_Histogram, m_Options.m_HistogramCutoff);
		res->m_Confidences = calcConfidences(bestTen);
		if (!res->m_Confidences.empty())
		{
			res->m_Tempo = res->m_Confidences[0].first;
			res->m_Confidence = res->m_Confidences[0].second;
		}
		return res;
	}





	/** Groups the tempos in the histogram by their compatibility and calculates the confidence for each group.
	Returns a vector of <tempo, confidence>, sorted from the highest confidence. */
	std::vector<std::pair<int, int>> calcConfidences(std::vector<std::pair<int, size_t>> a_SortedHistogram)
	{
		std::vector<std::pair<int, size_t>> compatibilityGroups;  // pairs of <baseTempo, sumCount>
		size_t total = 1;  // Don't want zero divisor
		while (!a_SortedHistogram.empty())
		{
			// Find the lowest leftover tempo:
			int lowestTempo = a_SortedHistogram[0].first;
			for (const auto & h: a_SortedHistogram)
			{
				if (h.first < lowestTempo)
				{
					lowestTempo = h.first;
				}
			}

			// Build the compatibility group:
			size_t count = 0;
			for (auto itr = a_SortedHistogram.begin(); itr != a_SortedHistogram.end();)
			{
				if (isCompatibleTempo(lowestTempo, itr->first))
				{
					count += itr->second;
					itr = a_SortedHistogram.erase(itr);
				}
				else
				{
					++itr;
				}
			}
			compatibilityGroups.emplace_back(lowestTempo, count);
			total += count;
		}

		// Convert the compatibility groups' sumCounts into confidences:
		std::vector<std::pair<int, int>> res;
		for (const auto & cg: compatibilityGroups)
		{
			res.emplace_back(
				cg.first,
				static_cast<int>(100 * cg.second / total)
			);
		}

		// Sort the compatibility groups:
		using CgType = decltype(res[0]);
		std::sort(res.begin(), res.end(), [](CgType a_Val1, CgType a_Val2)
			{
				return (a_Val1.second > a_Val2.second);
			}
		);
		return res;
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




	/** Detects the beats within the provided audio levels.
	Returns a vector containing the indices (into a_Levels) where beats have been located. */
	std::vector<size_t> detectBeats(const std::vector<qint32> & a_Levels)
	{
		// Calculate the beats and their weight:
		std::vector<std::pair<size_t, float>> weightedBeats;
		auto maxIdx = a_Levels.size() - m_Options.m_LevelPeak;
		for (size_t i = m_Options.m_LevelPeak; i < maxIdx; ++i)
		{
			bool isLocalMaximum = true;
			qint32 sum = 0;
			for (size_t j = i - m_Options.m_LevelPeak; j < i + m_Options.m_LevelPeak; ++j)
			{
				if ((i != j) && (a_Levels[j] >= a_Levels[i]))
				{
					isLocalMaximum = false;
					break;
				}
				sum += a_Levels[j];
			}
			if (!isLocalMaximum)
			{
				continue;
			}
			weightedBeats.emplace_back(i, static_cast<float>(a_Levels[i]) * m_Options.m_LevelPeak / sum);
		}

		// Prune the beats according to their weight, so that we only have 120 BPM on average:
		using BeatType = decltype(weightedBeats[0]);
		std::sort(weightedBeats.begin(), weightedBeats.end(),
			[](BeatType b1, BeatType b2)
			{
				return (b1.second > b2.second);
			}
		);
		size_t songLengthSec = a_Levels.size() * m_Options.m_Stride / 48000;
		size_t wantedNumBeats = 120 * songLengthSec / 60;
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

		// Output only the beats:
		std::vector<size_t> beats;
		for (const auto & b: weightedBeats)
		{
			beats.push_back(b.first);
		}
		return beats;
	}





	/** Calculates the histogram of beat distances.
	Returns a map of bpm -> number of occurences. */
	std::map<int, size_t> beatsToHistogram(const std::vector<size_t> & a_Beats)
	{
		std::map<int, size_t> beatDiffMap;  // Maps beat time difference -> number of occurrences of such differrence
		auto numBeats = a_Beats.size();
		for (size_t i = 0; i < numBeats; ++i)
		{
			auto start = i - std::min<size_t>(i, 2);
			auto end = std::min(i + 2, numBeats);
			auto currBeat = a_Beats[i];
			for (size_t j = start; j < end; ++j)
			{
				if (i == j)
				{
					continue;
				}
				auto dist = (i > j) ? (currBeat - a_Beats[j]) : (a_Beats[j] - currBeat);
				auto bpm = static_cast<int>(std::round(distToBPM(dist)));
				beatDiffMap[bpm] += 1;
			}
		}
		return beatDiffMap;
	}





	/** "Folds" the histogram:
	Values below m_Options.m_HistogramFoldMin are dropped.
	Values above m_Options.m_HistogramFoldMax are halved until they are lower, then half their weight is added. */
	std::map<int, size_t> foldHistogram(const std::map<int, size_t> & a_Histogram)
	{
		std::map<int, size_t> res;
		for (const auto & v: a_Histogram)
		{
			auto tempo = v.first;
			if (tempo < m_Options.m_HistogramFoldMin)
			{
				continue;
			}
			if (tempo <= m_Options.m_HistogramFoldMax)
			{
				res[tempo] += v.second;
				continue;
			}
			while (tempo > m_Options.m_HistogramFoldMax)
			{
				tempo /= 2;
			}
			res[tempo] += v.second / 2;
		}
		return res;
	}





	/** Sorts the histogram by the counts and returns up to a_Count most common values. */
	std::vector<std::pair<int, size_t>> sortHistogram(const std::map<int, size_t> & a_Histogram, size_t a_Count)
	{
		std::vector<std::pair<int, size_t>> sorted;
		for (const auto & v: a_Histogram)
		{
			sorted.emplace_back(v.first, v.second);
		}
		using itemType = decltype(sorted[0]);
		std::sort(sorted.begin(), sorted.end(), [](itemType v1, itemType v2)
			{
				return (v1.second > v2.second);
			}
		);
		if (a_Count < sorted.size())
		{
			sorted.resize(a_Count);
		}
		return sorted;
	}





	/** Converts the distance between levels to a BPM value. */
	double distToBPM(size_t a_Dist)
	{
		return 60.0 / (m_Options.m_Stride * static_cast<double>(a_Dist) / 48000.0);
	}




	/** Outputs the debug audio data with mixed-in beats. */
	void debugBeatsInAudioData(
		const PlaybackBuffer & a_Buf,
		const std::vector<size_t> & a_Beats
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
		size_t lastBeatIdx = 0;
		size_t maxIdx = a_Beats.back();
		qint16 mixStrength = 0;
		std::vector<qint16> interlaced;
		interlaced.resize(m_Options.m_Stride * 2);
		for (size_t i = 0; i < maxIdx; ++i)
		{
			if (i == a_Beats[lastBeatIdx])
			{
				mixStrength = 32767;
				lastBeatIdx += 1;
			}
			auto audio = reinterpret_cast<const qint16 *>(a_Buf.audioData().data()) + i * m_Options.m_Stride;
			for (size_t j = 0; j < m_Options.m_Stride; ++j)
			{
				interlaced[2 * j] = audio[j];
				auto m = static_cast<int>(mixStrength * sin(static_cast<float>(j + m_Options.m_Stride * i) / 10));
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
	BackgroundTasks::enqueue(tr("Scan metadata: %1").arg(a_Song->fileName()), [this, a_Song, options]()
		{
			Detector d(a_Song, options);
			auto result = d.process();
			m_QueueLength -= 1;
			emit songScanned(a_Song, result);
		}
	);
}





TempoDetector::Options::Options():
	m_LevelAlgorithm(laSumDist),
	m_WindowSize(1024),
	m_Stride(256),
	m_LevelAvg(19),
	m_LevelPeak(19),
	m_HistogramCutoff(10),
	m_ShouldFoldHistogram(true),
	m_HistogramFoldMin(16),
	m_HistogramFoldMax(240)
{
}

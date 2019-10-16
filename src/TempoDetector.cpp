#include "TempoDetector.hpp"
#include <algorithm>
#include <cassert>





/** Clamps the value into the specified range. */
template <typename T>
static T clamp(T aValue, T aMin, T aMax)
{
	if (aValue < aMin)
	{
		return aMin;
	}
	else if (aValue > aMax)
	{
		return aMax;
	}
	else
	{
		return aValue;
	}
}





/** Internal class that implements the entire detection. */
class Detector
{
	/** The array of samples (mono, 16-bit signed, mNumSamples-long). */
	const Int16 * mSamples;

	/** Number of samples in mSamples. */
	size_t mNumSamples;

	/** The detection options. */
	const TempoDetector::Options & mOptions;

	/** The result of the scan, built incrementally. */
	TempoDetector::ResultPtr mResult;


public:

	Detector(const Int16 * aSamples, size_t aNumSamples, const TempoDetector::Options & aOptions):
		mSamples(aSamples),
		mNumSamples(aNumSamples),
		mOptions(aOptions),
		mResult(std::make_shared<TempoDetector::Result>())
	{
		mResult->mOptions = aOptions;
	}





	TempoDetector::ResultPtr scan()
	{
		mResult->mLevels = calcLevels();
		if (mOptions.mShouldNormalizeLevels)
		{
			mResult->mLevels = normalizeLevels(mResult->mLevels);
		}
		mResult->mBeats = detectBeats();
		mResult->mHistogram = beatsToHistogram(mResult->mBeats);
		if (mOptions.mShouldFoldHistogram)
		{
			foldHistogram(mResult->mHistogram);
		}
		auto bestTen = sortHistogram(mResult->mHistogram, mOptions.mHistogramCutoff);
		mResult->mConfidences = calcConfidences(bestTen);
		if (!mResult->mConfidences.empty())
		{
			mResult->mTempo = mResult->mConfidences[0].first;
			mResult->mConfidence = mResult->mConfidences[0].second;
		}
		return mResult;
	}





	/** Groups the tempos in the histogram by their compatibility and calculates the confidence for each group.
	Returns a vector of <tempo, confidence>, sorted from the highest confidence. */
	std::vector<std::pair<int, int>> calcConfidences(std::vector<std::pair<int, size_t>> aSortedHistogram)
	{
		std::vector<std::pair<int, size_t>> compatibilityGroups;  // pairs of <baseTempo, sumCount>
		size_t total = 1;  // Don't want zero divisor
		while (!aSortedHistogram.empty())
		{
			// Find the lowest leftover tempo:
			auto lowestTempo = aSortedHistogram[0].first;
			for (const auto & h: aSortedHistogram)
			{
				if (h.first < lowestTempo)
				{
					lowestTempo = h.first;
				}
			}

			// Build the compatibility group for the lowest tempo:
			size_t count = 0;
			for (auto itr = aSortedHistogram.begin(); itr != aSortedHistogram.end();)
			{
				if (isCompatibleTempo(lowestTempo, itr->first))
				{
					count += itr->second;
					itr = aSortedHistogram.erase(itr);
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
		std::sort(res.begin(), res.end(), [](CgType aVal1, CgType aVal2)
			{
				return (aVal1.second > aVal2.second);
			}
		);
		return res;
	}





	/** Calculates levels of the mSamples data.
	Uses the appropriate algorithm according to mOptions. */
	std::vector<Int32> calcLevels()
	{
		switch (mOptions.mLevelAlgorithm)
		{
			case TempoDetector::laSumDist:               return calcLevelsSumDist();
			case TempoDetector::laDiscreetSineTransform: return calcLevelsDST();
			case TempoDetector::laMinMax:                return calcLevelsMinMax();
			case TempoDetector::laSumDistMinMax:         return calcLevelsSumDistMinMax();
		}
		assert(!"Invalid level algorithm");
		return {};
	}





	/** Calculates the loudness levels from the input mSamples data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<Int32> calcLevelsSumDist()
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / mOptions.mStride);

		// Calculate the first window:
		Int32 current = 0;
		for (size_t i = 0; i < mOptions.mWindowSize; ++i)
		{
			current += std::abs(mSamples[i] - mSamples[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = mNumSamples - mOptions.mWindowSize - mOptions.mStride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += mOptions.mStride)
		{
			for (size_t i = 0; i < mOptions.mStride; ++i)
			{
				current += std::abs(mSamples[curPos + i + mOptions.mWindowSize] - mSamples[curPos + i + mOptions.mWindowSize + 1]);
				current -= std::abs(mSamples[curPos + i] - mSamples[curPos + i + 1]);
			}
			res.push_back(current);
		}
		return res;
	}




	/** Calculates the loudness levels from the input mSamples data.
	Combines the SumDist and MinMax methods into a single number. */
	std::vector<Int32> calcLevelsSumDistMinMax()
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / mOptions.mStride);

		// Calculate the first window:
		Int32 current = 0;
		for (size_t i = 0; i < mOptions.mWindowSize; ++i)
		{
			current += std::abs(mSamples[i] - mSamples[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = mNumSamples - mOptions.mWindowSize - mOptions.mStride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += mOptions.mStride)
		{
			for (size_t i = 0; i < mOptions.mStride; ++i)
			{
				current += std::abs(mSamples[curPos + i + mOptions.mWindowSize] - mSamples[curPos + i + mOptions.mWindowSize + 1]);
				current -= std::abs(mSamples[curPos + i] - mSamples[curPos + i + 1]);
			}
			auto minVal = mSamples[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < mOptions.mWindowSize; ++i)
			{
				auto val = mSamples[curPos + i];
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




	/** Calculates the loudness levels from the input mSamples data.
	The level is calculated as the difference between the max and min value in the sample range. */
	std::vector<Int32> calcLevelsMinMax()
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / mOptions.mStride);

		// Calculate each window:
		size_t maxPos = mNumSamples - mOptions.mWindowSize - mOptions.mStride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += mOptions.mStride)
		{
			auto minVal = mSamples[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < mOptions.mWindowSize; ++i)
			{
				auto val = mSamples[curPos + i];
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




	/** Calculates the loudness levels from the input mSamples data, using discreet sine transform.
	The idea is that a change in what we think of as levels should be across all frequencies.
	So we sample a few frequency bands and calculate the levels from those.
	Doesn't seem to work so well as the Simple method. */
	std::vector<Int32> calcLevelsDST()
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / mOptions.mStride);

		// Calculate each window's level:
		size_t maxPos = mNumSamples - mOptions.mWindowSize - mOptions.mStride - 1;
		static const float freq[] = {
			600,  //   80 Hz
			250,  //  192 Hz
			109,  //  440 Hz
			48,   // 1000 Hz
		};
		static const size_t NUM_FREQ = sizeof(freq) / sizeof(*freq);
		float Int32Min = static_cast<float>(std::numeric_limits<Int32>::min());
		float Int32Max = static_cast<float>(std::numeric_limits<Int32>::max());
		for (size_t curPos = 0; curPos < maxPos; curPos += mOptions.mStride)
		{
			float freqCoeff[NUM_FREQ] = {};
			for (size_t i = 0; i < mOptions.mWindowSize; ++i)
			{
				auto sampleIndex = i + curPos;
				auto sampleIndexF = static_cast<float>(i + curPos);
				auto audioSample = mSamples[sampleIndex];
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
			res.push_back(static_cast<Int32>(clamp(level, Int32Min, Int32Max)));
		}
		return res;
	}





	/** Normalizes the input levels across the m_NormalizeLevelsWindowSize neighbors. */
	std::vector<Int32> normalizeLevels(
		const std::vector<Int32> & aLevels
	)
	{
		// Normalize against the local min / max
		std::vector<Int32> normalizedLevels;
		auto count = aLevels.size();
		normalizedLevels.resize(count);
		auto win = mOptions.mNormalizeLevelsWindowSize;
		for (size_t i = 0; i < count; ++i)
		{
			size_t startIdx = (i < win) ? 0 : i - win;
			size_t endIdx = std::min(i + win, count);
			auto lMin = aLevels[startIdx];
			auto lMax = aLevels[startIdx];
			for (size_t j = startIdx + 1; j < endIdx; ++j)
			{
				if (aLevels[j] < lMin)
				{
					lMin = aLevels[j];
				}
				else if (aLevels[j] > lMax)
				{
					lMax = aLevels[j];
				}
			}
			normalizedLevels[i] = static_cast<Int32>(65536.0f * (aLevels[i] - lMin) / (lMax - lMin + 1));
		}
		return normalizedLevels;

		/*
		// Normalize against the local average
		std::vector<Int32> normalizedLevels;
		normalizedLevels.resize(aLevels.size());
		Int32 sum = 0;
		for (size_t i = 0; i < mOptions.m_NormalizeLevelsWindowSize; ++i)
		{
			sum += aLevels[i];
			normalizedLevels[i] = 0;
		}
		size_t maxIdx = aLevels.size() - mOptions.m_NormalizeLevelsWindowSize;
		size_t halfSize = mOptions.m_NormalizeLevelsWindowSize / 2;
		for (size_t i = 0; i < maxIdx; ++i)
		{
			normalizedLevels[i + halfSize] = static_cast<Int16>(Utils::clamp<Int32>(
				static_cast<Int32>(327670.0f * aLevels[i + halfSize] / sum), -32768, 32767
			));
			sum = sum + aLevels[i + mOptions.m_NormalizeLevelsWindowSize] - aLevels[i];
		}
		for (size_t i = maxIdx; i < aLevels.size(); ++i)
		{
			normalizedLevels[i] = 0;
		}
		return normalizedLevels;
		*/
	}




	/** Detects the beats within the provided mSamples levels.
	Returns a vector containing the indices (into aLevels) where beats have been located. */
	std::vector<std::pair<size_t, Int32>> detectBeats()
	{
		// Calculate the climbing tendencies in levels:
		std::vector<Int32> climbs;
		const auto & levels = mResult->mLevels;
		auto count = levels.size();
		climbs.reserve(count);
		climbs.push_back(0);
		Int32 maxClimb = 0;
		auto maxLevel = levels[0];
		for (size_t i = 1; i < count; ++i)
		{
			auto c = std::max(0, levels[i] - levels[i - 1]);
			climbs.push_back(c);
			if (c > maxClimb)
			{
				maxClimb = c;
			}
			if (levels[i] > maxLevel)
			{
				maxLevel = levels[i];
			}
		}

		// Calculate beats and their weight:
		std::vector<std::pair<size_t, Int32>> weightedBeats;
		auto maxIdx = levels.size() - mOptions.mLevelPeak;
		for (size_t i = mOptions.mLevelPeak; i < maxIdx; ++i)
		{
			bool isLocalMaximum = true;
			for (size_t j = i - mOptions.mLevelPeak; j < i + mOptions.mLevelPeak; ++j)
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
			[](auto b1, auto b2)
			{
				return (b1.second > b2.second);
			}
		);
		size_t songLengthSec = levels.size() * mOptions.mStride / static_cast<size_t>(mOptions.mSampleRate);
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
		const std::vector<std::pair<size_t, Int32>> & a_Beats
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





	/** "Folds" the histogram:
	Values below mOptions.m_HistogramFoldMin are dropped.
	Values above mOptions.m_HistogramFoldMax are halved until they are lower, then half their weight is added. */
	std::map<int, size_t> foldHistogram(const std::map<int, size_t> & a_Histogram)
	{
		std::map<int, size_t> res;
		for (const auto & v: a_Histogram)
		{
			auto tempo = v.first;
			if (tempo < mOptions.mHistogramFoldMin)
			{
				continue;
			}
			if (tempo <= mOptions.mHistogramFoldMax)
			{
				res[tempo] += v.second;
				continue;
			}
			while (tempo > mOptions.mHistogramFoldMax)
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
		return 60.0 / (mOptions.mStride * static_cast<double>(a_Dist) / mOptions.mSampleRate);
	}




	/** Calculates the confidence in the detected tempo, from the histogram. */
	int calcConfidence(const std::vector<std::pair<int, size_t>> & aBeatHistogram)
	{
		if (aBeatHistogram.empty())
		{
			return 0;
		}
		auto finalGuess = aBeatHistogram[0].first;
		size_t pro = 0, against = 0;
		for (const auto & h: aBeatHistogram)
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
};





////////////////////////////////////////////////////////////////////////////////
// TempoDetector:

TempoDetector::ResultPtr TempoDetector::scan(const Int16 * aSamples, size_t aNumSamples, const TempoDetector::Options & aOptions)
{
	Detector d(aSamples, aNumSamples, aOptions);
	return d.scan();
}





////////////////////////////////////////////////////////////////////////////////
// TempoDetector::Options:

TempoDetector::Options::Options():
	mSampleRate(48000),
	mLevelAlgorithm(laSumDist),
	mWindowSize(1024),
	mStride(256),
	mLevelPeak(13),
	mHistogramCutoff(10),
	mShouldFoldHistogram(true),
	mHistogramFoldMin(16),
	mHistogramFoldMax(240),
	mShouldNormalizeLevels(true),
	mNormalizeLevelsWindowSize(31)
{
}





bool TempoDetector::Options::operator < (const TempoDetector::Options & a_Other)
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

	COMPARE(mSampleRate)
	COMPARE(mLevelAlgorithm)
	COMPARE(mWindowSize)
	COMPARE(mStride)
	COMPARE(mLevelPeak)
	COMPARE(mHistogramCutoff)
	COMPARE(mShouldFoldHistogram)
	if (mShouldFoldHistogram)
	{
		COMPARE(mHistogramFoldMin)
		COMPARE(mHistogramFoldMax)
	}
	COMPARE(mShouldNormalizeLevels)
	if (mShouldNormalizeLevels)
	{
		COMPARE(mNormalizeLevelsWindowSize);
	}
	return false;
}





bool TempoDetector::Options::operator == (const TempoDetector::Options & aOther)
{
	if (
		mShouldFoldHistogram &&
		(
			(mHistogramFoldMin != aOther.mHistogramFoldMin) ||
			(mHistogramFoldMax != aOther.mHistogramFoldMax)
		)
	)
	{
		return false;
	}
	return (
		(mLevelAlgorithm == aOther.mLevelAlgorithm) &&
		(mWindowSize == aOther.mWindowSize) &&
		(mStride == aOther.mStride) &&
		(mLevelPeak == aOther.mLevelPeak) &&
		(mHistogramCutoff == aOther.mHistogramCutoff) &&
		(mShouldFoldHistogram == aOther.mShouldFoldHistogram)
	);
}

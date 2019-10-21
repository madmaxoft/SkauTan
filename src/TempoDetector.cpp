#include "TempoDetector.hpp"
#include <cassert>
#include <algorithm>





/** Returns the value, clamped to the range provided. */
template <typename T> T clamp(T aValue, T aMin, T aMax)
{
	if (aValue < aMin)
	{
		return aMin;
	}
	if (aValue > aMax)
	{
		return aMax;
	}
	return aValue;
}





/** Performs floor and static_cast at the same time. */
template <typename T> T floor(double aValue)
{
	return static_cast<T>(std::floor(aValue));
}





/** Performs ceil and static_cast at the same time. */
template <typename T> T ceil(double aValue)
{
	return static_cast<T>(std::ceil(aValue));
}





////////////////////////////////////////////////////////////////////////////////
// Detector:

/** The actual tempo detector. The TempoDetector interface class uses this to detect the tempo. */
class Detector
{
public:
	Detector(
		const Int16 * aSamples,
		size_t aNumSamples,
		const TempoDetector::Options & aOptions
	):
		mSamples(aSamples),
		mNumSamples(aNumSamples),
		mOptions(aOptions)
	{
	}





	/** Processes the mSamples data - detects its tempo. */
	std::shared_ptr<TempoDetector::Result> process()
	{
		// Run the mSamples through the detector:
		auto res = std::make_shared<TempoDetector::Result>();
		res->mOptions = mOptions;
		res->mLevels = calcLevels(mOptions);
		if (mOptions.mShouldNormalizeLevels)
		{
			res->mLevels = normalizeLevels(mOptions, res->mLevels);
		}
		res->mBeats = detectBeats(mOptions, res->mLevels);
		if (!res->mBeats.empty())
		{
			std::tie(res->mTempo, res->mConfidence) = detectTempoFromBeats(mOptions, res->mBeats, res->mLevels);
		}
		return res;
	}





	/** Returns true if the two tempos are compatible - close enough to each other. */
	static bool isCompatibleTempo(double aTempo1, double aTempo2)
	{
		return (std::abs(aTempo1 - aTempo2) < 1.2);
	}





	/** Calculates the best match for the tempo, and its confidence, based on the detected beats.
	Uses self-similarity matching to calculate the confidence for each tempo value.
	The returned confidence ranges from 0 to 100. */
	std::pair<double, double> detectTempoFromBeats(
		const TempoDetector::Options & aOptions,
		std::vector<std::pair<size_t, Int32>> & aBeats,
		std::vector<Int32> & aLevels
	)
	{
		// Calculate the similarities:
		Int32 maxLevel = 0;
		auto beats = prepareBeats(aBeats, aLevels, maxLevel);
		auto minOfs = floor<size_t>(mpmToBeatStrides(aOptions, aOptions.mMaxTempo));
		auto maxOfs = ceil<size_t> (mpmToBeatStrides(aOptions, aOptions.mMinTempo));
		std::vector<std::pair<double, double>> similarities;  // {similarity, mpm}
		for (size_t ofs = minOfs; ofs <= maxOfs; ++ofs)
		{
			auto sim = calcSimilarityBeatsWeight(aBeats, beats, ofs, maxLevel);
			auto mpm = beatStridesToBpm(aOptions, ofs);
			similarities.push_back({sim, mpm});
		}
		if (similarities.empty())
		{
			return {0, 0};
		}

		// Find the second-best MPM (that isn't close enough to the best MPM):
		std::sort(similarities.begin(), similarities.end(),
			[](auto aSim1, auto aSim2)
			{
				return (aSim1.first > aSim2.first);
			}
		);
		auto bestMpm = similarities[0].second;
		auto bestSimilarity = similarities[0].first;
		if (bestSimilarity < 1)
		{
			return {0, 0};
		}
		for (const auto & sim: similarities)
		{
			if (!isCloseEnoughMpm(sim.second, bestMpm))
			{
				auto confidence = 100 - 100 * sim.first / bestSimilarity;
				return {bestMpm, clamp<double>(confidence, 0, 100)};
			}
		}
		return {0, 0};
	}




	/** Returns true if the two MPMs are close enough to be considered a single match. */
	static bool isCloseEnoughMpm(double aMpm1, double aMpm2)
	{
		return (std::abs(aMpm1 - aMpm2) <= 1.5);
	}





	/** Calculates the self-similarity on aBeats using the specified offset.
	aBeatMap is the helper map for quick beat lookups, created by prepareBeats().
	The similarity is a number that can be compared to other similarities. */
	double calcSimilarityBeatsWeight(
		const std::vector<std::pair<size_t, Int32>> & aBeats,
		const std::map<size_t, Int32> & aBeatMap,
		size_t aOffset,
		Int32 aMaxLevel
	)
	{
		double res = 0;
		for (const auto & beat: aBeats)
		{
			auto idx = beat.first + aOffset;
			auto itr = aBeatMap.find(idx);
			if (itr != aBeatMap.cend())
			{
				res += aMaxLevel - std::abs(beat.second - itr->second);
				continue;
			}
			itr = aBeatMap.find(idx + 1);
			if (itr != aBeatMap.cend())
			{
				res += aMaxLevel - std::abs(beat.second - itr->second) / 2;
				continue;
			}
			itr = aBeatMap.find(idx - 1);
			if (itr != aBeatMap.cend())
			{
				res += aMaxLevel - std::abs(beat.second - itr->second) / 2;
				continue;
			}
		}
		return res;
	}





	/** Prepares a lookup-table of beats based on the input calculated beats.
	Returns a map of beatIndex -> soundLevel for each detected beat.
	Cuts 10 % off the beginning and 10 % off the end of the song, timewise. */
	std::map<size_t, Int32> prepareBeats(
		const std::vector<std::pair<size_t, Int32>> & aBeats,
		const std::vector<Int32> & aLevels,
		Int32 & aOutMaxLevel
	)
	{
		std::map<size_t, Int32> res;
		auto minIdx = static_cast<size_t>(aBeats.back().first * 0.1);
		auto maxIdx = static_cast<size_t>(aBeats.back().first * 0.9);
		Int32 maxLevel = 0;
		for (const auto & beat: aBeats)
		{
			if ((beat.first < minIdx) || (beat.first > maxIdx))
			{
				continue;
			}
			auto level = std::abs(aLevels[beat.first]);
			res[beat.first] = level;
			if (level > maxLevel)
			{
				maxLevel = level;
			}
		}
		aOutMaxLevel = maxLevel;
		return res;
	}





	/** Converts the number of strides (across calculated beats) that make up a time interval,
	into the corresponding BPM, rounded to 2 decimal places.
	The inverse function is mpmToBeatStrides() */
	double beatStridesToBpm(
		const TempoDetector::Options & aOptions,
		size_t aBeatStrides
	)
	{
		auto bpm = static_cast<double>(aOptions.mSampleRate * 60 / static_cast<int>(aOptions.mStride)) / aBeatStrides;
		return std::floor(bpm * 100 + 0.5) / 100;
	}





	/** Converts the MPM to the stride length (across calculated beats) representing the tempo. */
	double mpmToBeatStrides(const TempoDetector::Options & aOptions, double aMpm)
	{
		return ((aOptions.mSampleRate * 60 / static_cast<int>(aOptions.mStride)) / aMpm);
	}





	/** Calculates levels of the mSamples data.
	Uses the appropriate algorithm according to mOptions. */
	std::vector<Int32> calcLevels(const TempoDetector::Options & aOptions)
	{
		switch (aOptions.mLevelAlgorithm)
		{
			case TempoDetector::laSumDist:               return calcLevelsSumDist      (aOptions);
			case TempoDetector::laDiscreetSineTransform: return calcLevelsDST          (aOptions);
			case TempoDetector::laMinMax:                return calcLevelsMinMax       (aOptions);
			case TempoDetector::laSumDistMinMax:         return calcLevelsSumDistMinMax(aOptions);
		}
		assert(!"Invalid level algorithm");
		return {};
	}





	/** Calculates the loudness levels from the input mSamples data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<Int32> calcLevelsSumDist(const TempoDetector::Options & aOptions)
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / aOptions.mStride);

		// Calculate the first window:
		Int32 current = 0;
		for (size_t i = 0; i < aOptions.mWindowSize; ++i)
		{
			current += std::abs(mSamples[i] - mSamples[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = mNumSamples - aOptions.mWindowSize - aOptions.mStride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += aOptions.mStride)
		{
			for (size_t i = 0; i < aOptions.mStride; ++i)
			{
				current += std::abs(mSamples[curPos + i + aOptions.mWindowSize] - mSamples[curPos + i + aOptions.mWindowSize + 1]);
				current -= std::abs(mSamples[curPos + i] - mSamples[curPos + i + 1]);
			}
			res.push_back(current);
		}
		return res;
	}




	/** Calculates the loudness levels from the input mSamples data.
	This simply assumes that the more the waveform changes, the louder it is, hence the level is just
	a sum of the distances between neighboring samples across the window.*/
	std::vector<Int32> calcLevelsSumDistMinMax(const TempoDetector::Options & aOptions)
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / aOptions.mStride);

		// Calculate the first window:
		Int32 current = 0;
		for (size_t i = 0; i < aOptions.mWindowSize; ++i)
		{
			current += std::abs(mSamples[i] - mSamples[i + 1]);
		}
		res.push_back(current);

		// Calculate the next windows, relatively to the current one:
		size_t maxPos = mNumSamples - aOptions.mWindowSize - aOptions.mStride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += aOptions.mStride)
		{
			for (size_t i = 0; i < aOptions.mStride; ++i)
			{
				current += std::abs(mSamples[curPos + i + aOptions.mWindowSize] - mSamples[curPos + i + aOptions.mWindowSize + 1]);
				current -= std::abs(mSamples[curPos + i] - mSamples[curPos + i + 1]);
			}
			auto minVal = mSamples[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < aOptions.mWindowSize; ++i)
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
	std::vector<Int32> calcLevelsMinMax(const TempoDetector::Options & aOptions)
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / aOptions.mStride);

		// Calculate each window:
		size_t maxPos = mNumSamples - aOptions.mWindowSize - aOptions.mStride - 1;
		for (size_t curPos = 0; curPos < maxPos; curPos += aOptions.mStride)
		{
			auto minVal = mSamples[curPos];
			auto maxVal = minVal;
			for (size_t i = 0; i < aOptions.mWindowSize; ++i)
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
	std::vector<Int32> calcLevelsDST(const TempoDetector::Options & aOptions)
	{
		std::vector<Int32> res;
		res.reserve(mNumSamples / aOptions.mStride);

		// Calculate each window's level:
		size_t maxPos = mNumSamples - aOptions.mWindowSize - aOptions.mStride - 1;
		static const float freq[] = {
			600,  //   80 Hz
			250,  //  192 Hz
			109,  //  440 Hz
			48,   // 1000 Hz
		};
		static const size_t NUM_FREQ = sizeof(freq) / sizeof(*freq);
		float Int32Min = static_cast<float>(std::numeric_limits<Int32>::min());
		float Int32Max = static_cast<float>(std::numeric_limits<Int32>::max());
		for (size_t curPos = 0; curPos < maxPos; curPos += aOptions.mStride)
		{
			float freqCoeff[NUM_FREQ] = {};
			for (size_t i = 0; i < aOptions.mWindowSize; ++i)
			{
				auto sampleIndex = i + curPos;
				auto sampleIndexF = static_cast<float>(i + curPos);
				auto mSamplesSample = mSamples[sampleIndex];
				for (size_t f = 0; f < NUM_FREQ; ++f)
				{
					freqCoeff[f] += mSamplesSample * sin(sampleIndexF / freq[f]);
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




	/** Normalizes the input levels across the mNormalizeLevelsWindowSize neighbors. */
	std::vector<Int32> normalizeLevels(
		const TempoDetector::Options & aOptions,
		const std::vector<Int32> & aLevels
	)
	{
		// Normalize against the local min / max
		std::vector<Int32> normalizedLevels;
		auto count = aLevels.size();
		normalizedLevels.resize(count);
		auto win = aOptions.mNormalizeLevelsWindowSize;
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
	}




	/** Detects the beats within the provided mSamples levels.
	Returns a vector containing the indices (into aLevels) where beats have been located. */
	std::vector<std::pair<size_t, Int32>> detectBeats(
		const TempoDetector::Options & aOptions,
		const std::vector<Int32> & aLevels
	)
	{
		// Calculate the climbing tendencies in aLevels:
		std::vector<Int32> climbs;
		auto count = aLevels.size();
		climbs.reserve(count);
		climbs.push_back(0);
		Int32 maxClimb = 0;
		auto maxLevel = aLevels[0];
		for (size_t i = 1; i < count; ++i)
		{
			auto c = std::max(0, aLevels[i] - aLevels[i - 1]);
			climbs.push_back(c);
			if (c > maxClimb)
			{
				maxClimb = c;
			}
			if (aLevels[i] > maxLevel)
			{
				maxLevel = aLevels[i];
			}
		}

		// Calculate beats and their weight:
		std::vector<std::pair<size_t, Int32>> weightedBeats;
		auto maxIdx = aLevels.size() - aOptions.mLocalMaxDistance;
		for (size_t i = aOptions.mLocalMaxDistance; i < maxIdx; ++i)
		{
			bool isLocalMaximum = true;
			for (size_t j = i - aOptions.mLocalMaxDistance; j < i + aOptions.mLocalMaxDistance; ++j)
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
		size_t songLengthSec = aLevels.size() * aOptions.mStride / static_cast<size_t>(aOptions.mSampleRate);
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


protected:

	/** The mSamples samples to scan (16-bit signed mono, mNumSamples-length). */
	const Int16 * const mSamples;

	/** Number of samples in mSamples; */
	const size_t mNumSamples;

	/** The options for the detection. */
	const TempoDetector::Options & mOptions;
};





////////////////////////////////////////////////////////////////////////////////
// TempoDetector:

TempoDetector::ResultPtr TempoDetector::scan(
	const Int16 * aSamples,
	size_t aNumSamples,
	const Options & aOptions
)
{
	Detector d(aSamples, aNumSamples, aOptions);
	return d.process();
}





std::pair<double, double> TempoDetector::aggregateResults(const std::vector<TempoDetector::ResultPtr> & aResults)
{
	assert(!aResults.empty());
	// If there's just one result, use it directly:
	if (aResults.size() == 1)
	{
		return {aResults[0]->mTempo, aResults[0]->mConfidence};
	}

	// For multiple results, aggregate the compatible tempos and their confidences:
	std::vector<std::pair<double, double>> processedTempos;
	for (const auto & tin: aResults)
	{
		bool hasFound = false;
		for (auto & tout: processedTempos)
		{
			if (Detector::isCompatibleTempo(tout.first, tin->mTempo))
			{
				// There's a compatible tempo already in the processedTempos, add my confidence to it:
				tout.second += tin->mConfidence;
				hasFound = true;
				break;
			}
		}
		if (!hasFound)
		{
			processedTempos.emplace_back(tin->mTempo, tin->mConfidence);
		}
	}  // for tin: aTempos[]
	assert(!processedTempos.empty());
	if (processedTempos.size() == 1)
	{
		return processedTempos[0];
	}

	// Pick the tempos with the highest sum of confidence, then use the confidences as weights for averaging the contributing tempos:
	std::sort(processedTempos.begin(), processedTempos.end(),
		[](auto aTempo1, auto aTempo2)
		{
			return (aTempo1.second > aTempo2.second);
		}
	);
	auto pickedTempo = processedTempos[0];
	double sumTempo = 0, sumConfidence = 0;
	double sumNegConfidence = 0;  // Sum of confidences for tempos NOT matching
	for (const auto & tin: aResults)
	{
		if (Detector::isCompatibleTempo(tin->mTempo, pickedTempo.first))
		{
			sumTempo += tin->mTempo * tin->mConfidence;
			sumConfidence += tin->mConfidence;
		}
		else
		{
			sumNegConfidence += tin->mConfidence;
		}
	}
	auto res = std::floor(sumTempo / sumConfidence * 10 + 0.5) / 10;  // keep only 1 decimal digit
	return {res, 100 * sumConfidence / (sumConfidence + sumNegConfidence)};
}





////////////////////////////////////////////////////////////////////////////////
// TempoDetector::Options:

TempoDetector::Options::Options():
	mSampleRate(500),
	mLevelAlgorithm(laSumDistMinMax),
	mWindowSize(11),
	mStride(8),
	mLocalMaxDistance(13),
	mShouldNormalizeLevels(true),
	mNormalizeLevelsWindowSize(31),
	mMaxTempo(200),
	mMinTempo(15)
{
}





bool TempoDetector::Options::operator <(const TempoDetector::Options & aOther)
{
	#define COMPARE(val) \
		if (val < aOther.val) \
		{ \
			return true; \
		} \
		if (val > aOther.val) \
		{ \
			return false; \
		}

	COMPARE(mSampleRate)
	COMPARE(mLevelAlgorithm)
	COMPARE(mWindowSize)
	COMPARE(mStride)
	COMPARE(mLocalMaxDistance)
	COMPARE(mShouldNormalizeLevels)
	if (mShouldNormalizeLevels)
	{
		COMPARE(mNormalizeLevelsWindowSize);
	}
	COMPARE(mMaxTempo);
	COMPARE(mMinTempo);
	return false;
}





bool TempoDetector::Options::operator ==(const TempoDetector::Options & aOther)
{
	return (
		(mSampleRate == aOther.mSampleRate) &&
		(mLevelAlgorithm == aOther.mLevelAlgorithm) &&
		(mWindowSize == aOther.mWindowSize) &&
		(mStride == aOther.mStride) &&
		(mLocalMaxDistance == aOther.mLocalMaxDistance) &&
		(mShouldNormalizeLevels == aOther.mShouldNormalizeLevels) &&
		(mNormalizeLevelsWindowSize == aOther.mNormalizeLevelsWindowSize) &&
		(mMaxTempo == aOther.mMaxTempo) &&
		(mMinTempo == aOther.mMinTempo)
	);
}

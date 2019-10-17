#pragma once

#include <memory>
#include <vector>
#include <map>





using Int16 = int16_t;
using Int32 = int32_t;
using UInt32 = uint32_t;





/** Implements the tempo detection using several different algorithms.
A single scan yields a Result object.
Results from multiple scans can be aggregated together into a single value. */
class TempoDetector
{
public:

	/** Specifies the algorithm to use for detecting levels in the audio. */
	enum ELevelAlgorithm
	{
		laSumDist,                ///< level = sum(abs(sample - prevSample))
		laMinMax,                 ///< level = maxSample - minSample
		laDiscreetSineTransform,  ///< level = sum(abs(DST in several frequency bands))
		laSumDistMinMax,          ///< Combination of laSumDist and laMinMax
	};



	/** Holds the options for a single detection. */
	struct Options
	{
		/** The sample rate to which the song is converted before detecting. */
		int mSampleRate;

		/** The algorithm to use for detecting loudness levels in the audio. */
		ELevelAlgorithm mLevelAlgorithm;

		/** The number of audio samples to process for one levels sample. */
		size_t mWindowSize;

		/** The distance between successive levels, in samples. */
		size_t mStride;

		/** The number of levels to check before and after current level to filter out non-peak levels. */
		size_t mLocalMaxDistance;

		/** If true, the levels are normalized across mNormalizeLevelsWindowSize elements. */
		bool mShouldNormalizeLevels;

		/** Number of neighboring levels to normalize against when mShouldNormalizeLevels is true. */
		size_t mNormalizeLevelsWindowSize;

		/** The maximum tempo that is tested in the detection. */
		int mMaxTempo;

		/** The minimum tempo that is tested in the detection. */
		int mMinTempo;

		Options(const Options &) = default;
		Options(Options &&) = default;
		Options();

		bool operator < (const Options & a_Other);
		bool operator ==(const Options & a_Other);
		Options & operator = (const Options & a_Other) = default;
	};



	/** Holds the calculated result of a single detection. */
	struct Result
	{
		/** The options used to calculate this result. */
		Options mOptions;

		/** The final detected tempo (BPM). negative if not detected. */
		double mTempo;

		/** The confidence of the detection. Ranges from 0 to 100, higher means more confident. */
		double mConfidence;

		/** A time-sorted vector of beat indices (into mLevels) and their weight.
		Only contains the beats from the last mOptions item. */
		std::vector<std::pair<size_t, Int32>> mBeats;

		/** A vector of all levels calculated for the audio.
		Only contains the levels from the last mOptions item. */
		std::vector<Int32> mLevels;


		/** Creates an "invalid" result - no confidence, no detected tempo. */
		Result():
			mTempo(-1),
			mConfidence(0)
		{
		}
	};

	using ResultPtr = std::shared_ptr<Result>;



	/** Scans the specified audiodata synchronously, using the specified options.
	aSamples is a pointer to a contiguous block of 16-bit mono samples, aNumSamples in length.
	The samples are interpreted as having the samplerate of aOptions.mSampleRate.
	Results from multiple scans over the same song may be aggregated with aggregateResults(). */
	static ResultPtr scan(const Int16 * aSamples, size_t aNumSamples, const Options & aOptions);

	/** Aggregates results from multiple scans (using varying options) into a single tempo + confidence value.
	Returns the tempo (BPM) and confidence (0 .. 100; higher means more confident). */
	static std::pair<double, double> aggregateResults(const std::vector<ResultPtr> & aResults);
};

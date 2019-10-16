#pragma once

#include <memory>
#include <vector>
#include <map>
#include <atomic>
#include <string>





using Int16 = int16_t;
using Int32 = int32_t;
using UInt32 = uint32_t;





/** Implements the tempo detection using several different algorithms.
The scan finishes with a Result object that contains the calculated values, it is up to the caller to
process the results and potentially set them into the song metadata, display to the user etc.
This is a reusable class that has no dependencies on external libs (Qt, LibAV etc). */
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



	/** Holds the options for the detection. */
	struct Options
	{
		/** The sample rate at which the samples are delivered.
		Used mainly to limit BPM to reasonable values. */
		int mSampleRate;

		/** The algorithm to use for detecting loudness levels in the audio. */
		ELevelAlgorithm mLevelAlgorithm;

		/** The number of samples to process for one levels sample. */
		size_t mWindowSize;

		/** The distance between successive levels, in samples. */
		size_t mStride;

		/** The number of levels to check before and after current level to filter out non-peak levels. */
		size_t mLevelPeak;

		/** Number of histogram entries that are used for the confidence calculation. */
		size_t mHistogramCutoff;

		/** If set to true, the histogram is folded before calculating confidences.
		Tempos lower than mHistogramFoldMin are dropped.
		Tempos higher than mHistogramFoldMax are halved until they are lower, then half their count is added. */
		bool mShouldFoldHistogram;

		/** The minimum tempo to keep when folding the histogram.
		Tempos lower than this are dropped from the histogram. */
		int mHistogramFoldMin;

		/** The maximum tempo to keep when folding the histogram.
		Tempos higher than this are halved until they are lower, then half their count is added. */
		int mHistogramFoldMax;

		/** If true, the levels are normalized across mNormalizeLevelsWindowSize elements. */
		bool mShouldNormalizeLevels;

		/** Number of neighboring levels to normalize against when mShouldNormalizeLevels is true. */
		size_t mNormalizeLevelsWindowSize;

		Options(const Options &) = default;
		Options(Options &&) = default;
		Options();

		bool operator < (const Options & a_Other);
		bool operator == (const Options & a_Other);
		Options & operator = (const Options & a_Other) = default;
	};



	/** Holds the calculated result of the detection. */
	struct Result
	{
		/** The options used to calculate this result. */
		Options mOptions;

		/** The detected tempo. */
		int mTempo;

		/** The confidence of the detection. Ranges from 0 to 100, higher means more confident. */
		int mConfidence;

		/** A confidence-desc-sorted vector of tempo -> confidence.
		The first item corresponds to mTempo, mConfidence. */
		std::vector<std::pair<int, int>> mConfidences;

		/** The raw histogram of tempo -> number of occurences. */
		std::map<int, size_t> mHistogram;

		/** A sorted vector of beat indices into mLevels and their weight. */
		std::vector<std::pair<size_t, Int32>> mBeats;

		/** A vector of all levels calculated for the audio. */
		std::vector<Int32> mLevels;
	};

	using ResultPtr = std::shared_ptr<Result>;


	/** Scans the specified sound data and returns the analysis result.
	The aSamples is a pointer to a contiguous array of mono 16-bit signed samples at 500Hz samplerate.
	aNumSamples is the number of samples to read from a_Samples. */
	static ResultPtr scan(const Int16 * aSamples, size_t aNumSamples, const Options & aOptions = Options());
};

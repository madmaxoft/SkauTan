#ifndef TEMPODETECTOR_H
#define TEMPODETECTOR_H




#include <memory>
#include <atomic>
#include <QObject>





// fwd:
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Implements the tempo detection using several different algorithms.
Songs can be scanned synchronously or asynchronously using BackgroundTasks.
The scan finishes with a Result object that contains the calculated values, it is up to the caller to
process the results and potentially set them into the song metadata, display to the user etc. */
class TempoDetector:
	public QObject
{
	Q_OBJECT
	using Super = QObject;


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
		/** The sample rate to which the song is converted before detecting. */
		int m_SampleRate;

		/** The algorithm to use for detecting loudness levels in the audio. */
		ELevelAlgorithm m_LevelAlgorithm;

		/** The length of audio data to process for one levels sample. */
		size_t m_WindowSize;

		/** The distance between successive levels, in samples. */
		size_t m_Stride;

		/** The number of levels to check before and after current level to filter out non-peak levels. */
		size_t m_LevelPeak;

		/** If true, the levels are normalized across m_NormalizeLevelsWindowSize elements. */
		bool m_ShouldNormalizeLevels;

		/** Number of neighboring levels to normalize against when m_ShouldNormalizeLevels is true. */
		size_t m_NormalizeLevelsWindowSize;

		/** Name of file to store the audio levels for debugging purposes.
		Creates a 16-bit int stereo RAW file,
		left channel contains the original audiodata, right channel contains level values.
		If empty, no debug file is created. */
		QString m_DebugAudioLevelsFileName;

		/** Name of file to store the audio beats for debugging purposes.
		Creates a 16-bit int stereo RAW file,
		left channel contains the original audiodata, right channel contains a "ping" for each detected beat.
		If empty, no debug file is created. */
		QString m_DebugAudioBeatsFileName;

		Options(const Options &) = default;
		Options(Options &&) = default;
		Options();

		bool operator < (const Options & a_Other);
		bool operator ==(const Options & a_Other);
		Options & operator = (const Options & a_Other) = default;
	};



	/** Holds the calculated result of the detection. */
	struct Result
	{
		/** The options used to calculate this result. */
		Options m_Options;

		/** The detected tempo. */
		double m_Tempo;

		/** The confidence of the detection. Ranges from 0 to 100, higher means more confident. */
		double m_Confidence;

		/** A confidence-desc-sorted vector of tempo -> confidence.
		The first item corresponds to m_Tempo, m_Confidence. */
		std::vector<std::pair<double, double>> m_Confidences;

		/** A time-sorted vector of beat indices (into m_Levels) and their weight. */
		std::vector<std::pair<size_t, qint32>> m_Beats;

		/** A vector of all levels calculated for the audio. */
		std::vector<qint32> m_Levels;
	};

	using ResultPtr = std::shared_ptr<Result>;


	TempoDetector();

	/** Returns the number of songs that are queued for scanning. */
	int queueLength() { return m_QueueLength.load(); }

	/** Scans the specified song synchronously. */
	std::shared_ptr<Result> scanSong(SongPtr a_Song, const Options & a_Options = Options());

	/** Queues the specified song for scanning in a background task.
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScanSong(SongPtr a_Song, const Options & a_Options = Options());


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> m_QueueLength;


signals:

	/** Emitted after a song has been scanned. */
	void songScanned(SongPtr a_Song, TempoDetector::ResultPtr a_Result);
};

Q_DECLARE_METATYPE(TempoDetector::ResultPtr);




#endif // TEMPODETECTOR_H

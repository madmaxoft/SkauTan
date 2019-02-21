#ifndef TEMPODETECTOR_H
#define TEMPODETECTOR_H




#include <set>
#include <mutex>
#include <memory>
#include <atomic>
#include <QObject>
#include "ComponentCollection.hpp"
#include "Song.hpp"





/** Component that detects tempo in songs.
Implements the tempo detection using several different algorithms.
This encapsulates both the low-level scanning, and the higher-level options management for optimal scan results.
Terminology:
	- "scan" = detect tempo, return as a separate result (works on single SongPtr)
	- "detect" = detect tempo, set result directly into songSD (works on Song::SharedDataPtr, picks the first decodable file)
Songs can be scanned synchronously or asynchronously using BackgroundTasks.
The scan finishes with a Result object that contains the calculated values, it is up to the caller to
process the results and potentially set them into the song metadata, display to the user etc.
Also upon a finished scan a signal is emitted that can be hooked to. */
class TempoDetector:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckTempoDetector>
{
	Q_OBJECT
	using Super = ComponentCollection::Component<ComponentCollection::ckTempoDetector>;


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
		int m_SampleRate;

		/** The algorithm to use for detecting loudness levels in the audio. */
		ELevelAlgorithm m_LevelAlgorithm;

		/** The length of audio data to process for one levels sample. */
		size_t m_WindowSize;

		/** The distance between successive levels, in samples. */
		size_t m_Stride;

		/** The number of levels to check before and after current level to filter out non-peak levels. */
		size_t m_LocalMaxDistance;

		/** If true, the levels are normalized across m_NormalizeLevelsWindowSize elements. */
		bool m_ShouldNormalizeLevels;

		/** Number of neighboring levels to normalize against when m_ShouldNormalizeLevels is true. */
		size_t m_NormalizeLevelsWindowSize;

		/** The maximum tempo that is tested in the detection. */
		int m_MaxTempo;

		/** The minimum tempo that is tested in the detection. */
		int m_MinTempo;

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



	/** Holds the calculated result of a single detection. */
	struct Result
	{
		/** The options used to calculate this result. */
		std::vector<Options> m_Options;

		/** The final detected tempo. 0 if not detected. */
		double m_Tempo;

		/** The confidence of the detection. Ranges from 0 to 100, higher means more confident. */
		double m_Confidence;

		/** The tempos detected with the individual m_Options items, one pair per options item. */
		std::vector<std::pair<double, double>> m_Tempos;

		/** A time-sorted vector of beat indices (into m_Levels) and their weight.
		Only contains the beats from the last m_Options item. */
		std::vector<std::pair<size_t, qint32>> m_Beats;

		/** A vector of all levels calculated for the audio.
		Only contains the levels from the last m_Options item. */
		std::vector<qint32> m_Levels;


		/** Creates an "invalid" result - no confidence, no detected tempo. */
		Result():
			m_Tempo(0),
			m_Confidence(0)
		{
		}
	};

	using ResultPtr = std::shared_ptr<Result>;

	TempoDetector();


	////////////////////////////////////////////////////////////////////////////////
	// Low-level interface: single scan using explicit options:

	/** Scans the specified song synchronously, using the specified options.
	Each item in a_Options is used to scan the song, and the individual results are then aggregated into a single tempo value.
	Note that samplerate is only taken from the first item in a_Options, the other items' samplerates
	are ignored (can't change the samplerate once the song is decoded).
	Emits the songScanned() signal upon successful scan, but doesn't store the detected tempo in a_Song. */
	std::shared_ptr<Result> scanSong(SongPtr a_Song, const std::vector<Options> & a_Options = {Options()});

	/** Queues the specified song for scanning in a background task.
	Each item in a_Options is used to scan the song, and the individual results are then aggregated into a single tempo value.
	Note that samplerate is only taken from the first item in a_Options, the other items' samplerates
	are ignored (can't change the samplerate once the song is decoded).
	Once the song is scanned, the songScanned() signal is emitted, but the detected tempo is NOT stored in a_Song. */
	void queueScanSong(SongPtr a_Song, const std::vector<Options> & a_Options = {Options()});

	/** Returns the number of songs that are queued for scanning. */
	int scanQueueLength() { return m_ScanQueueLength.load(); }


	////////////////////////////////////////////////////////////////////////////////
	// High-level interface: Single scan using optimal options:

	/** Returns a name to be used for the background detection task on the specified song. */
	static QString createTaskName(Song::SharedDataPtr a_SongSD);

	/** Runs the detection synchronously on the specified song.
	Called internally from this class, and externally from the task repeater.
	The detected tempo is set directly into the song upon success (but not saved in the DB).
	Returns true if detection was successful, false on failure.
	Emits the songScanned() and songTempoDetected() signals. */
	bool detect(Song::SharedDataPtr a_SongSD);

	/** Queues the detection on the specified song to be run asynchronously in a BackgroundTask.
	The detected tempo is set directly into the song upon success (but not saved in the DB).
	Emits the songScanned() and songTempoDetected() signals upon success, nothing on failure. */
	void queueDetect(Song::SharedDataPtr a_SongSD);


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> m_ScanQueueLength;


signals:

	/** Emitted after a song has been scanned. */
	void songScanned(SongPtr a_Song, TempoDetector::ResultPtr a_Result);

	/** Emitted after a song has been scanned and its tempo was stored. */
	void songTempoDetected(Song::SharedDataPtr a_Song);
};

Q_DECLARE_METATYPE(TempoDetector::ResultPtr);




#endif // TEMPODETECTOR_H

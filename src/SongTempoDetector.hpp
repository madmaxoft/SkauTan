#pragma once

#include <atomic>
#include <QObject>
#include "ComponentCollection.hpp"
#include "Song.hpp"
#include "TempoDetector.hpp"





/** Component that detects tempo in songs.
Uses the TempoDetector library to detect the song tempo.
This encapsulates both the low-level scanning, and the higher-level options management for optimal scan results.
Terminology:
	- "scan" = detect tempo, return as a separate result (works on single SongPtr)
	- "detect" = detect tempo, set result directly into songSD (works on Song::SharedDataPtr, picks the first decodable file)
Songs can be scanned synchronously or asynchronously using BackgroundTasks.
The scan finishes with a Result object that contains the calculated values, it is up to the caller to
process the results and potentially set them into the song metadata, display to the user etc.
Also upon a finished scan a signal is emitted that can be hooked to. */
class SongTempoDetector:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckTempoDetector>
{
	Q_OBJECT
	using Super = ComponentCollection::Component<ComponentCollection::ckTempoDetector>;


public:


	/** Holds the options for a single detection. */
	struct Options: public TempoDetector::Options
	{
		/** Name of file to store the audio levels for debugging purposes.
		Creates a 16-bit int stereo RAW file,
		left channel contains the original audiodata, right channel contains level values.
		If empty, no debug file is created. */
		QString mDebugAudioLevelsFileName;

		/** Name of file to store the audio beats for debugging purposes.
		Creates a 16-bit int stereo RAW file,
		left channel contains the original audiodata, right channel contains a "ping" for each detected beat.
		If empty, no debug file is created. */
		QString mDebugAudioBeatsFileName;
	};




	SongTempoDetector();


	////////////////////////////////////////////////////////////////////////////////
	// Low-level interface: single scan using explicit options:

	/** Scans the specified song synchronously, using the specified options.
	Each item in aOptions is used to scan the song, and the individual results are then aggregated into a single tempo value.
	Note that samplerate is only taken from the first item in aOptions, the other items' samplerates
	are ignored (can't change the samplerate once the song is decoded).
	Returns a Result that is a combination of the result from the first aOptions scan and the aggregated tempo / confidence.
	Emits the songScanned() signal upon successful scan, but doesn't store the detected tempo in aSong. */
	TempoDetector::ResultPtr scanSong(SongPtr aSong, const std::vector<Options> & aOptions = {Options()});

	/** Queues the specified song for scanning in a background task.
	Each item in aOptions is used to scan the song, and the individual results are then aggregated into a single tempo value.
	Note that samplerate is only taken from the first item in aOptions, the other items' samplerates
	are ignored (can't change the samplerate once the song is decoded).
	Once the song is scanned, the songScanned() signal is emitted, but the detected tempo is NOT stored in aSong. */
	void queueScanSong(SongPtr aSong, const std::vector<Options> & aOptions = {Options()});

	/** Returns the number of songs that are queued for scanning. */
	int scanQueueLength() { return mScanQueueLength.load(); }


	////////////////////////////////////////////////////////////////////////////////
	// High-level interface: Single scan using optimal options:

	/** Returns a name to be used for the background detection task on the specified song. */
	static QString createTaskName(Song::SharedDataPtr aSongSD);

	/** Runs the detection synchronously on the specified song.
	Called internally from this class, and externally from the task repeater.
	The detected tempo is set directly into the song upon success (but not saved in the DB).
	Returns true if detection was successful, false on failure.
	Emits the songScanned() and songTempoDetected() signals. */
	bool detect(Song::SharedDataPtr aSongSD);

	/** Queues the detection on the specified song to be run asynchronously in a BackgroundTask.
	The detected tempo is set directly into the song upon success (but not saved in the DB).
	Emits the songScanned() and songTempoDetected() signals upon success, nothing on failure. */
	void queueDetect(Song::SharedDataPtr aSongSD);


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> mScanQueueLength;


signals:

	/** Emitted after a song has been scanned.
	If the song was scanned using multiple results, the aResult is a combination of the Result for the first
	options item and the aggregated tempo and confidence. */
	void songScanned(SongPtr aSong, TempoDetector::ResultPtr aResult);

	/** Emitted after a song has been scanned and its tempo was stored. */
	void songTempoDetected(Song::SharedDataPtr aSong);
};

Q_DECLARE_METATYPE(TempoDetector::ResultPtr);

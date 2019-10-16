#pragma once

#include <memory>
#include <atomic>
#include <QObject>
#include "TempoDetector.hpp"





// fwd:
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Applies the tempo detection to songs.
Songs can be scanned synchronously or asynchronously using BackgroundTasks.
The scan finishes with a TempoDetector::Result object that contains the calculated values,
it is up to the caller to process the results and potentially set them into the song metadata,
display to the user etc. */
class SongTempoDetector:
	public QObject
{
	Q_OBJECT
	using Super = QObject;


public:

	class Options:
		public TempoDetector::Options
	{
	public:
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

	/** Returns the number of songs that are queued for scanning. */
	int queueLength() { return m_QueueLength.load(); }

	/** Scans the specified song synchronously. */
	TempoDetector::ResultPtr scanSong(
		SongPtr a_Song,
		const SongTempoDetector::Options & a_Options = SongTempoDetector::Options()
	);

	/** Queues the specified song for scanning in a background task.
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScanSong(
		SongPtr a_Song,
		const SongTempoDetector::Options & a_Options = SongTempoDetector::Options()
	);


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> m_QueueLength;


signals:

	/** Emitted after a song has been scanned. */
	void songScanned(SongPtr a_Song, TempoDetector::ResultPtr a_Result);
};

Q_DECLARE_METATYPE(TempoDetector::ResultPtr);

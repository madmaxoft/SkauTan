#ifndef TEMPODETECTTASK_HPP
#define TEMPODETECTTASK_HPP





#include <atomic>
#include <set>
#include <mutex>
#include <QObject>
#include "BackgroundTasks.hpp"
#include "Song.hpp"
#include "ComponentCollection.hpp"





// fwd:
class Database;
class PlaybackBuffer;





/** Task that detects tempo in one song (possibly from multiple files of the same hash).
Usage:
Call TempoDetectTask::enqueue() to detect tempo in a song in the background
Call TempoDetectTask::detect() to detect in the current thread (synchronously).
There's logic to handle multiple song files in a single song hash, both file-wise and genre-wise.
See also TempoDetectTaskRepeater for continuous background processing of all songs. */
class TempoDetectTask:
	public QObject
{
	Q_OBJECT


public:

	/** Enqueues the specified song to be scanned.
	If a_CallAfterFinished is non-nullptr, it is called after the detection finishes, in the detection thread. */
	static void enqueue(
		ComponentCollection & a_Components,
		Song::SharedDataPtr a_SongSD,
		std::function<void(void)> a_CallAfterFinished = nullptr
	);

	/** Runs the detection synchronously on the specified song.
	Called internally from this class, and externally from the TempoDetectTaskRepeater.
	Returns true if detection was successful, false on failure. */
	static bool detect(ComponentCollection & a_Components, Song::SharedDataPtr a_SongSD);

	/** Returns the name to be used for the task in the user-visible UI. */
	static QString createTaskName(Song::SharedDataPtr a_SongSD);
};





/** Class that keeps repeating the TempoDetectTask as long as there are eligible songs in the DB.
Only one task is enqueued at a time, to reduce background threads overwhelming the main threads. */
class TempoDetectTaskRepeater:
	public ComponentCollection::Component<ComponentCollection::ckTempoDetectTaskRepeater>
{
public:

	/** Creates a new stopped instance. */
	TempoDetectTaskRepeater(ComponentCollection & a_Components);

	/** Starts the repeater.
	Ignored if already started. */
	void start();

	/** Stops the repeater.
	The current detection task, if any, will still keep running. */
	void stop();

	/** Returns true if there's a task currently being executed. */
	bool isRunning() { return m_IsRunning.load(); }


protected:

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** True if there's a TempoDetectTask queued / running in the background. */
	std::atomic<bool> m_IsRunning;

	/** If true, the enqueueing loop will not enqueue any more tasks and will terminate after the current
	detection finishes. */
	std::atomic<bool> m_ShouldAbort;

	/** The songs that have failed to provide a tempo; these will be skipped until program restart.
	Protected against multithreaded access by m_MtxFailedSongs. */
	std::set<Song::SharedDataPtr> m_FailedSongs;

	/** Mutex for protecting m_FailedSongs agains multithreaded access. */
	std::mutex m_MtxFailedSongs;


	/** Picks a song from the DB that should be processed next.
	If no eligible song found, returns nullptr. */
	Song::SharedDataPtr pickNextSong();

	/** Picks a song to process and enqueues the detection task. */
	void enqueueAnother();
};





#endif  // TEMPODETECTTASK_HPP

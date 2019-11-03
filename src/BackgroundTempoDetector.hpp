#pragma once

#include <atomic>
#include <set>
#include <mutex>
#include <QObject>
#include "BackgroundTasks.hpp"
#include "Song.hpp"
#include "ComponentCollection.hpp"





/** Class that runs tempo detection in the background for all eligible songs in the DB.
Only one task is enqueued at a time, to reduce background threads overwhelming the main threads. */
class BackgroundTempoDetector:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckBackgroundTempoDetector>
{
	Q_OBJECT


public:

	/** Creates a new stopped instance. */
	BackgroundTempoDetector(ComponentCollection & aComponents);

	/** Starts the repeater.
	Ignored if already started. */
	void start();

	/** Stops the repeater.
	The current detection task, if any, will still keep running. */
	void stop();

	/** Returns true if there's a task currently being executed. */
	bool isRunning() { return mIsRunning.load(); }


protected:

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** True if there's a TempoDetectTask queued / running in the background. */
	std::atomic<bool> mIsRunning;

	/** If true, the enqueueing loop will not enqueue any more tasks and will terminate after the current
	detection finishes. */
	std::atomic<bool> mShouldAbort;

	/** The songs that have failed to provide a tempo; these will be skipped until program restart.
	Protected against multithreaded access by mMtxFailedSongs. */
	std::set<Song::SharedDataPtr> mFailedSongs;

	/** Mutex for protecting mFailedSongs agains multithreaded access. */
	std::mutex mMtxFailedSongs;


	/** Picks a song from the DB that should be processed next.
	If no eligible song found, returns nullptr. */
	Song::SharedDataPtr pickNextSong();

	/** Picks a song to process and enqueues the detection task. */
	void enqueueAnother();
};

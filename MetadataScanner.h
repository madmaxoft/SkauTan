#ifndef METADATASCANNER_H
#define METADATASCANNER_H





#include <atomic>
#include <memory>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>





// fwd:
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Implements the background scan / update for song metadata. */
class MetadataScanner:
	public QThread
{
	Q_OBJECT
	using Super = QThread;

public:
	MetadataScanner();

	virtual ~MetadataScanner();

	/** Queues the specified song for scanning.
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScan(SongPtr a_Song);


protected:

	/** If set to true, the thread will terminate at the next possible moment. */
	std::atomic<bool> m_ShouldTerminate;

	/** Guards the m_Queue against multithreaded access. */
	QMutex m_MtxQueue;

	/** Wait condition for waking up the worker thread once there are songs in the queue. */
	QWaitCondition m_SongAvailable;

	/** The songs that are to be scanned.
	Protected by m_MtxQueue against multithreaded access. */
	std::vector<SongPtr> m_Queue;


	// QThread overrides:
	virtual void run() override;

	/** Returns the next song to process, or nullptr when the thread is to terminate.
	Blocks until a song is to be scanned or the thread should terminate. */
	SongPtr getSongToProcess();

	/** Scans the metadata for the specified song. */
	void processSong(SongPtr a_Song);


signals:

	/** Emitted after a song has been scanned.
	The metadata has already been set to the song. */
	void songScanned(Song * a_Song);
};





#endif // METADATASCANNER_H

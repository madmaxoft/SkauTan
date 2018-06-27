#ifndef METADATASCANNER_H
#define METADATASCANNER_H





#include <memory>
#include <atomic>
#include <QObject>
#include "ComponentCollection.h"





// fwd:
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Implements the background scan / update for song metadata.
Enqueues metadata scanning to background tasks. */
class MetadataScanner:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckMetadataScanner>
{
	Q_OBJECT
	using Super = QObject;


public:

	MetadataScanner();

	/** Returns the number of songs that are queued for scanning. */
	int queueLength() { return m_QueueLength.load(); }


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> m_QueueLength;


	/** Internal shared implementation of queueScanSong() and queueScanSongPriority(). */
	void enqueueScan(SongPtr a_Song, bool a_Prioritize);


signals:

	/** Emitted after a song has been scanned.
	The metadata has already been set to the song. */
	void songScanned(SongPtr a_Song);


public slots:

	/** Queues the specified song for scanning in a background task.
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScanSong(SongPtr a_Song);

	/** Queues the specified song for scanning in a background task, prioritized (executed asap).
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScanSongPriority(SongPtr a_Song);

	/** Scans the song synchronously.
	Once the song is scanned, the songScanned() signal is emitted, as part of this call. */
	void scanSong(SongPtr a_Song);
};





#endif // METADATASCANNER_H

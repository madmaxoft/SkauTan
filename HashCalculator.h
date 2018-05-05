#ifndef HASHCALCULATOR_H
#define HASHCALCULATOR_H





#include "Song.h"





/** Calculates the hash of a song file.
The hash used is a SHA1 checksum of the raw audio data in the file's audio track. This is considered immune
against changes in the ID3 tag.
The actual hashing takes place in a background task. */
class HashCalculator:
	public QObject
{
	using Super = QObject;

	Q_OBJECT


public:

	HashCalculator();


public slots:

	/** Queues the specified song for hashing in a background task.
	After the hash has been calculated, either songHashCalculated() or songHashFailed() is emitted. */
	void queueHashSong(SongPtr a_Song);


signals:

	/** Emitted after successfully hashing the song.
	The song's hash is already stored in a_Song before emitting this signal. */
	void songHashCalculated(SongPtr a_Song);

	/** Emitted after encountering a problem while hashing a song. */
	void songHashFailed(SongPtr a_Song);
};

#endif // HASHCALCULATOR_H

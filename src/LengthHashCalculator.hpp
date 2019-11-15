#pragma once

#include <atomic>
#include <QObject>
#include "Song.hpp"
#include "ComponentCollection.hpp"





/** Calculates the length and hash of a song file.
The hash used is a SHA1 checksum of the raw audio data in the file's audio track. This is considered immune
against changes in the ID3 tag.
The actual hashing takes place in a background task. */
class LengthHashCalculator:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckLengthHashCalculator>
{
	using Super = QObject;

	Q_OBJECT


public:

	LengthHashCalculator(ComponentCollection & aComponents);

	/** Returns the number of songs that are queued for hashing. */
	int queueLength() { return mQueueLength.load(); }

	/** Calculates the hash and length of the specified song file.
	Returns the hash and length (in seconds).
	If either cannot be calculated, returns an empty QByteArray / negative length. */
	static std::pair<QByteArray, double> calculateSongHashAndLength(const QString & aFileName, const QByteArray & aFileData);

	/** Calculates the length of the specified song file, in seconds.
	If the length cannot be calculated, returns a negative number. */
	static double calculateSongLength(const QString & aFileName);


protected:

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The number of songs that are queued for hashing. */
	std::atomic<int> mQueueLength;


public slots:

	/** Queues the specified file for length and hash in a background task.
	After the hash has been calculated, either fileHashCalculated() or fileHashFailed() is emitted. */
	void queueHashFile(const QString & aFileName);

	/** Queues the specified song for length calculation in a background task.
	Tries the first "duplicate" that actually exists in the filesystem.
	After the length has been calculated, either fileLengthCalculated() or fileLengthFailed() is emitted. */
	void queueLengthSong(Song::SharedDataPtr aSharedData);


signals:

	/** Emitted after successfully lengthing and hashing the song file. */
	void fileHashCalculated(const QString & aFileName, const QByteArray & aHash, double aLengthSec);

	/** Emitted after encountering a problem while lengthing or hashing a file. */
	void fileHashFailed(const QString & aFile);

	/** Emitted after successfully lengthing the song file. */
	void songLengthCalculated(Song::SharedDataPtr aSharedData, double aLengthSec);

	/** Emitted after encountering a problem while lengthing a song. */
	void songLengthFailed(Song::SharedDataPtr aSharedData);
};

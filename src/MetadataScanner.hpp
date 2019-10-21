#ifndef METADATASCANNER_H
#define METADATASCANNER_H





#include <memory>
#include <atomic>
#include <QObject>
#include "ComponentCollection.hpp"
#include "Song.hpp"
#include "DatedOptional.hpp"





// fwd:
namespace TagLib
{
	class FileRef;
}





/** Implements the background scan / update for song metadata.
Enqueues metadata scanning to background tasks. */
class MetadataScanner:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckMetadataScanner>
{
	Q_OBJECT
	using Super = QObject;


public:

	/** Representation of the data read from the file's tag.
	In addition to Song::Tag, it stores the comment as well. */
	struct Tag
	{
		DatedOptional<QString> mAuthor;
		DatedOptional<QString> mTitle;
		DatedOptional<QString> mComment;
		DatedOptional<QString> mGenre;
		DatedOptional<double>  mMeasuresPerMinute;

		Tag() = default;
		Tag(const Tag & aOther) = default;
		Tag(
			const QString & aAuthor,
			const QString & aTitle,
			const QString & aComment,
			const QString & aGenre,
			double aMeasuresPerMinute
		):
			mAuthor(aAuthor),
			mTitle(aTitle),
			mComment(aComment),
			mGenre(aGenre),
			mMeasuresPerMinute(aMeasuresPerMinute)
		{
		}

		Tag(
			const QString & aAuthor,
			const QString & aTitle,
			const QString & aComment,
			const QString & aGenre
		):
			mAuthor(aAuthor),
			mTitle(aTitle),
			mComment(aComment),
			mGenre(aGenre)
		{
		}
	};



	MetadataScanner();

	/** Returns the number of songs that are queued for scanning. */
	int queueLength() { return mQueueLength.load(); }

	/** Writes the tag data in aTag into aSong's ID3 tag.
	Also updates the song's parsed ID3 values based on the new tag contents (but doesn't save the song). */
	static void writeTagToSong(SongPtr aSong, const Tag & aTag);

	/** Reads the tag from the specified file.
	The first value indicates whether the tag could be read, false means failure. */
	static std::pair<bool, Tag> readTagFromFile(const QString & aFileName) noexcept;

	/** Parses the raw ID3 tag into song tag.
	Detects BPM, MPM and genre substrings in aFileTag's values and moves the to the appropriate value. */
	static Song::Tag parseId3Tag(const Tag & aFileTag);

	/** Returns a tag that contains values from aFileTag after applying changes in aChanges.
	Only values from aChanges that are present are applied into the result. */
	static Tag applyTagChanges(const Tag & aFileTag, const Tag & aChanges);

	/** Attempts to parse the filename into metadata.
	Assumes that most of our songs have some info in their filename or the containing folders,
	tries to extract that info into the returned tag. */
	static Song::Tag parseFileNameIntoMetadata(const QString & aFileName);


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> mQueueLength;


	/** Internal shared implementation of queueScanSong() and queueScanSongPriority(). */
	void enqueueScan(SongPtr aSong, bool aPrioritize);

	/** Opens the specified file for reading / writing the tag using TagLib. */
	static TagLib::FileRef openTagFile(const QString & aFileName);


signals:

	/** Emitted after a song has been scanned.
	The metadata has already been set to the song. */
	void songScanned(SongPtr aSong);


public slots:

	/** Queues the specified song for scanning in a background task.
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScanSong(SongPtr aSong);

	/** Queues the specified song for scanning in a background task, prioritized (executed asap).
	Once the song is scanned, the songScanned() signal is emitted. */
	void queueScanSongPriority(SongPtr aSong);

	/** Scans the song synchronously.
	Once the song is scanned, the songScanned() signal is emitted, as part of this call. */
	void scanSong(SongPtr aSong);
};





#endif // METADATASCANNER_H

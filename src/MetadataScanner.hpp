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
		DatedOptional<QString> m_Author;
		DatedOptional<QString> m_Title;
		DatedOptional<QString> m_Comment;
		DatedOptional<QString> m_Genre;
		DatedOptional<double>  m_MeasuresPerMinute;

		Tag() = default;
		Tag(const Tag & a_Other) = default;
		Tag(
			const QString & a_Author,
			const QString & a_Title,
			const QString & a_Comment,
			const QString & a_Genre,
			double a_MeasuresPerMinute
		):
			m_Author(a_Author),
			m_Title(a_Title),
			m_Comment(a_Comment),
			m_Genre(a_Genre),
			m_MeasuresPerMinute(a_MeasuresPerMinute)
		{
		}

		Tag(
			const QString & a_Author,
			const QString & a_Title,
			const QString & a_Comment,
			const QString & a_Genre
		):
			m_Author(a_Author),
			m_Title(a_Title),
			m_Comment(a_Comment),
			m_Genre(a_Genre)
		{
		}
	};



	MetadataScanner();

	/** Returns the number of songs that are queued for scanning. */
	int queueLength() { return m_QueueLength.load(); }

	/** Writes the tag data in a_Tag into a_Song's ID3 tag.
	Also updates the song's parsed ID3 values based on the new tag contents (but doesn't save the song). */
	static void writeTagToSong(SongPtr a_Song, const Tag & a_Tag);

	/** Reads the tag from the specified file.
	The first value indicates whether the tag could be read, false means failure. */
	static std::pair<bool, Tag> readTagFromFile(const QString & a_FileName) noexcept;

	/** Parses the raw ID3 tag into song tag.
	Detects BPM, MPM and genre substrings in a_FileTag's values and moves the to the appropriate value. */
	static Song::Tag parseId3Tag(const Tag & a_FileTag);

	/** Returns a tag that contains values from a_FileTag after applying changes in a_Changes.
	Only values from a_Changes that are present are applied into the result. */
	static Tag applyTagChanges(const Tag & a_FileTag, const Tag & a_Changes);

	/** Attempts to parse the filename into metadata.
	Assumes that most of our songs have some info in their filename or the containing folders,
	tries to extract that info into the returned tag. */
	static Song::Tag parseFileNameIntoMetadata(const QString & a_FileName);


protected:

	/** The number of songs that are queued for scanning. */
	std::atomic<int> m_QueueLength;


	/** Internal shared implementation of queueScanSong() and queueScanSongPriority(). */
	void enqueueScan(SongPtr a_Song, bool a_Prioritize);

	/** Opens the specified file for reading / writing the tag using TagLib. */
	static TagLib::FileRef openTagFile(const QString & a_FileName);


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

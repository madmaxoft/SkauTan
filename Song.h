#ifndef SONG_H
#define SONG_H





#include <memory>
#include <vector>
#include <QString>
#include <QDateTime>
#include <QVariant>





class QSqlQuery;





/** Binds an on-disk file with its metadata stored in our DB. */
class Song
{
public:
	/** A simple structure for a set of tags, extracted from a single source.
	Since there are three tag sources with common metadata (ID3, FileName, Manual), this stores
	the metadata for each source. */
	struct Tag
	{
		QVariant m_Title;
		QVariant m_Author;
		QVariant m_Genre;
		QVariant m_MeasuresPerMinute;
	};


	/** Creates a new instance that has only the file name and size set, all the other metadata are empty.
	Used when adding new files. */
	explicit Song(const QString & a_FileName,
		qulonglong a_FileSize
	);

	/** Creates a new instance with all fields set.
	Used when loading songs from the DB. */
	explicit Song(QString && a_FileName,
		qulonglong a_FileSize,
		QVariant && a_Hash,
		QVariant && a_Length,
		Tag && a_TagManual,
		Tag && a_TagFileName,
		Tag && a_TagId3,
		QVariant && a_LastPlayed,
		QVariant && a_Rating,
		QVariant && a_LastMetadataUpdated
	);

	const QString & fileName() const { return m_FileName; }
	qulonglong fileSize() const { return m_FileSize; }
	const QVariant & hash() const { return m_Hash; }
	const QVariant & length() const { return m_Length; }
	const Tag & tagManual() const { return m_TagManual; }
	const Tag & tagFileName() const { return m_TagFileName; }
	const Tag & tagId3() const { return m_TagId3; }
	const QVariant & lastPlayed() const { return m_LastPlayed; }
	const QVariant & rating() const { return m_Rating; }
	const QVariant & lastMetadataUpdated() const { return m_LastMetadataUpdated; }

	// These return the value from the first tag which has the value valid, in the order or Manual, Id3, FileName
	const QVariant & author() const;
	const QVariant & title() const;
	const QVariant & genre() const;
	const QVariant & measuresPerMinute() const;

	/** Returns whether the disk file still exists and it matches our stored hash. */
	bool isStillValid() const;

	/** Updates the length of the song, in seconds. */
	void setLength(double a_Length);

	// Basic setters:
	void setHash(QByteArray a_Hash) { m_Hash = a_Hash; }
	void setLastPlayed(const QDateTime & a_LastPlayed) { m_LastPlayed = a_LastPlayed; }
	void setLastMetadataUpdated(QDateTime a_LastMetadataUpdated) { m_LastMetadataUpdated = a_LastMetadataUpdated; }

	// Setters that redirect into the Manual tag:
	void setAuthor(QVariant a_Author) { m_TagManual.m_Author = a_Author; }
	void setTitle(QVariant a_Title) { m_TagManual.m_Title = a_Title; }
	void setGenre(const QString & a_Genre) { m_TagManual.m_Genre = a_Genre; }
	void setMeasuresPerMinute(double a_MeasuresPerMinute) { m_TagManual.m_MeasuresPerMinute = a_MeasuresPerMinute; }

	// Setters for specific tags:
	void setId3Author(const QString & a_Author) { m_TagId3.m_Author = a_Author; }
	void setId3Title(const QString & a_Title)   { m_TagId3.m_Title  = a_Title; }
	void setId3Genre(const QString & a_Genre)   { m_TagId3.m_Genre  = a_Genre; }
	void setId3MeasuresPerMinute(double a_MPM)  { m_TagId3.m_MeasuresPerMinute = a_MPM; }
	void setId3Tag(const Tag & a_Tag) { m_TagId3 = a_Tag; }
	void setFileNameAuthor(const QString & a_Author) { m_TagFileName.m_Author = a_Author; }
	void setFileNameTitle(const QString & a_Title)   { m_TagFileName.m_Title  = a_Title; }
	void setFileNameGenre(const QString & a_Genre)   { m_TagFileName.m_Genre  = a_Genre; }
	void setFileNameMeasuresPerMinute(double a_MPM)  { m_TagFileName.m_MeasuresPerMinute = a_MPM; }
	void setFileNameTag(const Tag & a_Tag) { m_TagFileName = a_Tag; }

	/** Returns true if any of the metadata is invalid or the song hasn't been scanned for a long time.
	SongDatabase uses this to decide whether to queue the song for re-scan upon loading the DB. */
	bool needsMetadataRescan() const;

	/** Copies all metadata from the specified source song.
	This is used when a hash duplicate is detected. */
	void copyMetadataFrom(const Song & a_Src);


protected:

	QString m_FileName;
	qulonglong m_FileSize;
	QVariant m_Hash;
	QVariant m_Length;
	Tag m_TagManual;
	Tag m_TagFileName;
	Tag m_TagId3;
	QVariant m_LastPlayed;
	QVariant m_Rating;
	QVariant m_LastMetadataUpdated;
};

using SongPtr = std::shared_ptr<Song>;

Q_DECLARE_METATYPE(SongPtr);





#endif // SONG_H

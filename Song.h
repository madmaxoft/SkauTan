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
	/** Creates a new instance that has only the file name and size set, all the other metadata are empty.
	Used when adding new files. */
	explicit Song(const QString & a_FileName,
		qulonglong a_FileSize,
		qlonglong a_DbRowId
	);

	/** Creates a new instance with all fields set.
	Used when loading songs from the DB. */
	explicit Song(QString && a_FileName,
		qulonglong a_FileSize,
		qlonglong a_DbRowId,
		QVariant && a_Hash,
		QVariant && a_Length,
		QVariant && a_Genre,
		QVariant && a_MeasuresPerMinute,
		QVariant && a_LastPlayed,
		QVariant && a_Rating
	, QVariant && a_LastMetadataUpdated);

	const QString & fileName() const { return m_FileName; }
	qulonglong fileSize() const { return m_FileSize; }
	qlonglong dbRowId() const { return m_DbRowId; }
	const QVariant & hash() const { return m_Hash; }
	const QVariant & length() const { return m_Length; }
	const QVariant & genre() const { return m_Genre; }
	const QVariant & measuresPerMinute() const { return m_MeasuresPerMinute; }
	const QVariant & lastPlayed() const { return m_LastPlayed; }
	const QVariant & rating() const { return m_Rating; }
	const QVariant & author() const { return m_Author; }
	const QVariant & title() const { return m_Title; }
	const QVariant & lastMetadataUpdated() const { return m_LastMetadataUpdated; }

	/** Returns whether the disk file still exists and it matches our stored hash. */
	bool isStillValid() const;

	/** Updates the length of the song, in seconds. */
	void setLength(double a_Length);

	// Basic setters:
	void setHash(QByteArray a_Hash) { m_Hash = a_Hash; }
	void setGenre(const QString & a_Genre) { m_Genre = a_Genre; }
	void setMeasuresPerMinute(double a_MeasuresPerMinute) { m_MeasuresPerMinute = a_MeasuresPerMinute; }
	void setLastPlayed(const QDateTime & a_LastPlayed) { m_LastPlayed = a_LastPlayed; }
	void setAuthor(QVariant a_Author) { m_Author = a_Author; }
	void setTitle(QVariant a_Title) { m_Title = a_Title; }
	void setLastMetadataUpdated(QDateTime a_LastMetadataUpdated) { m_LastMetadataUpdated = a_LastMetadataUpdated; }

	/** Returns true if any of the metadata is invalid or the song hasn't been scanned for a long time.
	SongDatabase uses this to decide whether to queue the song for re-scan upon loading the DB. */
	bool needsMetadataRescan() const;

protected:

	QString m_FileName;
	qulonglong m_FileSize;
	qlonglong m_DbRowId;
	QVariant m_Hash;
	QVariant m_Length;
	QVariant m_Genre;
	QVariant m_MeasuresPerMinute;
	QVariant m_LastPlayed;
	QVariant m_Rating;
	QVariant m_Author;
	QVariant m_Title;
	QVariant m_LastMetadataUpdated;
};

using SongPtr = std::shared_ptr<Song>;





#endif // SONG_H

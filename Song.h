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
	explicit Song(
		QString && a_FileName,
		qulonglong a_FileSize,
		qlonglong a_DbRowId,
		QByteArray && a_Hash,
		QVariant a_Length,
		QString && a_Genre,
		QVariant a_MeasuresPerMinute,
		QDateTime a_LastPlayed,
		QVariant a_Rating
	);

	const QString & fileName() const { return m_FileName; }
	qulonglong fileSize() const { return m_FileSize; }
	qlonglong dbRowId() const { return m_DbRowId; }
	const QByteArray & hash() const { return m_Hash; }
	double length() const { return m_Length; }
	const QString & genre() const { return m_Genre; }
	double measuresPerMinute() const { return m_MeasuresPerMinute; }
	QDateTime lastPlayed() const { return m_LastPlayed; }
	double rating() const { return m_Rating; }

	/** Returns whether the disk file still exists and it matches our stored hash. */
	bool isStillValid() const;


protected:
	QString m_FileName;
	qulonglong m_FileSize;
	qlonglong m_DbRowId;
	QByteArray m_Hash;
	double m_Length;
	QString m_Genre;
	double m_MeasuresPerMinute;
	QDateTime m_LastPlayed;
	double m_Rating;
};

using SongPtr = std::shared_ptr<Song>;





#endif // SONG_H

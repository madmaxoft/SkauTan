#ifndef SONG_H
#define SONG_H





#include <memory>
#include <vector>
#include <QString>
#include <QDateTime>
#include <QVariant>
#include <QCoreApplication>
#include <QMutex>
#include <DatedOptional.h>





// fwd:
class QSqlQuery;
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Binds an on-disk file with its metadata stored in our DB.
Each disk file has one Song instance; multiple instances may have the same audio data (matched using Hash), these all
have their SharedData pointing to the same object. This way the data that is common for the audio is stored shared,
while data pertaining to the file itself is stored separately for duplicates. */
class Song:
	public std::enable_shared_from_this<Song>
{
	Q_DECLARE_TR_FUNCTIONS(Song)

public:
	/** A simple structure for a set of tags, extracted from a single source.
	Since there are three tag sources with common metadata (ID3, FileName, Manual), this stores
	the metadata for each source. */
	struct Tag
	{
		DatedOptional<QString> m_Title;
		DatedOptional<QString> m_Author;
		DatedOptional<QString> m_Genre;
		DatedOptional<double>  m_MeasuresPerMinute;
	};


	/** Data that can be shared among multiple files that represent the same song (same hash). */
	struct SharedData
	{
		QByteArray m_Hash;
		QVariant m_Length;
		QVariant m_LastPlayed;
		QVariant m_Rating;
		QMutex m_Mtx;  // Mutex protecting m_Duplicates against multithreaded access
		std::vector<SongPtr> m_Duplicates;  // All songs having the same hash

		SharedData(const QByteArray & a_Hash, QVariant && a_Length, QVariant && a_LastPlayed, QVariant && a_Rating):
			m_Hash(a_Hash),
			m_Length(std::move(a_Length)),
			m_LastPlayed(std::move(a_LastPlayed)),
			m_Rating(std::move(a_Rating))
		{
		}

		SharedData(const QByteArray & a_Hash):
			m_Hash(a_Hash)
		{
		}

		void addDuplicate(SongPtr a_Duplicate);
		void delDuplicate(SongPtr a_Duplicate);
	};

	using SharedDataPtr = std::shared_ptr<SharedData>;


	/** Creates a new instance that has only the file name and size set, all the other metadata are empty.
	Used when adding new files. */
	explicit Song(
		const QString & a_FileName,
		qulonglong a_FileSize
	);

	/** Creates a new instance with all fields set.
	Used when loading songs from the DB. */
	explicit Song(
		QString && a_FileName,
		qulonglong a_FileSize,
		QVariant && a_Hash,
		Tag && a_TagManual,
		Tag && a_TagFileName,
		Tag && a_TagId3,
		QVariant && a_LastTagRescanned,
		QVariant && a_NumTagRescanAttempts
	);

	~Song();

	const QString & fileName() const { return m_FileName; }
	qulonglong fileSize() const { return m_FileSize; }
	const QVariant & hash() const { return m_Hash; }
	const Tag & tagManual() const { return m_TagManual; }
	const Tag & tagFileName() const { return m_TagFileName; }
	const Tag & tagId3() const { return m_TagId3; }
	const QVariant & lastTagRescanned() const { return m_LastTagRescanned; }
	const QVariant & numTagRescanAttempts() const { return m_NumTagRescanAttempts; }
	const SharedDataPtr & sharedData() const { return m_SharedData; }
	const DatedOptional<QString> & notes() const { return m_Notes; }

	// These return the value from the first tag which has the value valid, in the order or Manual, Id3, FileName
	const DatedOptional<QString> & primaryAuthor() const;
	const DatedOptional<QString> & primaryTitle() const;
	const DatedOptional<QString> & primaryGenre() const;
	const DatedOptional<double>  & primaryMeasuresPerMinute() const;

	// These return values from the shared data, if available:
	const QVariant & length()     const { return (m_SharedData == nullptr) ? m_Empty : m_SharedData->m_Length; }
	const QVariant & lastPlayed() const { return (m_SharedData == nullptr) ? m_Empty : m_SharedData->m_LastPlayed; }
	const QVariant & rating()     const { return (m_SharedData == nullptr) ? m_Empty : m_SharedData->m_Rating; }

	/** Returns whether the disk file still exists and it matches our stored hash. */
	bool isStillValid() const;

	/** Updates the length of the song, in seconds. */
	void setLength(double a_Length);

	/** Sets the hash to the specified value.
	Raises an assert if there is already a hash for this song, or valid SharedData.
	Does NOT set SharedData based on the hash! */
	void setHash(QByteArray && a_Hash);

	// void setLastPlayed(const QDateTime & a_LastPlayed);

	void setSharedData(SharedDataPtr a_SharedData);

	// Setters that redirect into the Manual tag:
	void setAuthor(QVariant a_Author) { m_TagManual.m_Author = a_Author; }
	void setTitle(QVariant a_Title) { m_TagManual.m_Title = a_Title; }
	void setGenre(const QString & a_Genre) { m_TagManual.m_Genre = a_Genre; }
	void setMeasuresPerMinute(double a_MeasuresPerMinute) { m_TagManual.m_MeasuresPerMinute = a_MeasuresPerMinute; }

	// Setters for specific tags:
	void setManualAuthor(QVariant a_Author) { m_TagManual.m_Author = a_Author; }
	void setManualTitle(QVariant a_Title) { m_TagManual.m_Title = a_Title; }
	void setManualGenre(const QString & a_Genre) { m_TagManual.m_Genre = a_Genre; }
	void setManualMeasuresPerMinute(double a_MeasuresPerMinute) { m_TagManual.m_MeasuresPerMinute = a_MeasuresPerMinute; }
	void setManualTag(const Tag & a_Tag) { m_TagManual = a_Tag; }
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

	// Basic setters:
	void setLastTagRescanned(const QDateTime & a_LastTagRescanned) { m_LastTagRescanned = a_LastTagRescanned; }
	void setNumTagRescanAttempts(int a_NumTagRescanAttempts) { m_NumTagRescanAttempts = a_NumTagRescanAttempts; }
	void setNotes(const QString & a_Notes) { m_Notes = a_Notes; }

	// Setters that preserve the date information:
	void setNotes(const DatedOptional<QString> & a_Notes) { m_Notes = a_Notes; }

	// Setters that move-preserve the date information:
	void setNotes(DatedOptional<QString> && a_Notes) { m_Notes = std::move(a_Notes); }

	/** Returns true if a tag rescan is needed for the song
	(the tags are empty and the scan hasn't been performed already). */
	bool needsTagRescan() const;

	/** Returns all the warnings that this song has, each as a separate string.
	Returns an empty string list if the song has no warnings. */
	QStringList getWarnings() const;

	/** Returns the first of the three variants that is non-empty.
	Returns the third if all are null.*/
	template <typename T> static const DatedOptional<T> & primaryValue(
		const DatedOptional<T> & a_First,
		const DatedOptional<T> & a_Second,
		const DatedOptional<T> & a_Third
	)
	{
		if (!a_First.isEmpty())
		{
			return a_First;
		}
		if (!a_Second.isEmpty())
		{
			return a_Second;
		}
		return a_Third;
	}

	/** Returns the competition tempo range for the specified genre.
	If the genre is not known, returns 0 .. MAX_USHORT */
	static std::pair<double, double> competitionTempoRangeForGenre(const QString & a_Genre);

	/** Returns all the songs that have the same hash as this song, including this.
	If this song has no hash assigned yet, returns only self in the vector. */
	std::vector<SongPtr> duplicates();

	/** Returns all recognized genres, in a format suitable for QComboBox items. */
	static QStringList recognizedGenres();

protected:

	QString m_FileName;
	qulonglong m_FileSize;
	QVariant m_Hash;
	Tag m_TagManual;
	Tag m_TagFileName;
	Tag m_TagId3;
	QVariant m_LastTagRescanned;
	QVariant m_NumTagRescanAttempts;
	SharedDataPtr m_SharedData;
	DatedOptional<QString> m_Notes;

	/** An empty variant returned when there's no shared data for a query */
	static QVariant m_Empty;
};

Q_DECLARE_METATYPE(SongPtr);
Q_DECLARE_METATYPE(Song::SharedDataPtr);





#endif // SONG_H

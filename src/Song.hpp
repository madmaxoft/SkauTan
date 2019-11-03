#pragma once

#include <memory>
#include <vector>
#include <QString>
#include <QDateTime>
#include <QVariant>
#include <QCoreApplication>
#include <QMutex>
#include <QColor>
#include "DatedOptional.hpp"





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
		DatedOptional<QString> mAuthor;
		DatedOptional<QString> mTitle;
		DatedOptional<QString> mGenre;
		DatedOptional<double>  mMeasuresPerMinute;

		Tag() = default;
		Tag(const Tag & aOther) = default;
		Tag(
			const QString & aAuthor,
			const QString & aTitle,
			const QString & aGenre,
			double aMeasuresPerMinute
		):
			mAuthor(aAuthor),
			mTitle(aTitle),
			mGenre(aGenre),
			mMeasuresPerMinute(aMeasuresPerMinute)
		{
		}

		Tag(
			const QString & aAuthor,
			const QString & aTitle,
			const QString & aGenre
		):
			mAuthor(aAuthor),
			mTitle(aTitle),
			mGenre(aGenre)
		{
		}

		Tag(
			const QString & aAuthor,
			const QString & aTitle
		):
			mAuthor(aAuthor),
			mTitle(aTitle)
		{
		}

		constexpr bool operator == (const Tag & aOther) const noexcept
		{
			return (
				(mTitle == aOther.mTitle) &&
				(mAuthor == aOther.mAuthor) &&
				(mGenre == aOther.mGenre) &&
				(mMeasuresPerMinute == aOther.mMeasuresPerMinute)
			);
		}

		constexpr bool operator != (const Tag & aOther) const noexcept
		{
			return !(operator==(aOther));
		}
	};


	/** Rating in various categories (#100) */
	struct Rating
	{
		DatedOptional<double> mRhythmClarity;
		DatedOptional<double> mGenreTypicality;
		DatedOptional<double> mPopularity;
		DatedOptional<double> mLocal;
	};


	/** Data that can be shared among multiple files that represent the same song (same hash). */
	struct SharedData
	{
		QByteArray mHash;
		DatedOptional<double> mLength;
		DatedOptional<QDateTime> mLastPlayed;
		Rating mRating;
		Tag mTagManual;
		mutable QMutex mMtx;                   ///< Mutex protecting mDuplicates against multithreaded access
		std::vector<Song *> mDuplicates;       ///< All songs having the same hash
		DatedOptional<double> mSkipStart;      ///< Where to start playing
		DatedOptional<QString> mNotes;
		DatedOptional<QColor> mBgColor;        ///< BgColor used for displaying the song in ClassroomWindow
		DatedOptional<double> mDetectedTempo;  ///< Tempo that was detected using TempoDetect

		SharedData(
			const QByteArray & aHash,
			DatedOptional<double> && aLength,
			DatedOptional<QDateTime> && aLastPlayed,
			Rating && aRating,
			Tag && aTagManual,
			DatedOptional<double> && aSkipStart,
			DatedOptional<QString> && aNotes,
			DatedOptional<QColor> && aBgColor,
			DatedOptional<double> && aDetectedTempo
		):
			mHash(aHash),
			mLength(std::move(aLength)),
			mLastPlayed(std::move(aLastPlayed)),
			mRating(std::move(aRating)),
			mTagManual(std::move(aTagManual)),
			mSkipStart(std::move(aSkipStart)),
			mNotes(std::move(aNotes)),
			mBgColor(std::move(aBgColor)),
			mDetectedTempo(std::move(aDetectedTempo))
		{
		}

		SharedData(
			const QByteArray & aHash,
			double aLength
		):
			mHash(aHash),
			mLength(aLength)
		{
		}

		void addDuplicate(Song * aDuplicate);
		void delDuplicate(const Song * aDuplicate);

		/** Returns the count of all duplicates for this SharedData. Thread-safe. */
		size_t duplicatesCount() const;

		/** Returns *a copy* of all the duplicates for this SharedData. Thread-safe. */
		std::vector<Song *> duplicates() const;
	};

	using SharedDataPtr = std::shared_ptr<SharedData>;


	/** Creates a new instance that has only the file name and size set, all the other metadata are empty.
	Used when adding new files. */
	explicit Song(
		const QString & aFileName,
		SharedDataPtr aSharedData
	);

	/** Creates a new instance with all fields set.
	Used when loading songs from the DB. */
	explicit Song(
		QString && aFileName,
		SharedDataPtr aSharedData,
		Tag && aTagFileName,
		Tag && aTagId3,
		QVariant && aLastTagRescanned,
		QVariant && aNumTagRescanAttempts
	);

	Song(const Song & aSong) = delete;
	Song(Song && aSong) = delete;

	~Song();

	const QString & fileName() const { return mFileName; }
	const QByteArray & hash() const { return mSharedData->mHash; }
	const Tag & tagManual() const { return mSharedData->mTagManual; }
	const Tag & tagFileName() const { return mTagFileName; }
	const Tag & tagId3() const { return mTagId3; }
	const QVariant & lastTagRescanned() const { return mLastTagRescanned; }
	const QVariant & numTagRescanAttempts() const { return mNumTagRescanAttempts; }
	const SharedDataPtr & sharedData() const { return mSharedData; }
	const DatedOptional<QString> & notes() const { return mSharedData->mNotes; }

	// These return the value from the first tag which has the value valid, in the order or Manual, Id3, FileName
	const DatedOptional<QString> & primaryAuthor() const;
	const DatedOptional<QString> & primaryTitle() const;
	const DatedOptional<QString> & primaryGenre() const;
	const DatedOptional<double>  & primaryMeasuresPerMinute() const;

	// These return values from the shared data, if available:
	const DatedOptional<double> &    length()     const { return mSharedData->mLength; }
	const DatedOptional<QDateTime> & lastPlayed() const { return mSharedData->mLastPlayed; }
	const Rating &   rating()     const { return mSharedData->mRating; }
	const DatedOptional<double> skipStart() const;
	const DatedOptional<QColor> & bgColor() const { return mSharedData->mBgColor; }
	const DatedOptional<double> detectedTempo() const { return mSharedData->mDetectedTempo; }

	/** Returns whether the disk file still exists and it matches our stored hash. */
	bool isStillValid() const;

	/** Updates the length of the song, in seconds. */
	void setLength(double aLength);

	/** Updates the filename stored in the song.
	Doesn't rename the file itself.
	Doesn't update the filename tag. */
	void setFileName(const QString & aFileName);

	// Setters that redirect into the Manual tag:
	void setAuthor(QVariant aAuthor) { mSharedData->mTagManual.mAuthor = aAuthor; }
	void setTitle(QVariant aTitle) { mSharedData->mTagManual.mTitle = aTitle; }
	void setGenre(const QString & aGenre) { mSharedData->mTagManual.mGenre = aGenre; }
	void setMeasuresPerMinute(double aMeasuresPerMinute) { mSharedData->mTagManual.mMeasuresPerMinute = aMeasuresPerMinute; }

	// Setters for specific tags:
	void setManualAuthor(QVariant aAuthor) { mSharedData->mTagManual.mAuthor = aAuthor; }
	void setManualTitle(QVariant aTitle) { mSharedData->mTagManual.mTitle = aTitle; }
	void setManualGenre(const QString & aGenre) { mSharedData->mTagManual.mGenre = aGenre; }
	void setManualMeasuresPerMinute(double aMeasuresPerMinute) { mSharedData->mTagManual.mMeasuresPerMinute = aMeasuresPerMinute; }
	void setManualTag(const Tag & aTag) { mSharedData->mTagManual = aTag; }
	void setId3Author(const QString & aAuthor) { mTagId3.mAuthor = aAuthor; }
	void setId3Title(const QString & aTitle)   { mTagId3.mTitle  = aTitle; }
	void setId3Genre(const QString & aGenre)   { mTagId3.mGenre  = aGenre; }
	void setId3MeasuresPerMinute(double aMPM)  { mTagId3.mMeasuresPerMinute = aMPM; }
	void setId3Tag(const Tag & aTag) { mTagId3 = aTag; }
	void setFileNameAuthor(const QString & aAuthor) { mTagFileName.mAuthor = aAuthor; }
	void setFileNameTitle(const QString & aTitle)   { mTagFileName.mTitle  = aTitle; }
	void setFileNameGenre(const QString & aGenre)   { mTagFileName.mGenre  = aGenre; }
	void setFileNameMeasuresPerMinute(double aMPM)  { mTagFileName.mMeasuresPerMinute = aMPM; }
	void setFileNameTag(const Tag & aTag) { mTagFileName = aTag; }

	// Basic setters:
	void setLastTagRescanned(const QDateTime & aLastTagRescanned) { mLastTagRescanned = aLastTagRescanned; }
	void setNumTagRescanAttempts(int aNumTagRescanAttempts) { mNumTagRescanAttempts = aNumTagRescanAttempts; }
	void setNotes(const QString & aNotes) { mSharedData->mNotes = aNotes; }

	// Setters that preserve the date information:
	void setNotes(const DatedOptional<QString> & aNotes) { mSharedData->mNotes = aNotes; }

	// Setters that move-preserve the date information:
	void setNotes(DatedOptional<QString> && aNotes) { mSharedData->mNotes = std::move(aNotes); }

	/** Sets the local rating, if SharedData is present; otherwise ignored. */
	void setLocalRating(double aValue);

	/** Sets the skip start, if SharedData is present; otherwise ignored. */
	void setSkipStart(double aSeconds);

	/** Removes the skip-start, if SharedData is present; otherwise ignored. */
	void delSkipStart();

	/** Sets the BgColor, if SharedData is present; otherwise ignored. */
	void setBgColor(const QColor & aBgColor);

	/** Removes all properties from the Manual tag. */
	void clearManualTag();

	/** Returns true if a tag rescan is needed for the song
	(the tags are empty and the scan hasn't been performed already). */
	bool needsTagRescan() const;

	/** Returns all the warnings that this song has, each as a separate string.
	Returns an empty string list if the song has no warnings. */
	QStringList getWarnings() const;

	/** Returns the first of the three variants that is non-empty.
	Returns the third if all are null.*/
	template <typename T> static const DatedOptional<T> & primaryValue(
		const DatedOptional<T> & aFirst,
		const DatedOptional<T> & aSecond,
		const DatedOptional<T> & aThird
	)
	{
		if (!aFirst.isEmpty())
		{
			return aFirst;
		}
		if (!aSecond.isEmpty())
		{
			return aSecond;
		}
		return aThird;
	}

	/** Returns the competition tempo range for the specified genre.
	If the genre is not known, returns 0 .. MAX_USHORT */
	static std::pair<double, double> competitionTempoRangeForGenre(const QString & aGenre);

	/** Returns the tempo range to be used for tempo detection, based on the specified genre.
	If the genre is not known, returns the generic range of 17 .. 80. */
	static std::pair<int, int> detectionTempoRangeForGenre(const QString & aGenre);

	/** Returns the most likely MPM value given the MPM or BPM input and genre.
	If the input is double, triple or quadruple of a competition range MPM, return 1/2, 1/3 or 1/4 of the input,
	otherwise returns the input. */
	static double adjustMpm(double aInput, const QString & aGenre);

	/** Returns all the songs that have the same hash as this song, including this. */
	std::vector<Song *> duplicates();

	/** Returns all recognized genres, in a format suitable for QComboBox items. */
	static QStringList recognizedGenres();


protected:

	QString mFileName;
	SharedDataPtr mSharedData;
	Tag mTagFileName;
	Tag mTagId3;
	QVariant mLastTagRescanned;
	QVariant mNumTagRescanAttempts;

	/** An empty variant returned when there's no shared data for a song */
	static QVariant mEmpty;

	/** An empty rating returned when there's no shared ddata for a song. */
	static Rating mEmptyRating;
};

Q_DECLARE_METATYPE(SongPtr);
Q_DECLARE_METATYPE(Song::SharedDataPtr);

#include "Song.hpp"
#include <cassert>
#include <QVariant>
#include <QDebug>
#include "Utils.hpp"





QVariant Song::mEmpty;
Song::Rating Song::mEmptyRating;





Song::Song(
	const QString & aFileName,
	SharedDataPtr aSharedData
):
	mFileName(aFileName),
	mSharedData(aSharedData)
{
	aSharedData->addDuplicate(this);
}





Song::Song(
	QString && aFileName,
	SharedDataPtr aSharedData,
	Tag && aTagFileName,
	Tag && aTagId3,
	QVariant && aLastTagRescanned,
	QVariant && aNumTagRescanAttempts
):
	mFileName(std::move(aFileName)),
	mSharedData(aSharedData),
	mTagFileName(std::move(aTagFileName)),
	mTagId3(std::move(aTagId3)),
	mLastTagRescanned(std::move(aLastTagRescanned)),
	mNumTagRescanAttempts(std::move(aNumTagRescanAttempts))
{
	aSharedData->addDuplicate(this);
}





Song::~Song()
{
	mSharedData->delDuplicate(this);
}





const DatedOptional<QString> & Song::primaryAuthor() const
{
	return primaryValue(
		mSharedData->mTagManual.mAuthor,
		mTagId3.mAuthor,
		mTagFileName.mAuthor
	);
}





const DatedOptional<QString> & Song::primaryTitle() const
{
	return primaryValue(
		mSharedData->mTagManual.mTitle,
		mTagId3.mTitle,
		mTagFileName.mTitle
	);
}





const DatedOptional<QString> & Song::primaryGenre() const
{
	return primaryValue(
		mSharedData->mTagManual.mGenre,
		mTagId3.mGenre,
		mTagFileName.mGenre
	);
}





const DatedOptional<double> & Song::primaryMeasuresPerMinute() const
{
	return primaryValue(
		mSharedData->mTagManual.mMeasuresPerMinute,
		mTagId3.mMeasuresPerMinute,
		mTagFileName.mMeasuresPerMinute
	);
}





const DatedOptional<double> Song::skipStart() const
{
	return mSharedData->mSkipStart;
}





void Song::setLength(double aLength)
{
	mSharedData->mLength = aLength;
}





void Song::setLocalRating(double aValue)
{
	mSharedData->mRating.mLocal = aValue;
}





void Song::setSkipStart(double aSeconds)
{
	mSharedData->mSkipStart = aSeconds;
}





void Song::delSkipStart()
{
	mSharedData->mSkipStart.reset();
}





void Song::setBgColor(const QColor & aBgColor)
{
	if (mSharedData != nullptr)
	{
		mSharedData->mBgColor = aBgColor;
	}
}





void Song::clearManualTag()
{
	mSharedData->mTagManual.mAuthor.reset();
	mSharedData->mTagManual.mTitle.reset();
	mSharedData->mTagManual.mGenre.reset();
	mSharedData->mTagManual.mMeasuresPerMinute.reset();
}





bool Song::needsTagRescan() const
{
	// Any invalid metadata signals that the rescan hasn't been done
	// A rescan would have set the values (to empty strings if it failed)
	// TODO: Rescan if the file's last changed time grows over the last metadata updated value
	return (
		!mTagFileName.mAuthor.isPresent() ||
		!mTagId3.mAuthor.isPresent()
	);
}





QStringList Song::getWarnings() const
{
	QStringList res;

	// If auto-detected genres are different and there's no override, report:
	if (
		mSharedData->mTagManual.mGenre.isEmpty() &&  // Manual override not set
		!mTagFileName.mGenre.isEmpty() &&
		!mTagId3.mGenre.isEmpty() &&
		(mTagId3.mGenre.value() != mTagFileName.mGenre.value())   // ID3 genre not equal to FileName genre
	)
	{
		res.append(tr("Genre detection is confused, please provide a manual override."));
	}

	// If the detected MPM is way outside the primary genre's competition range, report:
	if (!mSharedData->mTagManual.mMeasuresPerMinute.isPresent())  // Allow the user to override the warning
	{
		auto primaryMPM = mTagId3.mMeasuresPerMinute.isPresent() ? mTagId3.mMeasuresPerMinute : mTagFileName.mMeasuresPerMinute;
		if (primaryMPM.isPresent())
		{
			auto mpm = primaryMPM.value();
			auto range = competitionTempoRangeForGenre(primaryGenre().valueOrDefault());
			if (mpm < range.first * 0.7)
			{
				res.append(tr("The detected tempo is suspiciously low: Lowest competition tempo: %1; detected tempo: %2")
					.arg(QString::number(range.first, 'f', 1))
					.arg(QString::number(mpm, 'f', 1))
				);
			}
			else if (mpm > range.second * 1.05)
			{
				res.append(tr("The detected tempo is suspiciously high: Highest competition tempo: %1; detected tempo: %2")
					.arg(QString::number(range.second, 'f', 1))
					.arg(QString::number(mpm, 'f', 1))
				);
			}
		}
	}
	return res;
}





std::pair<double, double> Song::competitionTempoRangeForGenre(const QString & aGenre)
{
	// Map of genre -> tempo range (Source: http://www.sut.cz/soutezni-rad-hobby-dance/#par14 )
	static const std::map<QString, std::pair<double, double>> competitionTempoRanges =
	{
		{"SW", {27, 30}},
		{"TG", {30, 32}},
		{"VW", {58, 60}},
		{"SF", {27, 30}},
		{"QS", {48, 52}},
		{"SB", {48, 52}},
		{"CH", {27, 32}},
		{"RU", {23, 26}},
		{"PD", {58, 60}},
		{"JI", {40, 44}},
		{"PO", {56, 60}},

		// Non-competition, but still reasonable tempo limits:
		{"BL", {20, 23}},
		{"RO", {28, 34}},
	};

	// Find the genre:
	auto itr = competitionTempoRanges.find(aGenre.toUpper());
	if (itr == competitionTempoRanges.end())
	{
		return std::make_pair(0, std::numeric_limits<unsigned short>::max());
	}
	return itr->second;
}





std::pair<int, int> Song::detectionTempoRangeForGenre(const QString & aGenre)
{
	// Map of genre -> tempo range
	static const std::map<QString, std::pair<int, int>> detectionTempoRanges =
	{
		{"SW", {25, 33}},
		{"TG", {28, 35}},
		{"VW", {40, 65}},
		{"SF", {25, 35}},
		{"QS", {45, 55}},
		{"SB", {34, 55}},
		{"CH", {22, 35}},
		{"RU", {17, 30}},
		{"PD", {40, 68}},
		{"JI", {28, 46}},
		{"BL", {15, 30}},
		{"PO", {40, 80}},
		{"RO", {22, 35}},  // Same as CH
	};

	// Find the genre:
	auto itr = detectionTempoRanges.find(aGenre.toUpper());
	if (itr == detectionTempoRanges.end())
	{
		return std::make_pair(17, 80);
	}
	return itr->second;
}





double Song::adjustMpm(double aInput, const QString & aGenre)
{
	auto mpmRange = detectionTempoRangeForGenre(aGenre);
	if (mpmRange.first < 0)
	{
		// No known competition range, don't adjust anything
		return aInput;
	}

	if (aInput < mpmRange.second)
	{
		// Input is low enough, no adjustment needed
		return aInput;
	}

	// Half, quarter, or third (last) the input, if the result is in range:
	double divisor[] = {2, 4, 3};
	for (auto d: divisor)
	{
		if ((aInput >= mpmRange.first * d) && (aInput <= mpmRange.second * d))
		{
			return aInput / d;
		}
	}

	// No adjustment was valid, return unchanged:
	return aInput;
}





std::vector<Song *> Song::duplicates()
{
	return mSharedData->duplicates();
}





QStringList Song::recognizedGenres()
{
	return
	{
		"SW",
		"TG",
		"VW",
		"SF",
		"QS",
		"SB",
		"CH",
		"RU",
		"PD",
		"JI",
		"PO",
		"BL",
		"MA",
		"SL",
		"RO",
	};
}





////////////////////////////////////////////////////////////////////////////////
// Song::SharedData:

void Song::SharedData::addDuplicate(Song * aDuplicate)
{
	QMutexLocker lock(&mMtx);
	for (const auto & d: mDuplicates)
	{
		if (d == aDuplicate)
		{
			// Already added, bail out.
			return;
		}
	}
	mDuplicates.push_back(aDuplicate);
}





void Song::SharedData::delDuplicate(const Song * aDuplicate)
{
	QMutexLocker lock(&mMtx);
	for (auto itr = mDuplicates.begin(), end = mDuplicates.end(); itr != end; ++itr)
	{
		if (*itr == aDuplicate)
		{
			mDuplicates.erase(itr);
			break;
		}
	}
}





size_t Song::SharedData::duplicatesCount() const
{
	QMutexLocker lock(&mMtx);
	return mDuplicates.size();
}





std::vector<Song *> Song::SharedData::duplicates() const
{
	QMutexLocker lock(&mMtx);
	return mDuplicates;
}

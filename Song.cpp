#include "Song.h"
#include <assert.h>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>





QVariant Song::m_Empty;
Song::Rating Song::m_EmptyRating;





Song::Song(
	const QString & a_FileName,
	qulonglong a_FileSize
):
	m_FileName(a_FileName),
	m_FileSize(a_FileSize)
{
}





Song::Song(
	QString && a_FileName,
	qulonglong a_FileSize,
	QVariant && a_Hash,
	Tag && a_TagManual,
	Tag && a_TagFileName,
	Tag && a_TagId3,
	QVariant && a_LastTagRescanned,
	QVariant && a_NumTagRescanAttempts
):
	m_FileName(std::move(a_FileName)),
	m_FileSize(a_FileSize),
	m_Hash(std::move(a_Hash)),
	m_TagManual(std::move(a_TagManual)),
	m_TagFileName(std::move(a_TagFileName)),
	m_TagId3(std::move(a_TagId3)),
	m_LastTagRescanned(std::move(a_LastTagRescanned)),
	m_NumTagRescanAttempts(std::move(a_NumTagRescanAttempts))
{
}





Song::~Song()
{
	if (m_SharedData != nullptr)
	{
		m_SharedData->delDuplicate(shared_from_this());
	}
}





const DatedOptional<QString> & Song::primaryAuthor() const
{
	return primaryValue(
		m_TagManual.m_Author,
		m_TagId3.m_Author,
		m_TagFileName.m_Author
	);
}





const DatedOptional<QString> & Song::primaryTitle() const
{
	return primaryValue(
		m_TagManual.m_Title,
		m_TagId3.m_Title,
		m_TagFileName.m_Title
	);
}





const DatedOptional<QString> & Song::primaryGenre() const
{
	return primaryValue(
		m_TagManual.m_Genre,
		m_TagId3.m_Genre,
		m_TagFileName.m_Genre
	);
}





const DatedOptional<double> & Song::primaryMeasuresPerMinute() const
{
	return primaryValue(
		m_TagManual.m_MeasuresPerMinute,
		m_TagId3.m_MeasuresPerMinute,
		m_TagFileName.m_MeasuresPerMinute
	);
}





void Song::setLength(double a_Length)
{
	assert(m_SharedData != nullptr);

	if (m_SharedData != nullptr)
	{
		m_SharedData->m_Length = a_Length;
	}
}





void Song::setHash(QByteArray && a_Hash)
{
	assert(m_SharedData == nullptr);
	assert(!m_Hash.isValid());

	m_Hash = std::move(a_Hash);
}





void Song::setSharedData(Song::SharedDataPtr a_SharedData)
{
	assert(m_Hash.toByteArray() == a_SharedData->m_Hash);
	m_SharedData = a_SharedData;
	m_SharedData->addDuplicate(shared_from_this());
}





void Song::setLocalRating(double a_Value)
{
	if (m_SharedData != nullptr)
	{
		m_SharedData->m_Rating.m_Local = a_Value;
	}
}





bool Song::needsTagRescan() const
{
	// Any invalid metadata signals that the rescan hasn't been done
	// A rescan would have set the values (to empty strings if it failed)
	// TODO: Rescan if the file's last changed time grows over the last metadata updated value
	return (
		!m_TagFileName.m_Author.isPresent() ||
		!m_TagId3.m_Author.isPresent()
	);
}





QStringList Song::getWarnings() const
{
	QStringList res;

	// If auto-detected genres are different and there's no override, report:
	if (
		!m_TagManual.m_Genre.isEmpty() &&  // Manual override not set
		m_TagFileName.m_Genre.isPresent() &&
		m_TagId3.m_Genre.isPresent() &&
		(m_TagId3.m_Genre.value() != m_TagFileName.m_Genre.value())   // ID3 genre not equal to FileName genre
	)
	{
		res.append(tr("Genre detection is confused, please provide a manual override."));
	}

	// If the detected MPM is way outside the primary genre's competition range, report:
	if (!m_TagManual.m_MeasuresPerMinute.isPresent())  // Allow the user to override the warning
	{
		auto primaryMPM = m_TagId3.m_MeasuresPerMinute.isPresent() ? m_TagId3.m_MeasuresPerMinute : m_TagFileName.m_MeasuresPerMinute;
		if (primaryMPM.isPresent())
		{
			auto mpm = primaryMPM.value();
			auto range = competitionTempoRangeForGenre(primaryGenre().value());
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





std::pair<double, double> Song::competitionTempoRangeForGenre(const QString & a_Genre)
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
	};

	// Find the genre:
	auto itr = competitionTempoRanges.find(a_Genre.toUpper());
	if (itr == competitionTempoRanges.end())
	{
		return std::make_pair(0, std::numeric_limits<unsigned short>::max());
	}
	return itr->second;
}





std::vector<SongPtr> Song::duplicates()
{
	if (m_SharedData == nullptr)
	{
		return {shared_from_this()};
	}
	return m_SharedData->m_Duplicates;
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

void Song::SharedData::addDuplicate(SongPtr a_Duplicate)
{
	QMutexLocker lock(&m_Mtx);
	m_Duplicates.push_back(a_Duplicate);
}





void Song::SharedData::delDuplicate(SongPtr a_Duplicate)
{
	QMutexLocker lock(&m_Mtx);
	for (auto itr = m_Duplicates.begin(), end = m_Duplicates.end(); itr != end; ++itr)
	{
		if (*itr == a_Duplicate)
		{
			m_Duplicates.erase(itr);
			break;
		}
	}
}





size_t Song::SharedData::duplicatesCount() const
{
	QMutexLocker lock(&m_Mtx);
	return m_Duplicates.size();
}

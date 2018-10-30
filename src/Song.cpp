#include "Song.hpp"
#include <cassert>
#include <QVariant>
#include <QDebug>
#include "Utils.hpp"





QVariant Song::m_Empty;
Song::Rating Song::m_EmptyRating;





Song::Song(
	const QString & a_FileName,
	SharedDataPtr a_SharedData
):
	m_FileName(a_FileName),
	m_SharedData(a_SharedData)
{
	a_SharedData->addDuplicate(this);
}





Song::Song(
	QString && a_FileName,
	SharedDataPtr a_SharedData,
	Tag && a_TagFileName,
	Tag && a_TagId3,
	QVariant && a_LastTagRescanned,
	QVariant && a_NumTagRescanAttempts
):
	m_FileName(std::move(a_FileName)),
	m_SharedData(a_SharedData),
	m_TagFileName(std::move(a_TagFileName)),
	m_TagId3(std::move(a_TagId3)),
	m_LastTagRescanned(std::move(a_LastTagRescanned)),
	m_NumTagRescanAttempts(std::move(a_NumTagRescanAttempts))
{
	a_SharedData->addDuplicate(this);
}





Song::~Song()
{
	m_SharedData->delDuplicate(this);
}





const DatedOptional<QString> & Song::primaryAuthor() const
{
	return primaryValue(
		m_SharedData->m_TagManual.m_Author,
		m_TagId3.m_Author,
		m_TagFileName.m_Author
	);
}





const DatedOptional<QString> & Song::primaryTitle() const
{
	return primaryValue(
		m_SharedData->m_TagManual.m_Title,
		m_TagId3.m_Title,
		m_TagFileName.m_Title
	);
}





const DatedOptional<QString> & Song::primaryGenre() const
{
	return primaryValue(
		m_SharedData->m_TagManual.m_Genre,
		m_TagId3.m_Genre,
		m_TagFileName.m_Genre
	);
}





const DatedOptional<double> & Song::primaryMeasuresPerMinute() const
{
	return primaryValue(
		m_SharedData->m_TagManual.m_MeasuresPerMinute,
		m_TagId3.m_MeasuresPerMinute,
		m_TagFileName.m_MeasuresPerMinute
	);
}





const DatedOptional<double> Song::skipStart() const
{
	return m_SharedData->m_SkipStart;
}





void Song::setLength(double a_Length)
{
	m_SharedData->m_Length = a_Length;
}





void Song::setLocalRating(double a_Value)
{
	m_SharedData->m_Rating.m_Local = a_Value;
}





void Song::setSkipStart(double a_Seconds)
{
	m_SharedData->m_SkipStart = a_Seconds;
}





void Song::delSkipStart()
{
	m_SharedData->m_SkipStart.reset();
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
		m_SharedData->m_TagManual.m_Genre.isEmpty() &&  // Manual override not set
		!m_TagFileName.m_Genre.isEmpty() &&
		!m_TagId3.m_Genre.isEmpty() &&
		(m_TagId3.m_Genre.value() != m_TagFileName.m_Genre.value())   // ID3 genre not equal to FileName genre
	)
	{
		res.append(tr("Genre detection is confused, please provide a manual override."));
	}

	// If the detected MPM is way outside the primary genre's competition range, report:
	if (!m_SharedData->m_TagManual.m_MeasuresPerMinute.isPresent())  // Allow the user to override the warning
	{
		auto primaryMPM = m_TagId3.m_MeasuresPerMinute.isPresent() ? m_TagId3.m_MeasuresPerMinute : m_TagFileName.m_MeasuresPerMinute;
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
		{"BL", {20, 23}},
	};

	// Find the genre:
	auto itr = competitionTempoRanges.find(a_Genre.toUpper());
	if (itr == competitionTempoRanges.end())
	{
		return std::make_pair(0, std::numeric_limits<unsigned short>::max());
	}
	return itr->second;
}





std::vector<Song *> Song::duplicates()
{
	return m_SharedData->duplicates();
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





double Song::foldTempoToMPM(double a_Tempo, const DatedOptional<QString> & a_Genre)
{
	auto range = competitionTempoRangeForGenre(a_Genre.valueOrDefault());
	if (range.first <= 0.1)
	{
		qDebug() << "No valid range for genre " << a_Genre.valueOrDefault() << ", bailing out of MPM adjustment.";
		return a_Tempo;
	}
	range.first = range.first * 0.8;  // Allow 20 % slower songs
	range.second = range.second * 1.1;  // Allow 10 % faster songs
	if (a_Tempo < range.first)
	{
		while (a_Tempo < range.first)
		{
			a_Tempo = a_Tempo * 2;
		}
		return a_Tempo;
	}

	// The tempo is faster than the regular range
	// For SW and VW, try first if it makes sense to divide by 3:
	const auto & genre = a_Genre.value();
	if ((genre == "SW") || (genre == "VW"))
	{
		for (int d: {1, 2, 4, 8, 16})
		{
			auto tempoThird = a_Tempo / 3;
			if (Utils::isInRange(tempoThird / d, range.first, range.second))
			{
				return tempoThird / d;
			}
		}
	}

	// All other genres only half the tempo until it is lower than the max:
	while (a_Tempo > range.second)
	{
		a_Tempo = a_Tempo / 2;
	}
	return a_Tempo;
}






////////////////////////////////////////////////////////////////////////////////
// Song::SharedData:

void Song::SharedData::addDuplicate(Song * a_Duplicate)
{
	QMutexLocker lock(&m_Mtx);
	for (const auto & d: m_Duplicates)
	{
		if (d == a_Duplicate)
		{
			// Already added, bail out.
			return;
		}
	}
	m_Duplicates.push_back(a_Duplicate);
}





void Song::SharedData::delDuplicate(const Song * a_Duplicate)
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





std::vector<Song *> Song::SharedData::duplicates() const
{
	QMutexLocker lock(&m_Mtx);
	return m_Duplicates;
}

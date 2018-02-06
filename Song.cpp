#include "Song.h"
#include <QSqlQuery>
#include <QVariant>





Song::Song(
	const QString & a_FileName,
	qulonglong a_FileSize,
	qlonglong a_DbRowId
):
	m_FileName(a_FileName),
	m_FileSize(a_FileSize),
	m_DbRowId(a_DbRowId)
{
}





Song::Song(
	QString && a_FileName,
	qulonglong a_FileSize,
	qlonglong a_DbRowId,
	QVariant && a_Hash,
	QVariant && a_Length,
	QVariant && a_Genre,
	QVariant && a_MeasuresPerMinute,
	QVariant && a_Author,
	QVariant && a_Title,
	QVariant && a_LastPlayed,
	QVariant && a_Rating,
	QVariant && a_LastMetadataUpdated
):
	m_FileName(std::move(a_FileName)),
	m_FileSize(a_FileSize),
	m_DbRowId(a_DbRowId),
	m_Hash(std::move(a_Hash)),
	m_Length(std::move(a_Length)),
	m_Genre(std::move(a_Genre)),
	m_MeasuresPerMinute(std::move(a_MeasuresPerMinute)),
	m_LastPlayed(std::move(a_LastPlayed)),
	m_Rating(std::move(a_Rating)),
	m_Author(std::move(a_Author)),
	m_Title(std::move(a_Title)),
	m_LastMetadataUpdated(std::move(a_LastMetadataUpdated))
{
	// Nothing needed
}





void Song::setLength(double a_Length)
{
	m_Length = a_Length;
}





bool Song::needsMetadataRescan() const
{
	// Invalid hash always needs a rescan
	if (!m_Hash.isValid())
	{
		return true;
	}

	// Length is needed, update it if invalid:
	if (!m_Length.isValid() || (m_Length.toDouble() < 0))
	{
		return true;
	}

	// Invalid metadata trigger a rescan only if it has been enough time since the last metadata update:
	if (!m_LastMetadataUpdated.isValid())
	{
		return true;
	}
	if (
		(m_MeasuresPerMinute < 0) ||
		(m_Rating < 0) ||
		!m_Author.isValid() ||
		!m_Title.isValid()
	)
	{
		return (m_LastMetadataUpdated.toDateTime() < QDateTime::currentDateTimeUtc().addDays(-14));
	}
	return false;
}






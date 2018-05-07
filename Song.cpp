#include "Song.h"
#include <QSqlQuery>
#include <QVariant>





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
	QVariant && a_Length,
	Tag && a_TagManual,
	Tag && a_TagFileName,
	Tag && a_TagId3,
	QVariant && a_LastPlayed,
	QVariant && a_Rating,
	QVariant && a_LastMetadataUpdated
):
	m_FileName(std::move(a_FileName)),
	m_FileSize(a_FileSize),
	m_Hash(std::move(a_Hash)),
	m_Length(std::move(a_Length)),
	m_TagManual(std::move(a_TagManual)),
	m_TagFileName(std::move(a_TagFileName)),
	m_TagId3(std::move(a_TagId3)),
	m_LastPlayed(std::move(a_LastPlayed)),
	m_Rating(std::move(a_Rating)),
	m_LastMetadataUpdated(std::move(a_LastMetadataUpdated))
{
}





const QVariant & Song::author() const
{
	if (m_TagManual.m_Author.isValid())
	{
		return m_TagManual.m_Author;
	}
	else if (m_TagId3.m_Author.isValid())
	{
		return m_TagId3.m_Author;
	}
	return m_TagFileName.m_Author;
}





const QVariant & Song::title() const
{
	if (m_TagManual.m_Title.isValid())
	{
		return m_TagManual.m_Title;
	}
	else if (m_TagId3.m_Title.isValid())
	{
		return m_TagId3.m_Title;
	}
	return m_TagFileName.m_Title;
}





const QVariant & Song::genre() const
{
	if (m_TagManual.m_Genre.isValid())
	{
		return m_TagManual.m_Genre;
	}
	else if (m_TagId3.m_Genre.isValid())
	{
		return m_TagId3.m_Genre;
	}
	return m_TagFileName.m_Genre;
}





const QVariant & Song::measuresPerMinute() const
{
	if (m_TagManual.m_MeasuresPerMinute.isValid())
	{
		return m_TagManual.m_MeasuresPerMinute;
	}
	else if (m_TagId3.m_MeasuresPerMinute.isValid())
	{
		return m_TagId3.m_MeasuresPerMinute;
	}
	return m_TagFileName.m_MeasuresPerMinute;
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
		(m_TagId3.m_MeasuresPerMinute < 0) ||
		!m_TagId3.m_Author.isValid() ||
		!m_TagId3.m_Title.isValid()
	)
	{
		return (m_LastMetadataUpdated.toDateTime() < QDateTime::currentDateTimeUtc().addDays(-14));
	}
	return false;
}





void Song::copyMetadataFrom(const Song & a_Src)
{
	m_Length              = a_Src.m_Length;
	m_TagManual           = a_Src.m_TagManual;
	m_TagFileName         = a_Src.m_TagFileName;
	m_TagId3              = a_Src.m_TagId3;
	m_LastPlayed          = a_Src.m_LastPlayed;
	m_Rating              = a_Src.m_Rating;
	m_LastMetadataUpdated = a_Src.m_LastMetadataUpdated;
}






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
	m_DbRowId(a_DbRowId),
	m_Length(-1),
	m_MeasuresPerMinute(-1)
{
}





Song::Song(QString && a_FileName,
	qulonglong a_FileSize,
	qlonglong a_DbRowId,
	QByteArray && a_Hash,
	QVariant a_Length,
	QString && a_Genre,
	QVariant a_MeasuresPerMinute,
	QDateTime a_LastPlayed,
	QVariant a_Rating
):
	m_FileName(std::move(a_FileName)),
	m_FileSize(a_FileSize),
	m_DbRowId(a_DbRowId),
	m_Hash(std::move(a_Hash)),
	m_Length(a_Length.isValid() ? a_Length.toDouble() : -1),
	m_Genre(std::move(a_Genre)),
	m_MeasuresPerMinute(a_MeasuresPerMinute.isValid() ? a_MeasuresPerMinute.toDouble() : -1),
	m_LastPlayed(a_LastPlayed),
	m_Rating(a_Rating.isValid() ? a_Rating.toDouble() : -1)
{
	// Nothing needed
}






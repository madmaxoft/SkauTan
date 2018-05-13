#include "Song.h"
#include <assert.h>
#include <QSqlQuery>
#include <QVariant>





QVariant Song::m_Empty;





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
}





bool Song::needsTagRescan() const
{
	// Any invalid metadata signals that the rescan hasn't been done
	// A rescan would have set the values (to empty strings if it failed)
	// TODO: Rescan if the file's last changed time grows over the last metadata updated value
	return (
		!m_TagFileName.m_Author.isValid() ||
		!m_TagId3.m_Author.isValid()
	);
}





void Song::copyTagsFrom(const Song & a_Src)
{
	m_TagManual           = a_Src.m_TagManual;
	m_TagFileName         = a_Src.m_TagFileName;
	m_TagId3              = a_Src.m_TagId3;
}






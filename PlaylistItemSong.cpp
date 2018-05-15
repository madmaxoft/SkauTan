#include "PlaylistItemSong.h"
#include <QAudioFormat>
#include "SongDecoder.h"





PlaylistItemSong::PlaylistItemSong(SongPtr a_Song):
	m_Song(a_Song)
{
}





QString PlaylistItemSong::displayName() const
{
	// TODO: Decide whether to show full filename, or just partial
	return m_Song->fileName();
}





QString PlaylistItemSong::displayAuthor() const
{
	return m_Song->primaryAuthor().toString();
}





QString PlaylistItemSong::displayTitle() const
{
	return m_Song->primaryTitle().toString();
}





double PlaylistItemSong::displayLength() const
{
	if (m_Song->length().isValid())
	{
		return m_Song->length().toDouble();
	}
	return -1;
}





QString PlaylistItemSong::displayGenre() const
{
	if (m_Song->primaryGenre().isValid())
	{
		return m_Song->primaryGenre().toString();
	}
	return QString();
}





double PlaylistItemSong::displayTempo() const
{
	if (m_Song->primaryMeasuresPerMinute().isValid())
	{
		return m_Song->primaryMeasuresPerMinute().toDouble();
	}
	return -1;
}





PlaybackBuffer * PlaylistItemSong::startDecoding(const QAudioFormat & a_Format)
{
	Q_UNUSED(a_Format);

	return new SongDecoder(a_Format, m_Song);
}

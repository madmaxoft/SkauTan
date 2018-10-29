#include "PlaylistItemSong.h"
#include <QAudioFormat>
#include "SongDecoder.h"





PlaylistItemSong::PlaylistItemSong(SongPtr a_Song, FilterPtr a_Filter):
	m_Song(a_Song),
	m_Filter(a_Filter),
	m_DurationLimit((a_Filter == nullptr) ? -1 : a_Filter->durationLimit().valueOr(-1))
{
}





QString PlaylistItemSong::displayName() const
{
	// TODO: Decide whether to show full filename, or just partial
	return m_Song->fileName();
}





QString PlaylistItemSong::displayAuthor() const
{
	return m_Song->primaryAuthor().valueOrDefault();
}





QString PlaylistItemSong::displayTitle() const
{
	return m_Song->primaryTitle().valueOrDefault();
}





double PlaylistItemSong::displayLength() const
{
	return m_Song->length().valueOr(-1);
}





QString PlaylistItemSong::displayGenre() const
{
	if (!m_Song->primaryGenre().isEmpty())
	{
		return m_Song->primaryGenre().value();
	}
	return QString();
}





double PlaylistItemSong::displayTempo() const
{
	if (m_Song->primaryMeasuresPerMinute().isPresent())
	{
		return m_Song->primaryMeasuresPerMinute().value();
	}
	return -1;
}





void PlaylistItemSong::setDurationLimit(double a_Seconds)
{
	m_DurationLimit = a_Seconds;
}





PlaybackBuffer * PlaylistItemSong::startDecoding(const QAudioFormat & a_Format)
{
	Q_UNUSED(a_Format);

	return new SongDecoder(a_Format, m_Song);
}

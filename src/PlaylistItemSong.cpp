#include "PlaylistItemSong.hpp"
#include <QAudioFormat>
#include "Audio/SongDecoder.hpp"





PlaylistItemSong::PlaylistItemSong(SongPtr aSong, FilterPtr aFilter):
	mSong(aSong),
	mFilter(aFilter),
	mDurationLimit((aFilter == nullptr) ? -1 : aFilter->durationLimit().valueOr(-1))
{
}





QString PlaylistItemSong::displayName() const
{
	// TODO: Decide whether to show full filename, or just partial
	return mSong->fileName();
}





QString PlaylistItemSong::displayAuthor() const
{
	return mSong->primaryAuthor().valueOrDefault();
}





QString PlaylistItemSong::displayTitle() const
{
	return mSong->primaryTitle().valueOrDefault();
}





double PlaylistItemSong::displayLength() const
{
	return mSong->length().valueOr(-1);
}





QString PlaylistItemSong::displayGenre() const
{
	if (!mSong->primaryGenre().isEmpty())
	{
		return mSong->primaryGenre().value();
	}
	return QString();
}





double PlaylistItemSong::displayTempo() const
{
	if (mSong->primaryMeasuresPerMinute().isPresent())
	{
		return mSong->primaryMeasuresPerMinute().value();
	}
	return -1;
}





void PlaylistItemSong::setDurationLimit(double aSeconds)
{
	mDurationLimit = aSeconds;
}





PlaybackBuffer * PlaylistItemSong::startDecoding(const QAudioFormat & aFormat)
{
	Q_UNUSED(aFormat);

	return new SongDecoder(aFormat, mSong);
}

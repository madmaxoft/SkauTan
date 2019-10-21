#ifndef PLAYLISTITEMSONG_H
#define PLAYLISTITEMSONG_H





#include "IPlaylistItem.hpp"
#include "Song.hpp"
#include "Filter.hpp"





/** Provides an IPlaylistItem interface for a Song. */
class PlaylistItemSong:
	public IPlaylistItem
{
	using Super = IPlaylistItem;


public:
	PlaylistItemSong(SongPtr aSong, FilterPtr aTemplateItem);


	// IPlaylistItem overrides, playlist-related functions:
	virtual QString displayName() const override;
	virtual QString displayAuthor() const override;
	virtual QString displayTitle() const override;
	virtual double displayLength() const override;
	virtual QString displayGenre() const override;
	virtual double displayTempo() const override;
	virtual double durationLimit() const override { return mDurationLimit; }
	virtual void setDurationLimit(double aSeconds) override;

	// IPlaylistItem overrides, playback-related functions:
	virtual PlaybackBuffer * startDecoding(const QAudioFormat & aFormat) override;

	SongPtr song() const { return mSong; }
	FilterPtr filter() const { return mFilter; }


protected:

	/** The song to be represented by this entry. */
	SongPtr mSong;

	/** The filter (of the template) that chose this song; nullptr if inserted explicitly by the user. */
	FilterPtr mFilter;

	/** The duration limit assigned to this item.
	Initially inherited from mFilter, but user-editable.
	-1 if no limit. */
	double mDurationLimit;
};





#endif // PLAYLISTITEMSONG_H

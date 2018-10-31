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
	PlaylistItemSong(SongPtr a_Song, FilterPtr a_TemplateItem);


	// IPlaylistItem overrides, playlist-related functions:
	virtual QString displayName() const override;
	virtual QString displayAuthor() const override;
	virtual QString displayTitle() const override;
	virtual double displayLength() const override;
	virtual QString displayGenre() const override;
	virtual double displayTempo() const override;
	virtual double durationLimit() const override { return m_DurationLimit; }
	virtual void setDurationLimit(double a_Seconds) override;

	// IPlaylistItem overrides, playback-related functions:
	virtual PlaybackBuffer * startDecoding(const QAudioFormat & a_Format) override;

	SongPtr song() const { return m_Song; }
	FilterPtr filter() const { return m_Filter; }


protected:

	/** The song to be represented by this entry. */
	SongPtr m_Song;

	/** The filter (of the template) that chose this song; nullptr if inserted explicitly by the user. */
	FilterPtr m_Filter;

	/** The duration limit assigned to this item.
	Initially inherited from m_Filter, but user-editable.
	-1 if no limit. */
	double m_DurationLimit;
};





#endif // PLAYLISTITEMSONG_H

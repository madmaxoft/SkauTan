#ifndef PLAYLISTITEMSONG_H
#define PLAYLISTITEMSONG_H





#include "IPlaylistItem.h"
#include"Song.h"
#include "Template.h"





/** Provides an IPlaylistItem interface for a Song. */
class PlaylistItemSong:
	public IPlaylistItem
{
	using Super = IPlaylistItem;


public:
	PlaylistItemSong(SongPtr a_Song, Template::ItemPtr a_TemplateItem);


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
	Template::ItemPtr templateItem() const { return m_TemplateItem; }


protected:

	/** The song to be represented by this entry. */
	SongPtr m_Song;

	/** The template that chose this song; nullptr if inserted directly by the user. */
	Template::ItemPtr m_TemplateItem;

	/** The duration limit assigned to this item.
	Initially inherited from m_TemplateItem, but user-editable.
	-1 if no limit. */
	double m_DurationLimit;
};





#endif // PLAYLISTITEMSONG_H

#ifndef PLAYLISTITEMSONG_H
#define PLAYLISTITEMSONG_H





#include "IPlaylistItem.h"
#include"Song.h"





/** Provides an IPlaylistItem interface for a Song. */
class PlaylistItemSong:
	public IPlaylistItem
{
	using Super = IPlaylistItem;


public:
	PlaylistItemSong(SongPtr a_Song);


	// IPlaylistItem overrides, playlist-related functions:
	virtual QString displayName() const override;
	virtual QString displayAuthor() const override;
	virtual QString displayTitle() const override;
	virtual double displayLength() const override;
	virtual QString displayGenre() const override;
	virtual double displayTempo() const override;

	// IPlaylistItem overrides, playback-related functions:
	virtual void startPlaying(QIODevice * a_AudioOutput, const QAudioFormat & a_Format) override;
	virtual void stopPlaying() override;


protected:

	SongPtr m_Song;
};





#endif // PLAYLISTITEMSONG_H

#ifndef PLAYLISTITEMSONG_H
#define PLAYLISTITEMSONG_H





#include "IPlaylistItem.h"





/** Provides an IPlaylistItem interface for a Song. */
class PlaylistItemSong:
	public IPlaylistItem
{
	using Super = IPlaylistItem;


public:
	PlaylistItemSong();
};





#endif // PLAYLISTITEMSONG_H

#ifndef PLAYLIST_H
#define PLAYLIST_H





#include <vector>
#include "IPlaylistItem.h"





/** Container for a single playlist.
Stores the items to be played, the currently played item, timings etc. */
class Playlist
{
public:
	Playlist();

protected:
	/** All the items on the playlist. */
	std::vector<IPlaylistItemPtr> m_Items;

	/** The item currently being played, or to be played when the playback is started. */
	IPlaylistItemPtr m_CurrentItem;

	/** If true, m_CurrentItem is being played back. */
	bool m_IsPlaying;

	/** The current position in playing back m_CurrentItem. Only valid when m_IsPlaying is true. */
	double m_CurrentPosition;
};





#endif // PLAYLIST_H

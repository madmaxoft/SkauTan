#ifndef PLAYLIST_H
#define PLAYLIST_H





#include <vector>
#include <QObject>
#include "IPlaylistItem.h"





/** Container for a single playlist.
Stores the items to be played, the currently played item, timings etc. */
class Playlist:
	public QObject
{
	using Super = QObject;
	Q_OBJECT

public:
	Playlist();

	const std::vector<IPlaylistItemPtr> & items() const { return m_Items; }

	/** Adds the specified item to the bottom of the playlist. */
	void addItem(IPlaylistItemPtr a_Item);

	/** Moves the item at a_FromIdex to a_ToIdx, shifting the items in between accordingly. */
	void moveItem(int a_FromIdx, int a_ToIdx);

protected:

	/** All the items on the playlist. */
	std::vector<IPlaylistItemPtr> m_Items;

	/** The item currently being played, or to be played when the playback is started. */
	IPlaylistItemPtr m_CurrentItem;

	/** If true, m_CurrentItem is being played back. */
	bool m_IsPlaying;

	/** The current position in playing back m_CurrentItem. Only valid when m_IsPlaying is true. */
	double m_CurrentPosition;


signals:

	/** Emitted after the specified item is added to the bottom of the playlist. */
	void itemAdded(IPlaylistItem * a_Item);

};

using PlaylistPtr = std::shared_ptr<Playlist>;





#endif // PLAYLIST_H

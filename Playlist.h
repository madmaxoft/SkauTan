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

	/** Moves the item at a_FromIdex to a_ToIdx, shifting the items in between accordingly.
	Doesn't emit any signal. */
	void moveItem(int a_FromIdx, int a_ToIdx);

	/** Deletes the item at the specified index.
	Emits the appropriate signals. */
	void deleteItem(int a_Index);

	/** Returns the item that is currently selected for playback.
	Note that this may return nullptr (no items in playlist). */
	IPlaylistItemPtr current() const;

protected:

	/** All the items on the playlist. */
	std::vector<IPlaylistItemPtr> m_Items;

	/** The index into m_Items for the item currently being played, or to be played when the playback is started. */
	size_t m_CurrentItemIdx;

	/** If true, current item is being played back. */
	bool m_IsPlaying;

	/** The current position (in fractional seconds) in playing back the current item. Only valid when m_IsPlaying is true. */
	double m_CurrentPosition;


signals:

	/** Emitted after the specified item is added to the bottom of the playlist. */
	void itemAdded(IPlaylistItem * a_Item);

	/** Emitted before the specified item is deleted from the playlist. */
	void itemDeleting(IPlaylistItem * a_Item, int a_Index);


public slots:

	/** Advances the current item back by one. */
	void prevItem();

	/** Advances the current item forward by one. */
	void nextItem();
};

using PlaylistPtr = std::shared_ptr<Playlist>;





#endif // PLAYLIST_H

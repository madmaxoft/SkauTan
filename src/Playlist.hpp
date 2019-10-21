#ifndef PLAYLIST_H
#define PLAYLIST_H





#include <memory>
#include <vector>
#include <QObject>
#include "IPlaylistItem.hpp"
#include "Template.hpp"





// fwd:
class Database;
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Container for a single playlist.
Stores the items to be played, the currently played item, timings etc. */
class Playlist:
	public QObject
{
	using Super = QObject;
	Q_OBJECT

public:
	Playlist();

	const std::vector<IPlaylistItemPtr> & items() const { return mItems; }

	/** Adds the specified item to the bottom of the playlist. */
	void addItem(IPlaylistItemPtr aItem);

	/** Moves the item at aFromIdex to aToIdx, shifting the items in between accordingly.
	Doesn't emit any signal. */
	void moveItem(int aFromIdx, int aToIdx);

	/** Deletes the item at the specified index.
	Emits the appropriate signals. */
	void deleteItem(int aIndex);

	/** Reletes the items at the specified indices.
	Emits the appropriate signals. */
	void deleteItems(std::vector<int> && aIndices);

	/** Returns the current item being played, or to be played when the playback is started. */
	IPlaylistItemPtr currentItem() const;

	/** Returns the current item's index in the playlist. */
	int currentItemIndex() const { return mCurrentItemIdx; }

	/** Returns true if the current item is not the last item in the playlist
	(ie. the nextItem() operation will succeed) */
	bool hasNextSong() const { return (mCurrentItemIdx + 1 < static_cast<int>(mItems.size())); }

	/** Moves the current item to the next in the list.
	Returns true if the current item changed, false if not (end of list / no items). */
	bool nextItem();

	/** Moves the current item to the previous in the list.
	Returns true if the current item changed, false if not (start of list / no items). */
	bool prevItem();

	/** Sets the current item to the specified index.
	Ignored if the index is invalid.
	Returns true iff the current item has been set. */
	bool setCurrentItem(int aIndex);

	/** Sets the current item to the specified item.
	Ignored if the specified item is not in the playlist.
	Returns true iff the current item has been set. */
	bool setCurrentItem(const IPlaylistItem * aItem);

	/** Adds new random songs based on the specified template, using songs from the specified DB. */
	void addFromTemplate(const Database & aDB, const Template & aTemplate);

	/** Adds a new random song based on the specified filter, using songs from the specified DB.
	Returns true if successful, false if no song added (no match). */
	bool addFromFilter(const Database & aDB, Filter & aFilter);

	/** Replaces the item at the specified index with the specified item.
	If the index is invalid, does nothing. */
	void replaceItem(int aIndex, IPlaylistItemPtr aItem);

	/** Inserts the specified item so that it is on the specified index.
	If the index is out of bounds, appends the item to the end of the playlist. */
	void insertItem(int aIndex, IPlaylistItemPtr aItem);

	/** Inserts the specified items so that the first of them is at the specified index.
	If the index is out of bounds, appends the items to the end of the playlist. */
	void insertItems(int aIndex, const std::vector<IPlaylistItemPtr> & aItems);

	/** Returns true if the current item is the last in the playlist. */
	bool isAtEnd() const;

	/** Returns true iff the specified index is valid (there are at least that many tracks + 1 in the playlist). */
	bool isValidIndex(int aIndex) const;

	/** Returns the index on which the specified item is in the playlist, or <0 if not present. */
	int indexFromItem(const IPlaylistItem & aItem);


protected:

	/** All the items on the playlist. */
	std::vector<IPlaylistItemPtr> mItems;

	/** The item in mItems currently being played, or to be played when the playback is started.
	Set to -1 if there's no current item. */
	int mCurrentItemIdx;

	/** If true, mCurrentItem is being played back. */
	bool mIsPlaying;


	/** Updates each items' start and end times, starting with the item at the specified index.
	Emits the itemTimeChanged for each item for which the times changed. */
	void updateItemsStartEndTimes(int aStartIdx);


signals:

	/** Emitted after the specified item is added to the bottom of the playlist. */
	void itemAdded(IPlaylistItem * aItem);

	/** Emitted before the specified item is deleted from the playlist. */
	void itemDeleting(IPlaylistItem * aItem, int aIndex);

	/** Emitted after an item at the specified index is replaced with aNewItem. */
	void itemReplaced(int aIndex, IPlaylistItem * aNewItem);

	/** Emitted after the specified item is inserted at the specified index. */
	void itemInserted(int aIndex, IPlaylistItem * aNewItem);

	/** Emitted after the index for the current item is changed. */
	void currentItemChanged(int aCurrentItemIdx);

	/** Emitted after the specified item's start or end time changes. */
	void itemTimesChanged(int aIndex, IPlaylistItem * aItem);


public slots:

	/** Removes any playlist entries relevant to the specified song. */
	void removeSong(SongPtr aSong);

	/** Emitted by the player when it starts playing this playlist's current item,
	the currently-played item changes tempo (and hence playback end),
	the user seeks in the current item,
	or the user changed the item's duration limit.
	Updates the items' start and end times, beginning with the one after the current item.
	NOTE: The current item is expected to have its times updated by the caller before calling this slot. */
	void updateItemTimesFromCurrent();

	/** Emitted by the player when it stops playing at all.
	Erases the items' start and end times for all items after the current one. */
	void eraseItemTimesAfterCurrent();
};

using PlaylistPtr = std::shared_ptr<Playlist>;





#endif // PLAYLIST_H

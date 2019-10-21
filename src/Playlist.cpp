#include "Playlist.hpp"
#include <limits>
#include <cassert>
#include <set>
#include <QDebug>
#include "DB/Database.hpp"
#include "PlaylistItemSong.hpp"





////////////////////////////////////////////////////////////////////////////////
// Playlist:

Playlist::Playlist():
	mCurrentItemIdx(0),
	mIsPlaying(false)
{

}





void Playlist::addItem(IPlaylistItemPtr aItem)
{
	mItems.push_back(aItem);
	emit itemAdded(aItem.get());
	updateItemTimesFromCurrent();
}





void Playlist::moveItem(int aFromIdx, int aToIdx)
{
	assert(aFromIdx < static_cast<int>(mItems.size()));
	assert(
		(aToIdx < static_cast<int>(mItems.size())) ||
		(
			(aToIdx == static_cast<int>(mItems.size())) &&
			(aFromIdx < aToIdx)
		)
	);

	if (aFromIdx == aToIdx)
	{
		// No action needed
		return;
	}
	auto stash = mItems[static_cast<size_t>(aFromIdx)];
	if (aFromIdx > aToIdx)
	{
		// Item moving up the list
		for (int i = aFromIdx; i > aToIdx; --i)
		{
			mItems[static_cast<size_t>(i)] = mItems[static_cast<size_t>(i - 1)];
		}
	}
	else
	{
		// Item moving down the list:
		aToIdx -= 1;
		for (int i = aFromIdx; i < aToIdx; ++i)
		{
			mItems[static_cast<size_t>(i)] = mItems[static_cast<size_t>(i + 1)];
		}
	}
	mItems[static_cast<size_t>(aToIdx)] = stash;

	// If crossing the mCurrentIdx, adjust it for the change:
	if (mCurrentItemIdx >= 0)
	{
		auto oldIdx = mCurrentItemIdx;
		if (mCurrentItemIdx == aFromIdx)
		{
			// Moving the current item
			mCurrentItemIdx = aToIdx;
		}
		else if ((mCurrentItemIdx > aFromIdx) && (mCurrentItemIdx < aToIdx))
		{
			mCurrentItemIdx -= 1;
		}
		else if ((mCurrentItemIdx < aFromIdx) && (mCurrentItemIdx >= aToIdx))
		{
			mCurrentItemIdx += 1;
		}
		if (oldIdx != mCurrentItemIdx)
		{
			emit currentItemChanged(mCurrentItemIdx);
		}
	}

	// If the item was moved above the currently-playing one, remove the item's playback times:
	if (aToIdx < mCurrentItemIdx)
	{
		stash->mPlaybackStarted = QDateTime();
		stash->mPlaybackEnded = QDateTime();
		emit itemTimesChanged(aToIdx, stash.get());
	}

	// Sanity check: no item is in the playlist twice:
	#ifdef _DEBUG
		std::set<IPlaylistItemPtr> items;
		for (const auto & item: mItems)
		{
			assert(items.find(item) == items.cend());
			items.insert(item);
		}
	#endif  // _DEBUG
}





void Playlist::deleteItem(int aIndex)
{
	emit itemDeleting(mItems[static_cast<size_t>(aIndex)].get(), aIndex);
	mItems.erase(mItems.begin() + aIndex);
	if (mCurrentItemIdx > aIndex)
	{
		mCurrentItemIdx -= 1;
		emit currentItemChanged(mCurrentItemIdx);
	}
}





void Playlist::deleteItems(std::vector<int> && aIndices)
{
	std::vector<int> sortedIndices(std::move(aIndices));
	std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
	for (const auto idx: sortedIndices)
	{
		deleteItem(idx);
	}
}





IPlaylistItemPtr Playlist::currentItem() const
{
	if ((mCurrentItemIdx < 0) || mItems.empty())
	{
		return nullptr;
	}
	assert(static_cast<size_t>(mCurrentItemIdx) < mItems.size());
	return mItems[static_cast<size_t>(mCurrentItemIdx)];
}





bool Playlist::nextItem()
{
	if (static_cast<size_t>(mCurrentItemIdx + 1) >= mItems.size())
	{
		return false;
	}
	mCurrentItemIdx += 1;
	emit currentItemChanged(mCurrentItemIdx);
	return true;
}





bool Playlist::prevItem()
{
	if (mCurrentItemIdx <= 0)
	{
		return false;
	}
	mCurrentItemIdx -= 1;
	emit currentItemChanged(mCurrentItemIdx);
	return true;
}





bool Playlist::setCurrentItem(int aIndex)
{
	if (
		(aIndex < 0) ||
		(static_cast<size_t>(aIndex) >= mItems.size())
	)
	{
		return false;
	}
	mCurrentItemIdx = aIndex;
	emit currentItemChanged(mCurrentItemIdx);
	return true;
}





bool Playlist::setCurrentItem(const IPlaylistItem * aItem)
{
	int idx = 0;
	for (const auto & item: mItems)
	{
		if (item.get() == aItem)
		{
			mCurrentItemIdx = idx;
			emit currentItemChanged(mCurrentItemIdx);
			return true;
		}
		idx += 1;
	}
	return false;
}





void Playlist::addFromTemplate(const Database & aDB, const Template & aTemplate)
{
	for (const auto & item: aTemplate.items())
	{
		addFromFilter(aDB, *item);
	}
}





bool Playlist::addFromFilter(const Database & aDB, Filter & aFilter)
{
	auto song = aDB.pickSongForFilter(aFilter);
	if (song == nullptr)
	{
		return false;
	}
	addItem(std::make_shared<PlaylistItemSong>(song, aFilter.shared_from_this()));
	return true;
}





void Playlist::replaceItem(int aIndex, IPlaylistItemPtr aItem)
{
	if (!isValidIndex(aIndex))
	{
		return;
	}
	mItems[static_cast<size_t>(aIndex)] = aItem;
	emit itemReplaced(static_cast<int>(aIndex), aItem.get());
	if (aIndex > mCurrentItemIdx)
	{
		updateItemTimesFromCurrent();
	}
}





void Playlist::insertItem(int aIndex, IPlaylistItemPtr aItem)
{
	if (aIndex > static_cast<int>(mItems.size()))
	{
		aIndex = static_cast<int>(mItems.size());
	}
	mItems.insert(mItems.begin() + aIndex, aItem);
	if (aIndex <= mCurrentItemIdx)
	{
		mCurrentItemIdx += 1;
	}
	emit itemInserted(aIndex, aItem.get());
	if (aIndex > mCurrentItemIdx)
	{
		updateItemTimesFromCurrent();
	}
	else
	{
		emit currentItemChanged(mCurrentItemIdx);
	}
}





void Playlist::insertItems(int aIndex, const std::vector<IPlaylistItemPtr> & aItems)
{
	for (const auto & item: aItems)
	{
		insertItem(aIndex, item);
		aIndex += 1;
	}
}





bool Playlist::isAtEnd() const
{
	return (mCurrentItemIdx == static_cast<int>(mItems.size()) - 1);
}





bool Playlist::isValidIndex(int aIndex) const
{
	return (aIndex >= 0) && (aIndex < static_cast<int>(mItems.size()));
}





int Playlist::indexFromItem(const IPlaylistItem & aItem)
{
	auto s = mItems.size();
	for (size_t i = 0; i < s; ++i)
	{
		if (mItems[i].get() == &aItem)
		{
			return static_cast<int>(i);
		}
	}
	// Not found
	return -1;
}





void Playlist::updateItemsStartEndTimes(int aStartIdx)
{
	assert(isValidIndex(aStartIdx));
	assert(aStartIdx > mCurrentItemIdx);  // Can only update track start times after the currently-playing track

	auto started = (aStartIdx > 0) ?
		mItems[static_cast<size_t>(aStartIdx) - 1]->mPlaybackEnded :
		QDateTime::currentDateTimeUtc();
	auto idx = aStartIdx;
	for (auto itr = mItems.begin() + aStartIdx, end = mItems.end(); itr != end; ++itr, ++idx)
	{
		auto item = *itr;
		auto dur = static_cast<int>(std::round(item->totalPlaybackDuration() * 1000));
		auto ended = started.addMSecs(dur);
		if ((started != item->mPlaybackStarted) || (ended != item->mPlaybackEnded))
		{
			// There's a change in this item, save it and emit it:
			item->mPlaybackStarted = started;
			item->mPlaybackEnded = ended;
			emit itemTimesChanged(idx, item.get());
		}
		started = ended;
	}
}





void Playlist::removeSong(SongPtr aSong)
{
	int idx = 0;
	for (auto itr = mItems.begin(); itr != mItems.end();)
	{
		auto plis = std::dynamic_pointer_cast<PlaylistItemSong>(*itr);
		if (plis == nullptr)
		{
			++itr;
			++idx;
			continue;
		}
		if (plis->song() != aSong)
		{
			++itr;
			++idx;
			continue;
		}
		emit itemDeleting(itr->get(), idx);
		itr = mItems.erase(itr);
	}
}





void Playlist::updateItemTimesFromCurrent()
{
	auto idx = mCurrentItemIdx;
	if (!isValidIndex(idx))
	{
		// Happens when deleting all items form the playlist
		return;
	}
	auto item = mItems[static_cast<size_t>(idx)];
	assert(item != nullptr);
	// The current item's times have already been set in the Player, just emit the update signal now:
	emit itemTimesChanged(idx, item.get());
	if (isValidIndex(idx + 1))
	{
		updateItemsStartEndTimes(idx + 1);
	}
}





void Playlist::eraseItemTimesAfterCurrent()
{
	if (!isValidIndex(mCurrentItemIdx))
	{
		return;
	}
	auto idx = mCurrentItemIdx + 1;
	for (auto itr = mItems.begin() + idx, end = mItems.end(); itr != end; ++itr, ++idx)
	{
		auto item = *itr;
		item->mPlaybackStarted = QDateTime();
		item->mPlaybackEnded = QDateTime();
		emit itemTimesChanged(idx, item.get());
	}
}





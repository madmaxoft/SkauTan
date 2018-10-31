#include "Playlist.hpp"
#include <limits>
#include <cassert>
#include <QDebug>
#include "DB/Database.hpp"
#include "PlaylistItemSong.hpp"





////////////////////////////////////////////////////////////////////////////////
// Playlist:

Playlist::Playlist():
	m_CurrentItemIdx(0),
	m_IsPlaying(false)
{

}





void Playlist::addItem(IPlaylistItemPtr a_Item)
{
	m_Items.push_back(a_Item);
	emit itemAdded(a_Item.get());
	updateItemTimesFromCurrent();
}





void Playlist::moveItem(int a_FromIdx, int a_ToIdx)
{
	assert(a_FromIdx < static_cast<int>(m_Items.size()));
	assert(
		(a_ToIdx < static_cast<int>(m_Items.size())) ||
		(
			(a_ToIdx == static_cast<int>(m_Items.size())) &&
			(a_FromIdx < a_ToIdx)
		)
	);

	if (a_FromIdx == a_ToIdx)
	{
		// No action needed
	}
	auto stash = m_Items[static_cast<size_t>(a_FromIdx)];
	if (a_FromIdx > a_ToIdx)
	{
		// Item moving up the list
		for (int i = a_FromIdx; i > a_ToIdx; --i)
		{
			m_Items[static_cast<size_t>(i)] = m_Items[static_cast<size_t>(i - 1)];
		}
	}
	else
	{
		// Item moving down the list:
		a_ToIdx -= 1;
		for (int i = a_FromIdx; i < a_ToIdx; ++i)
		{
			m_Items[static_cast<size_t>(i)] = m_Items[static_cast<size_t>(i + 1)];
		}
	}
	m_Items[static_cast<size_t>(a_ToIdx)] = stash;

	// If crossing the m_CurrentIdx, adjust it for the change:
	if (m_CurrentItemIdx >= 0)
	{
		auto oldIdx = m_CurrentItemIdx;
		if (m_CurrentItemIdx == a_FromIdx)
		{
			// Moving the current item
			m_CurrentItemIdx = a_ToIdx;
		}
		else if ((m_CurrentItemIdx > a_FromIdx) && (m_CurrentItemIdx < a_ToIdx))
		{
			m_CurrentItemIdx -= 1;
		}
		else if ((m_CurrentItemIdx < a_FromIdx) && (m_CurrentItemIdx >= a_ToIdx))
		{
			m_CurrentItemIdx += 1;
		}
		if (oldIdx != m_CurrentItemIdx)
		{
			emit currentItemChanged(m_CurrentItemIdx);
		}
	}

	// If the item was moved above the currently-playing one, remove the item's playback times:
	if (a_ToIdx < m_CurrentItemIdx)
	{
		stash->m_PlaybackStarted = QDateTime();
		stash->m_PlaybackEnded = QDateTime();
		emit itemTimesChanged(a_ToIdx, stash.get());
	}
}





void Playlist::deleteItem(int a_Index)
{
	emit itemDeleting(m_Items[static_cast<size_t>(a_Index)].get(), a_Index);
	m_Items.erase(m_Items.begin() + a_Index);
	if (m_CurrentItemIdx > a_Index)
	{
		m_CurrentItemIdx -= 1;
		emit currentItemChanged(m_CurrentItemIdx);
	}
}





void Playlist::deleteItems(std::vector<int> && a_Indices)
{
	std::vector<int> sortedIndices(std::move(a_Indices));
	std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
	for (const auto idx: sortedIndices)
	{
		deleteItem(idx);
	}
}





IPlaylistItemPtr Playlist::currentItem() const
{
	if ((m_CurrentItemIdx < 0) || m_Items.empty())
	{
		return nullptr;
	}
	assert(static_cast<size_t>(m_CurrentItemIdx) < m_Items.size());
	return m_Items[static_cast<size_t>(m_CurrentItemIdx)];
}





bool Playlist::nextItem()
{
	if (static_cast<size_t>(m_CurrentItemIdx + 1) >= m_Items.size())
	{
		return false;
	}
	m_CurrentItemIdx += 1;
	emit currentItemChanged(m_CurrentItemIdx);
	return true;
}





bool Playlist::prevItem()
{
	if (m_CurrentItemIdx <= 0)
	{
		return false;
	}
	m_CurrentItemIdx -= 1;
	emit currentItemChanged(m_CurrentItemIdx);
	return true;
}





bool Playlist::setCurrentItem(int a_Index)
{
	if (
		(a_Index < 0) ||
		(static_cast<size_t>(a_Index) >= m_Items.size())
	)
	{
		return false;
	}
	m_CurrentItemIdx = a_Index;
	emit currentItemChanged(m_CurrentItemIdx);
	return true;
}





bool Playlist::setCurrentItem(const IPlaylistItem * a_Item)
{
	int idx = 0;
	for (const auto & item: m_Items)
	{
		if (item.get() == a_Item)
		{
			m_CurrentItemIdx = idx;
			emit currentItemChanged(m_CurrentItemIdx);
			return true;
		}
		idx += 1;
	}
	return false;
}





void Playlist::addFromTemplate(const Database & a_DB, const Template & a_Template)
{
	for (const auto & item: a_Template.items())
	{
		addFromFilter(a_DB, *item);
	}
}





bool Playlist::addFromFilter(const Database & a_DB, Filter & a_Filter)
{
	auto song = a_DB.pickSongForFilter(a_Filter);
	if (song == nullptr)
	{
		return false;
	}
	addItem(std::make_shared<PlaylistItemSong>(song, a_Filter.shared_from_this()));
	return true;
}





void Playlist::replaceItem(int a_Index, IPlaylistItemPtr a_Item)
{
	if (!isValidIndex(a_Index))
	{
		return;
	}
	m_Items[static_cast<size_t>(a_Index)] = a_Item;
	emit itemReplaced(static_cast<int>(a_Index), a_Item.get());
	if (a_Index > m_CurrentItemIdx)
	{
		updateItemTimesFromCurrent();
	}
}





void Playlist::insertItem(int a_Index, IPlaylistItemPtr a_Item)
{
	if (a_Index > static_cast<int>(m_Items.size()))
	{
		a_Index = static_cast<int>(m_Items.size());
	}
	m_Items.insert(m_Items.begin() + a_Index, a_Item);
	emit itemInserted(a_Index, a_Item.get());
	if (a_Index > m_CurrentItemIdx)
	{
		updateItemTimesFromCurrent();
	}
}





bool Playlist::isAtEnd() const
{
	return (m_CurrentItemIdx == static_cast<int>(m_Items.size()) - 1);
}





bool Playlist::isValidIndex(int a_Index) const
{
	return (a_Index >= 0) && (a_Index < static_cast<int>(m_Items.size()));
}





int Playlist::indexFromItem(const IPlaylistItem & a_Item)
{
	auto s = m_Items.size();
	for (size_t i = 0; i < s; ++i)
	{
		if (m_Items[i].get() == &a_Item)
		{
			return static_cast<int>(i);
		}
	}
	// Not found
	return -1;
}





void Playlist::updateItemsStartEndTimes(int a_StartIdx)
{
	assert(isValidIndex(a_StartIdx));
	assert(a_StartIdx > m_CurrentItemIdx);  // Can only update track start times after the currently-playing track

	auto started = (a_StartIdx > 0) ?
		m_Items[static_cast<size_t>(a_StartIdx) - 1]->m_PlaybackEnded :
		QDateTime::currentDateTimeUtc();
	auto idx = a_StartIdx;
	for (auto itr = m_Items.begin() + a_StartIdx, end = m_Items.end(); itr != end; ++itr, ++idx)
	{
		auto item = *itr;
		auto dur = static_cast<int>(std::round(item->totalPlaybackDuration() * 1000));
		auto ended = started.addMSecs(dur);
		if ((started != item->m_PlaybackStarted) || (ended != item->m_PlaybackEnded))
		{
			// There's a change in this item, save it and emit it:
			item->m_PlaybackStarted = started;
			item->m_PlaybackEnded = ended;
			emit itemTimesChanged(idx, item.get());
		}
		started = ended;
	}
}





void Playlist::removeSong(SongPtr a_Song)
{
	int idx = 0;
	for (auto itr = m_Items.begin(); itr != m_Items.end();)
	{
		auto plis = std::dynamic_pointer_cast<PlaylistItemSong>(*itr);
		if (plis == nullptr)
		{
			++itr;
			++idx;
			continue;
		}
		if (plis->song() != a_Song)
		{
			++itr;
			++idx;
			continue;
		}
		emit itemDeleting(itr->get(), idx);
		itr = m_Items.erase(itr);
	}
}





void Playlist::updateItemTimesFromCurrent()
{
	auto idx = m_CurrentItemIdx;
	if (!isValidIndex(idx))
	{
		// Happens when deleting all items form the playlist
		return;
	}
	auto item = m_Items[static_cast<size_t>(idx)];
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
	if (!isValidIndex(m_CurrentItemIdx))
	{
		return;
	}
	auto idx = m_CurrentItemIdx + 1;
	for (auto itr = m_Items.begin() + idx, end = m_Items.end(); itr != end; ++itr, ++idx)
	{
		auto item = *itr;
		item->m_PlaybackStarted = QDateTime();
		item->m_PlaybackEnded = QDateTime();
		emit itemTimesChanged(idx, item.get());
	}
}





#include "Playlist.h"
#include <limits>
#include <assert.h>
#include <QDebug>
#include "Database.h"
#include "PlaylistItemSong.h"





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
}





void Playlist::moveItem(size_t a_FromIdx, size_t a_ToIdx)
{
	assert(a_FromIdx < m_Items.size());
	assert(
		(a_ToIdx < m_Items.size()) ||
		(
			(a_ToIdx == m_Items.size()) &&
			(a_FromIdx < a_ToIdx)
		)
	);

	if (a_FromIdx == a_ToIdx)
	{
		// No action needed
	}
	auto stash = m_Items[a_FromIdx];
	if (a_FromIdx > a_ToIdx)
	{
		// Item moving up the list
		for (size_t i = a_FromIdx; i > a_ToIdx; --i)
		{
			m_Items[i] = m_Items[i - 1];
		}
	}
	else
	{
		// Item moving down the list:
		a_ToIdx -= 1;
		for (size_t i = a_FromIdx; i < a_ToIdx; ++i)
		{
			m_Items[i] = m_Items[i + 1];
		}
	}
	m_Items[a_ToIdx] = stash;
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
		addFromTemplateItem(a_DB, item);
	}
}





bool Playlist::addFromTemplateItem(const Database & a_DB, Template::ItemPtr a_Item)
{
	auto song = a_DB.pickSongForTemplateItem(a_Item);
	if (song == nullptr)
	{
		return false;
	}
	addItem(std::make_shared<PlaylistItemSong>(song, a_Item));
	return true;
}





void Playlist::replaceItem(size_t a_Index, IPlaylistItemPtr a_Item)
{
	if (a_Index >= m_Items.size())
	{
		return;
	}
	m_Items[a_Index] = a_Item;
	emit itemReplaced(static_cast<int>(a_Index), a_Item.get());
}





void Playlist::insertItem(int a_Index, IPlaylistItemPtr a_Item)
{
	if (a_Index > static_cast<int>(m_Items.size()))
	{
		a_Index = static_cast<int>(m_Items.size());
	}
	m_Items.insert(m_Items.begin() + a_Index, a_Item);
	emit itemInserted(a_Index, a_Item.get());
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

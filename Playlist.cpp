#include "Playlist.h"
#include <assert.h>
#include <QDebug>





Playlist::Playlist():
	m_CurrentItemIdx(0),
	m_IsPlaying(false),
	m_CurrentPosition(0)
{

}





void Playlist::addItem(IPlaylistItemPtr a_Item)
{
	m_Items.push_back(a_Item);
	emit itemAdded(a_Item.get());
}





void Playlist::moveItem(int a_FromIdx, int a_ToIdx)
{
	assert(a_FromIdx >= 0);
	assert(a_ToIdx >= 0);
	assert(static_cast<size_t>(a_FromIdx) < m_Items.size());
	assert(
		(static_cast<size_t>(a_ToIdx) < m_Items.size()) ||
		(
			(static_cast<size_t>(a_ToIdx) == m_Items.size()) &&
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
		for (int i = a_FromIdx; i > a_ToIdx; --i)
		{
			m_Items[i] = m_Items[i - 1];
		}
	}
	else
	{
		// Item moving down the list:
		a_ToIdx -= 1;
		for (int i = a_FromIdx; i < a_ToIdx; ++i)
		{
			m_Items[i] = m_Items[i + 1];
		}
	}
	m_Items[a_ToIdx] = stash;
}





void Playlist::deleteItem(int a_Index)
{
	emit itemDeleting(m_Items[a_Index].get(), a_Index);
	m_Items.erase(m_Items.begin() + a_Index);
}





IPlaylistItemPtr Playlist::current() const
{
	if (m_Items.empty())
	{
		return nullptr;
	}
	assert(m_CurrentItemIdx < m_Items.size());
	return m_Items[m_CurrentItemIdx];
}





void Playlist::prevItem()
{
	if (m_CurrentItemIdx > 0)
	{
		m_CurrentItemIdx -= 1;
	}
}





void Playlist::nextItem()
{
	if (m_CurrentItemIdx + 1 < m_Items.size())
	{
		m_CurrentItemIdx += 1;
	}
}

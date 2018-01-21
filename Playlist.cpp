#include "Playlist.h"
#include <assert.h>
#include <QDebug>





Playlist::Playlist():
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

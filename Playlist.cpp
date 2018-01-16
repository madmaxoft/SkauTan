#include "Playlist.h"





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

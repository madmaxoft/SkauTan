#include "Playlist.h"
#include <limits>
#include <assert.h>
#include <fstream>
#include <random>
#include <QDebug>
#include "Database.h"
#include "PlaylistItemSong.h"





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
		addFromTemplateItem(a_DB, *item);
	}
}





bool Playlist::addFromTemplateItem(const Database & a_DB, const Template::Item & a_Item)
{
	std::vector<std::pair<SongPtr, int>> songs;  // Pairs of SongPtr and their weight
	int totalWeight = 0;
	for (const auto & song: a_DB.songs())
	{
		if (a_Item.filter()->isSatisfiedBy(*song))
		{
			auto weight = getSongWeight(a_DB, *song);
			songs.push_back(std::make_pair(song, weight));
			totalWeight += weight;
		}
	}

	if (songs.empty())
	{
		qDebug() << ": No song matches item " << a_Item.displayName();
		return false;
	}

	auto chosen = songs[0].first;
	totalWeight = std::max(totalWeight, 1);
	static std::mt19937_64 mt(0);
	auto rnd = std::uniform_int_distribution<>(0, totalWeight)(mt);
	auto threshold = rnd;
	for (const auto & song: songs)
	{
		threshold -= song.second;
		if (threshold <= 0)
		{
			chosen = song.first;
			break;
		}
	}

	#if 0
	// DEBUG: Output the choices made into a file:
	static int counter = 0;
	auto fnam = QString("debug_template_%1.log").arg(QString::number(counter++), 3, '0');
	std::ofstream f(fnam.toStdString().c_str());
	f << "Choices for template item " << a_Item.displayName().toStdString() << std::endl;
	f << "------" << std::endl << std::endl;
	f << "Candidates:" << std::endl;
	for (const auto & song: songs)
	{
		f << song.second << '\t';
		f << song.first->lastPlayed().toString().toStdString() << '\t';
		f << song.first->fileName().toStdString() << std::endl;
	}
	f << std::endl;
	f << "totalWeight: " << totalWeight << std::endl;
	f << "threshold: " << rnd << std::endl;
	f << "chosen: " << chosen->fileName().toStdString() << std::endl;
	#endif

	assert(chosen != nullptr);
	addItem(std::make_shared<PlaylistItemSong>(chosen));
	return true;
}





int Playlist::getSongWeight(const Database & a_DB, const Song & a_Song)
{
	Q_UNUSED(a_DB);
	qint64 res = 10000;  // Base weight

	// Penalize the last played date:
	auto lastPlayed = a_Song.lastPlayed();
	if (lastPlayed.isValid())
	{
		auto numDays = lastPlayed.toDateTime().daysTo(QDateTime::currentDateTime());
		res = res * (numDays + 1) / (numDays + 2);  // The more days have passed, the less penalty
	}

	// Penalize presence in the list:
	int idx = 0;
	for (const auto & itm: items())
	{
		auto spi = dynamic_cast<PlaylistItemSong *>(itm.get());
		if (spi != nullptr)
		{
			if (spi->song().get() == &a_Song)
			{
				// This song is already present, penalize depending on distance from the end (where presumably it is to be added):
				auto numInBetween = static_cast<int>(m_Items.size()) - idx;
				res = res * (numInBetween + 100) / (numInBetween + 200);
				// Do not stop processing - if present multiple times, penalize multiple times
			}
		}
		idx += 1;
	}

	// Penalize by rating:
	auto rating = a_Song.rating();
	if (rating.isValid())
	{
		res = res * (rating.toInt() + 1) / 5;  // Even zero-rating songs need *some* chance
	}
	else
	{
		// Default to 2.5-star rating:
		res = static_cast<qint64>(res * 3.5 / 5);
	}

	if (res > std::numeric_limits<int>::max())
	{
		return std::numeric_limits<int>::max();
	}
	return static_cast<int>(res);
}

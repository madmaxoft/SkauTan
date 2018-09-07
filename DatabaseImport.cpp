#include "DatabaseImport.h"
#include "Database.h"





DatabaseImport::DatabaseImport(const Database & a_From, Database & a_To, const Options & a_Options):
	m_From(a_From),
	m_To(a_To),
	m_Options(a_Options)
{
	if (m_Options.m_ShouldImportManualTag)
	{
		importManualTag();
	}
	if (m_Options.m_ShouldImportLastPlayedDate)
	{
		importLastPlayedDate();
	}
	if (m_Options.m_ShouldImportLocalRating)
	{
		importLocalRating();
	}
	if (m_Options.m_ShouldImportCommunityRating)
	{
		importCommunityRating();
	}
	if (m_Options.m_ShouldImportPlaybackHistory)
	{
		importPlaybackHistory();
	}
	if (m_Options.m_ShouldImportSkipStart)
	{
		importSkipStart();
	}
	if (m_Options.m_ShouldImportDeletionHistory)
	{
		importDeletionHistory();
	}
	m_To.saveAllSongSharedData();
}





void DatabaseImport::importManualTag()
{
	for (auto & sd: m_From.songSharedDataMap())
	{
		auto dest  = m_To.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->m_TagManual.m_Author.updateIfNewer(sd.second->m_TagManual.m_Author);
		dest->m_TagManual.m_Title.updateIfNewer(sd.second->m_TagManual.m_Title);
		dest->m_TagManual.m_Genre.updateIfNewer(sd.second->m_TagManual.m_Genre);
		dest->m_TagManual.m_MeasuresPerMinute.updateIfNewer(sd.second->m_TagManual.m_MeasuresPerMinute);
	}
}





void DatabaseImport::importLastPlayedDate()
{
	for (auto & sd: m_From.songSharedDataMap())
	{
		auto dest = m_To.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->m_LastPlayed.updateIfNewer(sd.second->m_LastPlayed);
	}
}





void DatabaseImport::importLocalRating()
{
	for (auto & sd: m_From.songSharedDataMap())
	{
		auto dest = m_To.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->m_Rating.m_Local.updateIfNewer(sd.second->m_Rating.m_Local);
	}
}





void DatabaseImport::importCommunityRating()
{
	for (auto & sd: m_From.songSharedDataMap())
	{
		auto dest = m_To.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->m_Rating.m_RhythmClarity.updateIfNewer(sd.second->m_Rating.m_GenreTypicality);
		dest->m_Rating.m_GenreTypicality.updateIfNewer(sd.second->m_Rating.m_GenreTypicality);
		dest->m_Rating.m_Popularity.updateIfNewer(sd.second->m_Rating.m_GenreTypicality);
	}
}





void DatabaseImport::importPlaybackHistory()
{
	auto fromHistory = m_From.playbackHistory();
	auto toHistory   = m_To.playbackHistory();

	// Special cases: empty histories:
	if (fromHistory.empty())
	{
		return;
	}
	if (toHistory.empty())
	{
		m_To.addPlaybackHistory(fromHistory);
		return;
	}

	// Compare each item by date:
	auto comparison = [](const Database::HistoryItem & a_Item1, const Database::HistoryItem a_Item2)
	{
		return (a_Item1.m_Timestamp < a_Item2.m_Timestamp);
	};
	std::sort(fromHistory.begin(), fromHistory.end(), comparison);
	std::sort(toHistory.begin(), toHistory.end(), comparison);
	std::vector<Database::HistoryItem> toAdd;
	auto itrF = fromHistory.cbegin(), endF = fromHistory.cend();
	auto itrT = toHistory.cbegin(),   endT = toHistory.cend();
	for (; itrF != endF;)
	{
		if (itrF->m_Timestamp == itrT->m_Timestamp)
		{
			itrF++;
			itrT++;
			if (itrT == endT)
			{
				break;
			}
		}
		else if (itrF->m_Timestamp > itrT->m_Timestamp)
		{
			++itrT;
			if (itrT == endT)
			{
				break;
			}
			continue;
		}
		else
		{
			// itrF->m_Timestamp < itrT->m_Timestamp
			toAdd.push_back(*itrF);
			++itrF;
		}
	}
	if (itrF != endF)
	{
		// Leftover items in From, add them all:
		for (; itrF != endF; ++itrF)
		{
			toAdd.push_back(*itrF);
		}
	}
	m_To.addPlaybackHistory(toAdd);
}





void DatabaseImport::importSkipStart()
{
	for (auto & sd: m_From.songSharedDataMap())
	{
		auto dest = m_To.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->m_SkipStart.updateIfNewer(sd.second->m_SkipStart);
	}
}





void DatabaseImport::importDeletionHistory()
{
	// Special cases: empty histories:
	const auto & fromHistory = m_From.removedSongs();
	const auto & toHistory = m_To.removedSongs();
	if (fromHistory.empty())
	{
		return;
	}
	if (toHistory.empty())
	{
		m_To.addSongRemovalHistory(fromHistory);
		return;
	}

	// Compare each item by date:
	std::vector<Database::RemovedSongPtr> toAdd;
	auto itrF = fromHistory.cbegin(), endF = fromHistory.cend();
	auto itrT = toHistory.cbegin(),   endT = toHistory.cend();
	for (; itrF != endF;)
	{
		if ((*itrF)->m_DateRemoved == (*itrT)->m_DateRemoved)
		{
			itrF++;
			itrT++;
			if (itrT == endT)
			{
				break;
			}
		}
		else if ((*itrF)->m_DateRemoved > (*itrT)->m_DateRemoved)
		{
			++itrT;
			if (itrT == endT)
			{
				break;
			}
			continue;
		}
		else
		{
			// itrF->m_DateRemoved < itrT->m_DateRemoved
			toAdd.push_back(*itrF);
			++itrF;
		}
	}
	if (itrF != endF)
	{
		// Leftover items in From, add them all:
		for (; itrF != endF; ++itrF)
		{
			toAdd.push_back(*itrF);
		}
	}
	m_To.addSongRemovalHistory(toAdd);
}

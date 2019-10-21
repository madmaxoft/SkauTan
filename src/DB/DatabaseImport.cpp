#include "DatabaseImport.hpp"
#include "Database.hpp"





DatabaseImport::DatabaseImport(const Database & aFrom, Database & aTo, const Options & aOptions):
	mFrom(aFrom),
	mTo(aTo),
	mOptions(aOptions)
{
	if (mOptions.mShouldImportManualTag)
	{
		importManualTag();
	}
	if (mOptions.mShouldImportLastPlayedDate)
	{
		importLastPlayedDate();
	}
	if (mOptions.mShouldImportLocalRating)
	{
		importLocalRating();
	}
	if (mOptions.mShouldImportCommunityRating)
	{
		importCommunityRating();
	}
	if (mOptions.mShouldImportPlaybackHistory)
	{
		importPlaybackHistory();
	}
	if (mOptions.mShouldImportSkipStart)
	{
		importSkipStart();
	}
	if (mOptions.mShouldImportDeletionHistory)
	{
		importDeletionHistory();
	}
	if (mOptions.mShouldImportSongColors)
	{
		importSongColors();
	}
	mTo.saveAllSongSharedData();
}





void DatabaseImport::importManualTag()
{
	for (auto & sd: mFrom.songSharedDataMap())
	{
		auto dest  = mTo.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->mTagManual.mAuthor.updateIfNewer(sd.second->mTagManual.mAuthor);
		dest->mTagManual.mTitle.updateIfNewer(sd.second->mTagManual.mTitle);
		dest->mTagManual.mGenre.updateIfNewer(sd.second->mTagManual.mGenre);
		dest->mTagManual.mMeasuresPerMinute.updateIfNewer(sd.second->mTagManual.mMeasuresPerMinute);
	}
}





void DatabaseImport::importDetectedTempo()
{
	for (auto & sd: mFrom.songSharedDataMap())
	{
		auto dest  = mTo.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->mDetectedTempo.updateIfNewer(sd.second->mDetectedTempo);
	}
}





void DatabaseImport::importLastPlayedDate()
{
	for (auto & sd: mFrom.songSharedDataMap())
	{
		auto dest = mTo.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->mLastPlayed.updateIfNewer(sd.second->mLastPlayed);
	}
}





void DatabaseImport::importLocalRating()
{
	for (auto & sd: mFrom.songSharedDataMap())
	{
		auto dest = mTo.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->mRating.mLocal.updateIfNewer(sd.second->mRating.mLocal);
	}
}





void DatabaseImport::importCommunityRating()
{
	// Copy the individual votes:
	importVotes("VotesRhythmClarity");
	importVotes("VotesGenreTypicality");
	importVotes("VotesPopularity");

	// Update the aggregated ratings:
	auto votesRC  = mTo.loadVotes("VotesRhythmClarity");
	auto votesGT  = mTo.loadVotes("VotesGenreTypicality");
	auto votesPop = mTo.loadVotes("VotesPopularity");
	for (auto & sd: mTo.songSharedDataMap())
	{
		sd.second->mRating.mRhythmClarity.updateIfNewer  (averageVotes(votesRC, sd.first));
		sd.second->mRating.mGenreTypicality.updateIfNewer(averageVotes(votesRC, sd.first));
		sd.second->mRating.mPopularity.updateIfNewer     (averageVotes(votesRC, sd.first));
	}
	mTo.saveAllSongSharedData();
}





void DatabaseImport::importPlaybackHistory()
{
	auto fromHistory = mFrom.playbackHistory();
	auto toHistory   = mTo.playbackHistory();

	// Special cases: empty histories:
	if (fromHistory.empty())
	{
		return;
	}
	if (toHistory.empty())
	{
		mTo.addPlaybackHistory(fromHistory);
		return;
	}

	// Compare each item by date:
	auto comparison = [](const Database::HistoryItem & aItem1, const Database::HistoryItem aItem2)
	{
		return (aItem1.mTimestamp < aItem2.mTimestamp);
	};
	std::sort(fromHistory.begin(), fromHistory.end(), comparison);
	std::sort(toHistory.begin(), toHistory.end(), comparison);
	std::vector<Database::HistoryItem> toAdd;
	auto itrF = fromHistory.cbegin(), endF = fromHistory.cend();
	auto itrT = toHistory.cbegin(),   endT = toHistory.cend();
	for (; itrF != endF;)
	{
		if (itrF->mTimestamp == itrT->mTimestamp)
		{
			itrF++;
			itrT++;
			if (itrT == endT)
			{
				break;
			}
		}
		else if (itrF->mTimestamp > itrT->mTimestamp)
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
			// itrF->mTimestamp < itrT->mTimestamp
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
	mTo.addPlaybackHistory(toAdd);
}





void DatabaseImport::importSkipStart()
{
	for (auto & sd: mFrom.songSharedDataMap())
	{
		auto dest = mTo.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->mSkipStart.updateIfNewer(sd.second->mSkipStart);
	}
}





void DatabaseImport::importDeletionHistory()
{
	// Special cases: empty histories:
	const auto & fromHistory = mFrom.removedSongs();
	const auto & toHistory = mTo.removedSongs();
	if (fromHistory.empty())
	{
		return;
	}
	if (toHistory.empty())
	{
		mTo.addSongRemovalHistory(fromHistory);
		return;
	}

	// Compare each item by date:
	std::vector<Database::RemovedSongPtr> toAdd;
	auto itrF = fromHistory.cbegin(), endF = fromHistory.cend();
	auto itrT = toHistory.cbegin(),   endT = toHistory.cend();
	for (; itrF != endF;)
	{
		if ((*itrF)->mDateRemoved == (*itrT)->mDateRemoved)
		{
			itrF++;
			itrT++;
			if (itrT == endT)
			{
				break;
			}
		}
		else if ((*itrF)->mDateRemoved > (*itrT)->mDateRemoved)
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
			// itrF->mDateRemoved < itrT->mDateRemoved
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
	mTo.addSongRemovalHistory(toAdd);
}





void DatabaseImport::importSongColors()
{
	for (auto & sd: mFrom.songSharedDataMap())
	{
		auto dest = mTo.sharedDataFromHash(sd.first);
		if (dest == nullptr)
		{
			continue;
		}
		dest->mBgColor.updateIfNewer(sd.second->mBgColor);
	}
}





void DatabaseImport::importVotes(const QString & aTableName)
{
	auto fromVotes = mFrom.loadVotes(aTableName);
	auto toVotes   = mTo.loadVotes(aTableName);
	if (fromVotes.empty())
	{
		return;
	}
	if (toVotes.empty())
	{
		mTo.addVotes(aTableName, fromVotes);
		return;
	}

	// Compare each item by date (we assume there aren't votes with the same timestamp):
	std::vector<Database::Vote> toAdd;
	auto itrF = fromVotes.cbegin(), endF = fromVotes.cend();
	auto itrT = toVotes.cbegin(),   endT = toVotes.cend();
	for (; itrF != endF;)
	{
		if (itrF->mDateAdded == itrT->mDateAdded)
		{
			itrF++;
			itrT++;
			if (itrT == endT)
			{
				break;
			}
		}
		else if (itrF->mDateAdded > itrT->mDateAdded)
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
			// itrF->mDateAdded < itrT->mDateAdded
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
	mTo.addVotes(aTableName, toAdd);
}





DatedOptional<double> DatabaseImport::averageVotes(
	const std::vector<Database::Vote> & aVotes,
	const QByteArray & aSongHash
)
{
	int sum = 0;
	int count = 0;
	for (const auto & v: aVotes)
	{
		if (v.mSongHash == aSongHash)
		{
			sum += v.mVoteValue;
			count += 1;
		}
	}
	if (count > 0)
	{
		return static_cast<double>(sum) / count;
	}
	return DatedOptional<double>();
}

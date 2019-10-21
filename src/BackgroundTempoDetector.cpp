#include "BackgroundTempoDetector.hpp"
#include <cassert>
#include "ComponentCollection.hpp"
#include "DB/Database.hpp"
#include "SongTempoDetector.hpp"
#include "Stopwatch.hpp"





BackgroundTempoDetector::BackgroundTempoDetector(ComponentCollection & aComponents):
	mComponents(aComponents),
	mIsRunning(false),
	mShouldAbort(true)
{
}





void BackgroundTempoDetector::start()
{
	if (mIsRunning.load())
	{
		qDebug() << "Already running";
		return;
	}
	mShouldAbort = false;
	enqueueAnother();
}





void BackgroundTempoDetector::stop()
{
	mShouldAbort = true;
}





Song::SharedDataPtr BackgroundTempoDetector::pickNextSong()
{
	// Return the first song that has no MPM set:
	auto db = mComponents.get<Database>();
	auto sdm = db->songSharedDataMap();
	for (auto itr = sdm.cbegin(), end = sdm.cend(); itr != end; ++itr)
	{
		auto sd = itr->second;
		{
			std::lock_guard<std::mutex> lock(mMtxFailedSongs);
			if (mFailedSongs.find(sd) != mFailedSongs.end())
			{
				// Has failed recently, skip it
				continue;
			}
		}
		if (
			sd->mTagManual.mMeasuresPerMinute.isPresent() ||
			sd->mDetectedTempo.isPresent()
		)
		{
			continue;
		}
		auto duplicates = sd->duplicates();
		if (duplicates.empty())
		{
			continue;
		}
		bool hasMPM = false;
		for (const auto s: duplicates)
		{
			if (
				s->tagFileName().mMeasuresPerMinute.isPresent() ||
				s->tagId3().mMeasuresPerMinute.isPresent()
			)
			{
				hasMPM = true;
				break;
			}
		}  // for s - duplicates[]
		if (!hasMPM)
		{
			return sd;
		}
	}  // for itr - sdm[]

	// No eligible song found
	return nullptr;
}





void BackgroundTempoDetector::enqueueAnother()
{
	auto sd = pickNextSong();
	if (sd == nullptr)
	{
		return;
	}
	mIsRunning = true;
	auto taskName = SongTempoDetector::createTaskName(sd);
	auto td = mComponents.get<SongTempoDetector>();
	BackgroundTasks::get().enqueue(taskName, [this, sd, td]()
		{
			if (!td->detect(sd))
			{
				std::lock_guard<std::mutex> lock(mMtxFailedSongs);
				mFailedSongs.insert(sd);
			}
			if (!mShouldAbort.load())
			{
				enqueueAnother();
			}
			else
			{
				mIsRunning = false;
			}
		}
	);
}

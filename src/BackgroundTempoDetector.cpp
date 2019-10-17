#include "BackgroundTempoDetector.hpp"
#include <cassert>
#include "ComponentCollection.hpp"
#include "DB/Database.hpp"
#include "SongTempoDetector.hpp"
#include "Stopwatch.hpp"





BackgroundTempoDetector::BackgroundTempoDetector(ComponentCollection & a_Components):
	m_Components(a_Components),
	m_IsRunning(false),
	m_ShouldAbort(true)
{
}





void BackgroundTempoDetector::start()
{
	if (m_IsRunning.load())
	{
		qDebug() << "Already running";
		return;
	}
	m_ShouldAbort = false;
	enqueueAnother();
}





void BackgroundTempoDetector::stop()
{
	m_ShouldAbort = true;
}





Song::SharedDataPtr BackgroundTempoDetector::pickNextSong()
{
	// Return the first song that has no MPM set:
	auto db = m_Components.get<Database>();
	auto sdm = db->songSharedDataMap();
	for (auto itr = sdm.cbegin(), end = sdm.cend(); itr != end; ++itr)
	{
		auto sd = itr->second;
		{
			std::lock_guard<std::mutex> lock(m_MtxFailedSongs);
			if (m_FailedSongs.find(sd) != m_FailedSongs.end())
			{
				// Has failed recently, skip it
				continue;
			}
		}
		if (
			sd->m_TagManual.m_MeasuresPerMinute.isPresent() ||
			sd->m_DetectedTempo.isPresent()
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
				s->tagFileName().m_MeasuresPerMinute.isPresent() ||
				s->tagId3().m_MeasuresPerMinute.isPresent()
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
	m_IsRunning = true;
	auto taskName = SongTempoDetector::createTaskName(sd);
	auto td = m_Components.get<SongTempoDetector>();
	BackgroundTasks::get().enqueue(taskName, [this, sd, td]()
		{
			if (!td->detect(sd))
			{
				std::lock_guard<std::mutex> lock(m_MtxFailedSongs);
				m_FailedSongs.insert(sd);
			}
			if (!m_ShouldAbort.load())
			{
				enqueueAnother();
			}
			else
			{
				m_IsRunning = false;
			}
		}
	);
}

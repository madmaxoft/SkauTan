#include "TempoDetectTask.hpp"
#include <cassert>
#include "ComponentCollection.hpp"
#include "DB/Database.hpp"
#include "TempoDetector.hpp"
#include "Stopwatch.hpp"





/** Returns a vector of TempoDetector Options to be used for detecting in the specified tempo range. */
static std::vector<TempoDetector::Options> getTempoDetectorOptions(const std::pair<int, int> & a_TempoRange)
{
	TempoDetector::Options opt;
	opt.m_SampleRate = 500;
	opt.m_LevelAlgorithm = TempoDetector::laSumDistMinMax;
	opt.m_MinTempo = a_TempoRange.first;
	opt.m_MaxTempo = a_TempoRange.second;
	opt.m_ShouldNormalizeLevels = true;
	opt.m_NormalizeLevelsWindowSize = 31;
	opt.m_Stride = 8;
	opt.m_LocalMaxDistance = 13;
	std::vector<TempoDetector::Options> opts;
	for (auto ws: {11, 13, 15, 17, 19})
	{
		opt.m_WindowSize = static_cast<size_t>(ws);
		opts.push_back(opt);
	}
	return opts;
}





////////////////////////////////////////////////////////////////////////////////
// TempoDetectTask:

QString TempoDetectTask::createTaskName(Song::SharedDataPtr a_SongSD)
{
	assert(a_SongSD != nullptr);
	auto duplicates = a_SongSD->duplicates();
	if (duplicates.empty())
	{
		return tr("Detect tempo: unknown song");
	}
	else
	{
		return tr("Detect tempo: %1").arg(duplicates[0]->fileName());
	}
}





void TempoDetectTask::enqueue(ComponentCollection & a_Components, Song::SharedDataPtr a_SongSD)
{
	// Prefer to use a task class, because we're unsure on how exactly C++'s lambda captures work with
	// references given through function's parameters across threads.
	class DTask:
		public BackgroundTasks::Task
	{
		using Super = BackgroundTasks::Task;
		ComponentCollection & m_Components;
		Song::SharedDataPtr m_SongSD;
	public:
		DTask(const QString & a_TaskName, ComponentCollection & a_Components, Song::SharedDataPtr a_SongSD):
			Super(a_TaskName),
			m_Components(a_Components),
			m_SongSD(a_SongSD)
		{
		}
		virtual void execute() override
		{
			TempoDetectTask::detect(m_Components, m_SongSD);
		}
	};

	auto taskName = createTaskName(a_SongSD);
	BackgroundTasks::get().addTask(std::make_shared<DTask>(taskName, a_Components, a_SongSD));
}





bool TempoDetectTask::detect(ComponentCollection & a_Components, Song::SharedDataPtr a_SongSD)
{
	// Prepare the detection options, esp. the tempo range, if genre is known:
	auto tempoRange = Song::detectionTempoRangeForGenre("unknown");
	auto duplicates = a_SongSD->duplicates();
	for (auto s: duplicates)
	{
		auto genre = s->primaryGenre();
		if (genre.isPresent())
		{
			tempoRange = Song::detectionTempoRangeForGenre(genre.value());
			break;
		}
	}
	auto opts = getTempoDetectorOptions(tempoRange);

	// Detect from the first available duplicate:
	STOPWATCH("Detect tempo")
	TempoDetector td;
	for (auto s: duplicates)
	{
		auto res = td.scanSong(s->shared_from_this(), opts);
		if (res == nullptr)
		{
			// Reading the audio data failed, try another file (perhaps the file was removed):
			continue;
		}
		if (res->m_Tempo <= 0)
		{
			// Tempo detection failed, reading another file won't help
			break;
		}
		qDebug() << "Detected tempo in " << s->fileName() << ": " << res->m_Tempo;
		a_SongSD->m_DetectedTempo = res->m_Tempo;
		auto db = a_Components.get<Database>();
		db->saveSongSharedData(a_SongSD);
		return true;
	}
	qWarning() << "Cannot detect tempo in song hash " << a_SongSD->m_Hash << ", none of the files provided data for analysis.";
	return false;
}




////////////////////////////////////////////////////////////////////////////////
// TempoDetectTaskRepeater:

TempoDetectTaskRepeater::TempoDetectTaskRepeater(ComponentCollection & a_Components):
	m_Components(a_Components),
	m_IsRunning(false),
	m_ShouldAbort(true)
{
}





void TempoDetectTaskRepeater::start()
{
	if (m_IsRunning.load())
	{
		qDebug() << "Already running";
		return;
	}
	m_ShouldAbort = false;
	enqueueAnother();
}





void TempoDetectTaskRepeater::stop()
{
	m_ShouldAbort = true;
}





Song::SharedDataPtr TempoDetectTaskRepeater::pickNextSong()
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





void TempoDetectTaskRepeater::enqueueAnother()
{
	auto sd = pickNextSong();
	if (sd != nullptr)
	{
		m_IsRunning = true;
		auto taskName = TempoDetectTask::createTaskName(sd);
		BackgroundTasks::get().enqueue(taskName, [this, sd]()
			{
				if (!TempoDetectTask::detect(m_Components, sd))
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
}

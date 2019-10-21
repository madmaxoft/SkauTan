#include "SongTempoDetector.hpp"
#include <QAudioFormat>
#include "Audio/AVPP.hpp"
#include "Audio/PlaybackBuffer.hpp"
#include "BackgroundTasks.hpp"
#include "Song.hpp"
#include "Utils.hpp"





/** Returns a vector of TempoDetector Options to be used for detecting in the specified tempo range. */
static std::vector<SongTempoDetector::Options> optimalOptions(const std::pair<int, int> & aTempoRange)
{
	SongTempoDetector::Options opt;
	opt.mSampleRate = 500;
	opt.mLevelAlgorithm = TempoDetector::laSumDistMinMax;
	opt.mMinTempo = aTempoRange.first;
	opt.mMaxTempo = aTempoRange.second;
	opt.mShouldNormalizeLevels = true;
	opt.mNormalizeLevelsWindowSize = 31;
	opt.mStride = 8;
	opt.mLocalMaxDistance = 13;
	std::vector<SongTempoDetector::Options> opts;
	for (auto ws: {11, 13, 15, 17, 19})
	{
		opt.mWindowSize = static_cast<size_t>(ws);
		opts.push_back(opt);
	}
	return opts;
}





/** Outputs a debug audio file with one channel of the original audio data and the other channel
of the detected levels. */
void debugLevelsInAudioData(
	const SongTempoDetector::Options & aOptions,
	const PlaybackBuffer & aBuf,
	const std::vector<qint32> & aLevels
)
{
	if (aOptions.mDebugAudioLevelsFileName.isEmpty())
	{
		return;
	}
	QFile f(aOptions.mDebugAudioLevelsFileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		qDebug() << "Cannot open debug file " << aOptions.mDebugAudioLevelsFileName;
		return;
	}
	size_t maxIdx = aLevels.size();
	qint32 maxLevel = 1;
	for (const auto & lev: aLevels)
	{
		if (lev > maxLevel)
		{
			maxLevel = lev;
		}
	}
	auto audio = reinterpret_cast<const qint16 *>(aBuf.audioData().data());
	std::vector<qint16> interlaced;
	interlaced.resize(aOptions.mStride * 2);
	for (size_t i = 0; i < maxIdx; ++i)
	{
		float outF = static_cast<float>(aLevels[i]) / maxLevel;
		qint16 outLevel = static_cast<qint16>(Utils::clamp<qint32>(static_cast<qint32>(outF * 65535) - 32768, -32768, 32767));
		for (size_t j = 0; j < aOptions.mStride; ++j)
		{
			interlaced[2 * j] = audio[i * aOptions.mStride + j];
			interlaced[2 * j + 1] = outLevel;
		}
		f.write(
			reinterpret_cast<const char *>(interlaced.data()),
			static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
		);
	}
}





/** Outputs the debug audio data with mixed-in beats. */
static void debugBeatsInAudioData(
	const SongTempoDetector::Options & aOptions,
	const PlaybackBuffer & aBuf,
	const std::vector<std::pair<size_t, qint32>> & aBeats
)
{
	if (aOptions.mDebugAudioBeatsFileName.isEmpty())
	{
		return;
	}
	QFile f(aOptions.mDebugAudioBeatsFileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		qDebug() << "Cannot open debug file " << aOptions.mDebugAudioBeatsFileName;
		return;
	}
	qint32 maxBeatWeight = 0;
	for (const auto & beat: aBeats)
	{
		if (beat.second > maxBeatWeight)
		{
			maxBeatWeight = beat.second;
		}
	}
	float levelCoeff = 32767.0f / maxBeatWeight;
	size_t lastBeatIdx = 0;
	size_t maxIdx = aBeats.back().first;
	qint16 mixStrength = 0;
	std::vector<qint16> interlaced;
	interlaced.resize(aOptions.mStride * 2);
	for (size_t i = 0; i < maxIdx; ++i)
	{
		if (i == aBeats[lastBeatIdx].first)
		{
			mixStrength = static_cast<qint16>(aBeats[lastBeatIdx].second * levelCoeff);
			lastBeatIdx += 1;
		}
		auto audio = reinterpret_cast<const qint16 *>(aBuf.audioData().data()) + i * aOptions.mStride;
		for (size_t j = 0; j < aOptions.mStride; ++j)
		{
			interlaced[2 * j] = audio[j];
			auto m = static_cast<int>(mixStrength * sin(static_cast<float>(j + aOptions.mStride * i) * 2000 / aOptions.mSampleRate));
			interlaced[2 * j + 1]  = static_cast<qint16>(Utils::clamp<int>(m, -32768, 32767));
		}
		mixStrength = static_cast<qint16>(mixStrength * 0.8);
		f.write(
			reinterpret_cast<const char *>(interlaced.data()),
			static_cast<qint64>(interlaced.size() * sizeof(interlaced[0]))
		);
	}
}





////////////////////////////////////////////////////////////////////////////////
// SongTempoDetector:

SongTempoDetector::SongTempoDetector():
	mScanQueueLength(0)
{
}





std::shared_ptr<TempoDetector::Result> SongTempoDetector::scanSong(SongPtr aSong, const std::vector<Options> & aOptions)
{
	if (aOptions.empty())
	{
		qWarning() << "Tempo detection started with no options given.";
		assert(!"Tempo detection started with no options given.");
		return nullptr;
	}

	// Decode the entire file into memory:
	auto context = AVPP::Format::createContext(aSong->fileName());
	if (context == nullptr)
	{
		qWarning() << "Cannot open file " << aSong->fileName();
		return nullptr;
	}
	QAudioFormat fmt;
	fmt.setSampleRate(aOptions[0].mSampleRate);
	fmt.setChannelCount(1);
	fmt.setSampleSize(16);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
	fmt.setCodec("audio/pcm");
	PlaybackBuffer buf(fmt);
	if (!context->routeAudioTo(&buf))
	{
		qWarning() << "Cannot route audio from file " << aSong->fileName();
		return nullptr;
	}
	context->decode();
	if (buf.shouldAbort())
	{
		qDebug() << "Decoding audio failed: " << aSong->fileName();
		return nullptr;
	}

	// Scan the song using all the options:
	std::vector<TempoDetector::ResultPtr> res;
	auto samples = reinterpret_cast<const Int16 *>(buf.audioData().data());
	auto numSamples = buf.audioData().size() / sizeof(Int16);
	for (const auto & opt: aOptions)
	{
		auto result = TempoDetector::scan(samples, numSamples, opt);
		res.push_back(result);
		debugBeatsInAudioData(opt, buf, result->mBeats);
		debugLevelsInAudioData(opt, buf, result->mLevels);
	}

	// Aggregate the detection results into a single one:
	std::tie(res[0]->mTempo, res[0]->mConfidence) = TempoDetector::aggregateResults(res);
	emit songScanned(aSong, res[0]);
	return res[0];
}





void SongTempoDetector::queueScanSong(SongPtr aSong, const std::vector<Options> & aOptions)
{
	auto options = aOptions;  // Make a copy for the background thread
	mScanQueueLength += 1;
	BackgroundTasks::enqueue(tr("Detect tempo: %1").arg(aSong->fileName()),
		[this, aSong, options]()
		{
			mScanQueueLength -= 1;
			scanSong(aSong, options);
		}
	);
}





QString SongTempoDetector::createTaskName(Song::SharedDataPtr aSongSD)
{
	assert(aSongSD != nullptr);
	auto duplicates = aSongSD->duplicates();
	if (duplicates.empty())
	{
		return tr("Detect tempo: unknown song");
	}
	else
	{
		return tr("Detect tempo: %1").arg(duplicates[0]->fileName());
	}
}





bool SongTempoDetector::detect(Song::SharedDataPtr aSongSD)
{
	// Prepare the detection options, esp. the tempo range, if genre is known:
	auto tempoRange = Song::detectionTempoRangeForGenre("unknown");
	auto duplicates = aSongSD->duplicates();
	for (auto s: duplicates)
	{
		auto genre = s->primaryGenre();
		if (genre.isPresent())
		{
			tempoRange = Song::detectionTempoRangeForGenre(genre.value());
			break;
		}
	}
	auto opts = optimalOptions(tempoRange);

	// Detect from the first available duplicate:
	STOPWATCH("Detect tempo")
	for (auto s: duplicates)
	{
		auto res = scanSong(s->shared_from_this(), opts);
		if (res == nullptr)
		{
			// Reading the audio data failed, try another file (perhaps the file was removed):
			continue;
		}
		if (res->mTempo <= 0)
		{
			// Tempo detection failed, reading another file won't help
			break;
		}
		qDebug() << "Detected tempo in " << s->fileName() << ": " << res->mTempo;
		aSongSD->mDetectedTempo = res->mTempo;
		emit songTempoDetected(aSongSD);
		return true;
	}
	qWarning() << "Cannot detect tempo in song hash " << aSongSD->mHash << ", none of the files provided data for analysis.";
	return false;
}





void SongTempoDetector::queueDetect(Song::SharedDataPtr aSongSD)
{
	assert(aSongSD != nullptr);
	BackgroundTasks::enqueue(
		createTaskName(aSongSD),
		[aSongSD, this]()
		{
			detect(aSongSD);
		}
	);
}

#include "WaveformDisplay.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include "../Audio/Player.hpp"
#include "../PlaylistItemSong.hpp"
#include "../Utils.hpp"





WaveformDisplay::WaveformDisplay(QWidget * aParent):
	Super(aParent),
	mPlayer(nullptr),
	mPlaybackBuffer(nullptr),
	mWidth(1000),
	mIsPeakDataComplete(false)
{
	mPeaks.resize(static_cast<size_t>(mWidth));
	mSums.resize(static_cast<size_t>(mWidth));
	connect(&mUpdateTimer, &QTimer::timeout, this, &WaveformDisplay::updateOnTimer);
}





void WaveformDisplay::setPlayer(Player & aPlayer)
{
	if (mPlayer != nullptr)
	{
		disconnect(mPlayer);
	}
	mPlayer = &aPlayer;
	connect(mPlayer, &Player::startedPlayback,  this, &WaveformDisplay::playerStartedPlayback);
	connect(mPlayer, &Player::finishedPlayback, this, &WaveformDisplay::playerFinishedPlayback);

	// If the player has a track loaded, display it in the widget:
	if (mPlayer->isTrackLoaded())
	{
		playerStartedPlayback(mPlayer->currentTrack(), mPlayer->playbackBuffer());
	}
}





void WaveformDisplay::paint(QPainter & aPainter, int aHeight)
{
	if (mPlaybackBuffer == nullptr)
	{
		return;
	}
	Utils::QPainterSaver saver(aPainter);

	// Paint the skip-start background:
	int skipStartPx = 0;
	if ((mCurrentSong != nullptr) && mCurrentSong->skipStart().isPresent())
	{
		skipStartPx = timeToScreen(mCurrentSong->skipStart().value());
		aPainter.setBrush(QColor(128, 0, 0));
		aPainter.drawRect(0, 0, skipStartPx, aHeight);
	}

	// Paint the duration limit background:
	int durationLimitCutoffPx = mWidth;
	auto durLimit = mPlayer->currentTrack()->durationLimit();
	if (durLimit >= 0)
	{
		auto dlc = mPlayer->currentPosition() + durLimit - mPlayer->totalTime();
		durationLimitCutoffPx = timeToScreen(dlc);
		aPainter.setBrush(QColor(128, 0, 0));
		aPainter.drawRect(durationLimitCutoffPx, 0, mWidth, aHeight);
	}

	// Paint the waveform:
	int halfHeight = aHeight / 2;
	int mid = halfHeight;
	aPainter.setPen(QColor(255, 128, 128));
	for (int i = 0; i < mWidth; ++i)
	{
		if (i == skipStartPx)
		{
			aPainter.setPen(QColor(128, 128, 128));
		}
		if (i == durationLimitCutoffPx)
		{
			aPainter.setPen(QColor(255, 128, 128));
		}
		int v = mPeaks[static_cast<unsigned>(i)] * halfHeight / std::numeric_limits<short>::max();
		aPainter.drawLine(i, mid - v, i, mid + v);
	}
	aPainter.setPen(QColor(255, 192, 192));
	for (int i = 0; i < mWidth; ++i)
	{
		if (i == skipStartPx)
		{
			aPainter.setPen(QColor(255, 255, 255));
		}
		if (i == durationLimitCutoffPx)
		{
			aPainter.setPen(QColor(255, 192, 192));
		}
		int v = mSums[static_cast<unsigned>(i)] * halfHeight / std::numeric_limits<short>::max();
		aPainter.drawLine(i, mid - v, i, mid + v);
	}

	// Paint the player position:
	auto pos = mPlaybackBuffer->readPos();
	auto total = mPlaybackBuffer->bufferLimit();
	aPainter.setPen(QPen(QBrush(QColor(0, 160, 0)), 2));
	int left = static_cast<int>(static_cast<float>(mWidth) * pos / total);
	aPainter.drawLine(left, 0, left, aHeight);

	// Paint the skip-start end-line:
	if (skipStartPx > 0)
	{
		aPainter.setPen(QColor(255, 0, 0));
		aPainter.drawLine(skipStartPx, 0, skipStartPx, aHeight);
	}

	// Paint the duration limit end-line:
	if (durationLimitCutoffPx < mWidth)
	{
		aPainter.setPen(QColor(255, 0, 0));
		aPainter.drawLine(durationLimitCutoffPx, 0, durationLimitCutoffPx, aHeight);
	}
}





void WaveformDisplay::calculatePeaks()
{
	if (mPlaybackBuffer == nullptr)
	{
		std::fill(mPeaks.begin(), mPeaks.end(), 0);
		std::fill(mSums.begin(), mSums.end(), 0);
		return;
	}

	// For the peak calculation we consider the playback buffer as having a single channel only
	// since it doesn't matter visually.
	auto totalSamples = mPlaybackBuffer->bufferLimit() / sizeof(short);
	auto writePos = mPlaybackBuffer->writePos() / sizeof(short);
	mIsPeakDataComplete = (writePos == totalSamples);
	auto width = static_cast<size_t>(mWidth);
	size_t fromSample = 0;
	auto sample = reinterpret_cast<const short *>(mPlaybackBuffer->audioData().data());
	for (size_t i = 0; i < width; ++i)
	{
		size_t toSample = totalSamples * (i + 1) / width;
		if (toSample >= writePos)
		{
			std::fill(mPeaks.begin() + static_cast<int>(i), mPeaks.end(), 0);
			std::fill(mSums.begin() + static_cast<int>(i), mSums.end(), 0);
			break;
		}
		short minValue = 0;
		short maxValue = 0;
		int sum = 0;
		for (size_t sampleIdx = fromSample; sampleIdx < toSample; ++sampleIdx)
		{
			if (sample[sampleIdx] < minValue)
			{
				minValue = sample[sampleIdx];
			}
			if (sample[sampleIdx] > maxValue)
			{
				maxValue = sample[sampleIdx];
			}
			sum += std::abs(sample[sampleIdx]);
		}
		mPeaks[i] = std::max<short>(maxValue, -minValue);
		mSums[i] = static_cast<int>(sum / static_cast<int>(toSample - fromSample + 1));
		fromSample = toSample;
	}
}





void WaveformDisplay::setSkipStart(int aPosX)
{
	if (mCurrentSong == nullptr)
	{
		qDebug() << "Not a song, ignoring.";
		return;
	}
	auto skipStart = screenToTime(aPosX);
	qDebug() << "Setting skip-start for " << mCurrentSong->fileName() << " to " << skipStart;
	mCurrentSong->setSkipStart(skipStart);
	emit songChanged(mCurrentSong);
	update();
}





void WaveformDisplay::delSkipStart()
{
	if (mCurrentSong == nullptr)
	{
		qDebug() << "Not a song, ignoring.";
		return;
	}
	mCurrentSong->delSkipStart();
	emit songChanged(mCurrentSong);
	update();
}





void WaveformDisplay::paintEvent(QPaintEvent * aEvent)
{
	QPainter painter(this);
	painter.fillRect(aEvent->rect(), QBrush(QColor(0, 0, 0)));
	paint(painter, aEvent->rect().height());
}





void WaveformDisplay::resizeEvent(QResizeEvent * aEvent)
{
	mWidth = aEvent->size().width();
	mPeaks.resize(static_cast<size_t>(mWidth));
	mSums.resize(static_cast<size_t>(mWidth));
	calculatePeaks();
}





QSize WaveformDisplay::sizeHint() const
{
	// Base our size hint on font size, although we're not drawing any text
	int wid = fontMetrics().width("-000:00 | 000:00");
	int hei = fontMetrics().height() * 2 + 2;
	return QSize(wid, hei);
}





void WaveformDisplay::mouseReleaseEvent(QMouseEvent * aEvent)
{
	if ((mPlaybackBuffer == nullptr) || (mPlayer == nullptr))
	{
		return;
	}

	if (aEvent->button() != Qt::LeftButton)
	{
		return;
	}
	mPlayer->seekTo(screenToTime(aEvent->pos().x()));
}





void WaveformDisplay::contextMenuEvent(QContextMenuEvent * aEvent)
{
	auto x = aEvent->x();
	QMenu menu;
	auto actSetSkipStart = menu.addAction(tr("Set skip-start here"));
	auto actDelSkipStart = menu.addAction(tr("Remove skip-start"));
	actSetSkipStart->setEnabled(mCurrentSong != nullptr);
	actDelSkipStart->setEnabled((mCurrentSong != nullptr) && mCurrentSong->skipStart().isPresent());
	connect(actSetSkipStart, &QAction::triggered, [this, x]() { setSkipStart (x); });
	connect(actDelSkipStart, &QAction::triggered, [this]() { delSkipStart(); });
	menu.exec(aEvent->globalPos());
}





int WaveformDisplay::timeToScreen(double aSeconds)
{
	assert(mPlaybackBuffer != nullptr);
	return static_cast<int>(mWidth * aSeconds / mPlaybackBuffer->duration());
}





double WaveformDisplay::screenToTime(int aPosX)
{
	assert(mPlaybackBuffer != nullptr);
	assert(mWidth > 0);
	auto x = Utils::clamp(aPosX, 0, mWidth);
	return mPlaybackBuffer->duration() * x / mWidth;
}





void WaveformDisplay::playerStartedPlayback(IPlaylistItemPtr aItem, PlaybackBufferPtr aPlaybackBuffer)
{
	Q_UNUSED(aItem);

	mPlaybackBuffer = aPlaybackBuffer;
	mUpdateTimer.start(50);
	mIsPeakDataComplete = false;
	auto plis = std::dynamic_pointer_cast<PlaylistItemSong>(mPlayer->currentTrack());
	if (plis != nullptr)
	{
		mCurrentSong = plis->song();
	}
}





void WaveformDisplay::playerFinishedPlayback()
{
	mPlaybackBuffer = nullptr;
	mUpdateTimer.stop();
}





void WaveformDisplay::updateOnTimer()
{
	// Recalculate the peaks, unless we already have the full data:
	if (!mIsPeakDataComplete)
	{
		calculatePeaks();
	}
	update();
}

#include "WaveformDisplay.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include "Player.h"
#include "PlaylistItemSong.h"





WaveformDisplay::WaveformDisplay(QWidget * a_Parent):
	Super(a_Parent),
	m_Player(nullptr),
	m_PlaybackBuffer(nullptr),
	m_Width(1000),
	m_IsPeakDataComplete(false)
{
	m_Peaks.resize(static_cast<size_t>(m_Width));
	m_Sums.resize(static_cast<size_t>(m_Width));
	connect(&m_UpdateTimer, &QTimer::timeout, this, &WaveformDisplay::updateOnTimer);
}





void WaveformDisplay::setPlayer(Player & a_Player)
{
	if (m_Player != nullptr)
	{
		disconnect(m_Player);
	}
	m_Player = &a_Player;
	connect(m_Player, &Player::startedPlayback,  this, &WaveformDisplay::playerStartedPlayback);
	connect(m_Player, &Player::finishedPlayback, this, &WaveformDisplay::playerFinishedPlayback);

	// If the player has a track loaded, display it in the widget:
	if (m_Player->isTrackLoaded())
	{
		playerStartedPlayback(m_Player->currentTrack(), m_Player->playbackBuffer());
	}
}





void WaveformDisplay::paint(QPainter & a_Painter, const QPoint & a_Origin, int a_Height)
{
	if (m_PlaybackBuffer == nullptr)
	{
		return;
	}

	// Paint the waveform:
	int halfHeight = a_Height / 2;
	int mid = a_Origin.y() + halfHeight;
	a_Painter.setPen(QColor(128, 128, 128));
	for (int i = 0; i < m_Width; ++i)
	{
		int left = a_Origin.x() + i;
		int v = m_Peaks[static_cast<unsigned>(i)] * halfHeight / std::numeric_limits<short>::max();
		a_Painter.drawLine(left, mid - v, left, mid + v);
	}
	a_Painter.setPen(QColor(255, 255, 255));
	for (int i = 0; i < m_Width; ++i)
	{
		int left = a_Origin.x() + i;
		int v = m_Sums[static_cast<unsigned>(i)] * halfHeight / std::numeric_limits<short>::max();
		a_Painter.drawLine(left, mid - v, left, mid + v);
	}

	// Paint the player position:
	auto pos = m_PlaybackBuffer->readPos();
	auto total = m_PlaybackBuffer->bufferLimit();
	a_Painter.setPen(QPen(QBrush(QColor(0, 160, 0)), 2));
	int left = static_cast<int>(static_cast<size_t>(m_Width) * pos / total);
	a_Painter.drawLine(left, 0, left, a_Height);

	// Paint the skip-start:
	if (m_CurrentSong != nullptr)
	{
		auto skipStart = m_CurrentSong->skipStart();
		if (skipStart.isPresent())
		{
			auto left = static_cast<int>(m_Width * skipStart.value() / m_PlaybackBuffer->duration());
			a_Painter.setPen(QColor(255, 0, 0));
			a_Painter.drawLine(left, 0, left, a_Height);
		}
	}
}





void WaveformDisplay::calculatePeaks()
{
	if (m_PlaybackBuffer == nullptr)
	{
		std::fill(m_Peaks.begin(), m_Peaks.end(), 0);
		std::fill(m_Sums.begin(), m_Sums.end(), 0);
		return;
	}

	// For the peak calculation we consider the playback buffer as having a single channel only
	// since it doesn't matter visually.
	auto totalSamples = m_PlaybackBuffer->bufferLimit() / sizeof(short);
	auto writePos = m_PlaybackBuffer->writePos() / sizeof(short);
	m_IsPeakDataComplete = (writePos == totalSamples);
	auto width = static_cast<size_t>(m_Width);
	size_t fromSample = 0;
	auto sample = reinterpret_cast<const short *>(m_PlaybackBuffer->audioData().data());
	for (size_t i = 0; i < width; ++i)
	{
		size_t toSample = totalSamples * (i + 1) / width;
		if (toSample >= writePos)
		{
			std::fill(m_Peaks.begin() + static_cast<int>(i), m_Peaks.end(), 0);
			std::fill(m_Sums.begin() + static_cast<int>(i), m_Sums.end(), 0);
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
		m_Peaks[i] = std::max<short>(maxValue, -minValue);
		m_Sums[i] = static_cast<int>(sum / static_cast<int>(toSample - fromSample + 1));
		fromSample = toSample;
	}
}





void WaveformDisplay::setSkipStart(int a_PosX)
{
	if (m_CurrentSong == nullptr)
	{
		qDebug() << "Not a song, ignoring.";
		return;
	}
	auto bufferBytes = static_cast<qint32>(m_PlaybackBuffer->bufferLimit());
	auto songLength = static_cast<double>(m_PlaybackBuffer->format().durationForBytes(bufferBytes)) / 1000000;
	qDebug() << "Setting skip-start for " << m_CurrentSong->fileName() << " to " << a_PosX * songLength / width();
	m_CurrentSong->setSkipStart(a_PosX * songLength / width());
	emit songChanged(m_CurrentSong);
	update();
}





void WaveformDisplay::delSkipStart()
{
	if (m_CurrentSong == nullptr)
	{
		qDebug() << "Not a song, ignoring.";
		return;
	}
	m_CurrentSong->delSkipStart();
	emit songChanged(m_CurrentSong);
	update();
}





void WaveformDisplay::paintEvent(QPaintEvent * a_Event)
{
	QPainter painter(this);
	painter.fillRect(a_Event->rect(), QBrush(QColor(0, 0, 0)));
	paint(painter, QPoint(0, 0), a_Event->rect().height());
}





void WaveformDisplay::resizeEvent(QResizeEvent * a_Event)
{
	m_Width = a_Event->size().width();
	m_Peaks.resize(static_cast<size_t>(m_Width));
	m_Sums.resize(static_cast<size_t>(m_Width));
	calculatePeaks();
}





QSize WaveformDisplay::sizeHint() const
{
	// Base our size hint on font size, although we're not drawing any text
	int wid = fontMetrics().width("-000:00 | 000:00");
	int hei = fontMetrics().height() * 2 + 2;
	return QSize(wid, hei);
}





void WaveformDisplay::mouseReleaseEvent(QMouseEvent * a_Event)
{
	if ((m_PlaybackBuffer == nullptr) || (m_Player == nullptr))
	{
		return;
	}

	if (a_Event->button() != Qt::LeftButton)
	{
		return;
	}
	qint64 x = std::max(std::min(a_Event->pos().x(), width()), 0);  // clamp x between 0 and width()
	m_Player->seekTo(m_PlaybackBuffer->duration() * x / width());
}





void WaveformDisplay::contextMenuEvent(QContextMenuEvent * a_Event)
{
	auto x = a_Event->x();
	QMenu menu;
	auto actSetSkipStart = menu.addAction(tr("Set skip-start here"));
	auto actDelSkipStart = menu.addAction(tr("Remove skip-start"));
	actSetSkipStart->setEnabled(m_CurrentSong != nullptr);
	actDelSkipStart->setEnabled((m_CurrentSong != nullptr) && m_CurrentSong->skipStart().isPresent());
	connect(actSetSkipStart, &QAction::triggered, [this, x]() { setSkipStart (x); });
	connect(actDelSkipStart, &QAction::triggered, [this]() { delSkipStart(); });
	menu.exec(a_Event->globalPos());
}





void WaveformDisplay::playerStartedPlayback(IPlaylistItemPtr a_Item, PlaybackBufferPtr a_PlaybackBuffer)
{
	Q_UNUSED(a_Item);

	m_PlaybackBuffer = a_PlaybackBuffer;
	m_UpdateTimer.start(50);
	m_IsPeakDataComplete = false;
	auto plis = std::dynamic_pointer_cast<PlaylistItemSong>(m_Player->currentTrack());
	if (plis != nullptr)
	{
		m_CurrentSong = plis->song();
	}
}





void WaveformDisplay::playerFinishedPlayback()
{
	m_PlaybackBuffer = nullptr;
	m_UpdateTimer.stop();
}





void WaveformDisplay::updateOnTimer()
{
	// Recaulculate the peaks, unless we already have the full data:
	if (!m_IsPeakDataComplete)
	{
		calculatePeaks();
	}
	update();
}

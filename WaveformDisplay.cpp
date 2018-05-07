#include "WaveformDisplay.h"
#include <QPainter>
#include <QPaintEvent>
#include "Player.h"





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
	a_Painter.setPen(QColor(0, 255, 0));
	int left = static_cast<int>(static_cast<size_t>(m_Width) * pos / total);
	a_Painter.drawLine(left, 0, left, a_Height);
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





void WaveformDisplay::playerStartedPlayback(IPlaylistItemPtr a_Item, PlaybackBufferPtr a_PlaybackBuffer)
{
	Q_UNUSED(a_Item);

	m_PlaybackBuffer = a_PlaybackBuffer;
	m_UpdateTimer.start(50);
	m_IsPeakDataComplete = false;
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

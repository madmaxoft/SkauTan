#include "Player.h"
#include <QDebug>
#include "Playlist.h"
#include "PlaybackBuffer.h"





Player::Player(std::shared_ptr<Playlist> a_Playlist, QObject * a_Parent):
	Super(a_Parent),
	m_State(psStopped),
	m_Playlist(a_Playlist)
{
	QAudioFormat fmt;
	fmt.setSampleRate(48000);
	fmt.setChannelCount(2);
	fmt.setSampleSize(16);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setByteOrder(QAudioFormat::LittleEndian);
	fmt.setCodec("audio/pcm");
	if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(fmt))
	{
		qDebug() << "Using 48 kHz output format";
		m_Output.reset(new QAudioOutput(fmt));
		return;
	}
	fmt.setSampleRate(44100);
	if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(fmt))
	{
		qDebug() << "Using 48 kHz output format";
		m_Output.reset(new QAudioOutput(fmt));
		return;
	}
	qWarning() << "No basic output format supported, attempting default";
	m_Output.reset(new QAudioOutput());
	qDebug() << "Audio format used: " << m_Output->format();
}





void Player::startPlaying()
{
	m_State = psPlaying;
	m_CurrentBuffer = m_Playlist->current()->startDecoding(m_Output->format());
	if (m_CurrentBuffer == nullptr)
	{
		m_State = psStopped;
	}
	connect(m_CurrentBuffer.get(), &PlaybackBuffer::dataReady, this, &Player::bufferDataReady);
}





void Player::startStop()
{
	switch (m_State)
	{
		case psStopped:
		{
			startPlaying();
			break;
		}
		case psPlaying:
		{
			// Fade out, then stop:
			m_CurrentBuffer->fadeOut();
			m_State = psStopped;
			break;
		}
		case psFadeOut:
		{
			// Already fading out, just stop after the fadeout
			m_State = psStopped;
		}
	}
}





void Player::prevItem()
{
	m_Playlist->prevItem();
	switch (m_State)
	{
		case psStopped:
		{
			// TODO: Should we start playing?
			break;
		}
		case psPlaying:
		{
			m_CurrentBuffer->fadeOut();
			break;
		}
		case psFadeOut:
		{
			// Already fading out, just advance the playlist
			break;
		}
	}
}





void Player::nextItem()
{
	m_Playlist->nextItem();
	switch (m_State)
	{
		case psStopped:
		{
			// TODO: Should we start playing?
			break;
		}
		case psPlaying:
		{
			m_CurrentBuffer->fadeOut();
			break;
		}
		case psFadeOut:
		{
			// Already fading out, just advance the playlist
			break;
		}
	}
}





void Player::audioOutputStateChanged(QAudio::State a_NewState)
{
	switch (a_NewState)
	{
		case QAudio::IdleState:
		{
			// Finished playing, no more data
			switch (m_State)
			{
				case psPlaying:
				{
					// Reached a song end without a fade-out, advance to the next song:
					m_Playlist->nextItem();
					break;
				}
				case psFadeOut:
				{
					// The next song to play is already set in m_Playlist
					break;
				}
				default:
				{
					// Playback stopped, close the audio device:
					m_Output->stop();
					return;
				}
			}
			startPlaying();
			break;
		}

		case QAudio::StoppedState:
		{
			// Stopped for other reason, possibly error
			if (m_Output->error() != QAudio::NoError)
			{
				qDebug() << "Error while outputting audio: " << m_Output->error();
			}
			break;
		}
	}
}





void Player::bufferDataReady()
{
	if (m_Output->state() != QAudio::ActiveState)
	{
		qDebug() << "Buffer data ready, starting actual playback";
		m_Output->start(m_CurrentBuffer.get());
	}
}

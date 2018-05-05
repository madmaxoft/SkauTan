#include "Player.h"
#include <assert.h>
#include <QDebug>
#include "Playlist.h"
#include "PlaybackBuffer.h"
#include "AudioEffects.h"





Player::Player(QObject * a_Parent):
	Super(a_Parent),
	m_Playlist(new Playlist),
	m_State(psStopped),
	m_FadeoutProgress(0),
	m_Tempo(1)
{
	// Pick an audio format:
	m_Format.setSampleRate(48000);
	m_Format.setChannelCount(2);
	m_Format.setSampleSize(16);
	m_Format.setSampleType(QAudioFormat::SignedInt);
	m_Format.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
	m_Format.setCodec("audio/pcm");
	if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(m_Format))
	{
		qDebug() << "Using 48 kHz output format";
	}
	else
	{
		m_Format.setSampleRate(44100);
		if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(m_Format))
		{
			qDebug() << "Using 44.1 kHz output format";
		}
		else
		{
			qWarning() << "No basic output format supported, attempting default";
			m_Format = QAudioDeviceInfo::defaultOutputDevice().preferredFormat();
			qDebug() << "Output audio format: " << m_Format;
		}
	}
	m_Thread.setObjectName("AudioOutput");
	m_Output.reset(new QAudioOutput(m_Format));
	m_Output->moveToThread(&m_Thread);
	connect(m_Output.get(), &QAudioOutput::stateChanged, this, &Player::outputStateChanged);
	m_Thread.start(QThread::HighPriority);
}





Player::~Player()
{
	m_Thread.quit();
	m_Thread.wait();
}





double Player::currentPosition() const
{
	if (m_AudioDataSource == nullptr)
	{
		return 0;
	}
	return m_AudioDataSource->currentSongPosition();
}





void Player::fadeOut(Player::State a_FadeOutState)
{
	assert((a_FadeOutState == psFadeOutToStop) || (a_FadeOutState == psFadeOutToTrack));  // Requested a fadeout
	assert((m_State != psFadeOutToStop) && (m_State != psFadeOutToTrack));  // Not already fading out

	m_FadeoutProgress = 0;
	m_State = a_FadeOutState;
	if (m_AudioDataSource != nullptr)
	{
		m_AudioDataSource->fadeOut(500);  // TODO: Settable FadeOut length
	}
}





void Player::seekTo(double a_Time)
{
	if (m_State != psPlaying)
	{
		return;
	}
	if (m_AudioDataSource == nullptr)
	{
		return;
	}
	m_AudioDataSource->seekTo(a_Time);
}





void Player::setVolume(qreal a_NewVolume)
{
	m_Output->setVolume(a_NewVolume);
}





void Player::setTempo(qreal a_NewTempo)
{
	m_Tempo = a_NewTempo;
	if (m_AudioDataSource == nullptr)
	{
		return;
	}
	m_AudioDataSource->setTempo(a_NewTempo);
}





void Player::nextTrack()
{
	if (!m_Playlist->nextItem())
	{
		// There's no next track in the playlist
		return;
	}
	switch (m_State)
	{
		case psStopped:
		{
			return;
		}
		case psPlaying:
		{
			fadeOut(psFadeOutToTrack);
			return;
		}
		case psFadeOutToTrack:
		{
			// Already fading out, no extra action to be taken
			break;
		}
		case psFadeOutToStop:
		{
			// Already fading out, change to starting the next track afterwards
			m_State = psFadeOutToTrack;
			break;
		}
	}
}





void Player::prevTrack()
{
	if (!m_Playlist->prevItem())
	{
		// There's no prev track in the playlist
		return;
	}
	switch (m_State)
	{
		case psStopped:
		{
			return;
		}
		case psPlaying:
		{
			fadeOut(psFadeOutToTrack);
			return;
		}
		case psFadeOutToTrack:
		{
			// Already fading out, no extra action to be taken
			break;
		}
		case psFadeOutToStop:
		{
			// Already fading out, change to starting the prev track afterwards
			m_State = psFadeOutToTrack;
			break;
		}
	}
}





void Player::startPause()
{
	switch (m_State)
	{
		case psStopped:
		{
			start();
			return;
		}
		case psPlaying:
		{
			pause();
			return;
		}
		case psFadeOutToTrack:
		{
			// Already fading out, just stop afterwards
			m_State = psFadeOutToStop;
			return;
		}
		case psFadeOutToStop:
		{
			return;
		}
	}
}





void Player::start()
{
	auto track = m_Playlist->currentItem();
	if (track == nullptr)
	{
		return;
	}
	switch (m_State)
	{
		case psFadeOutToStop:
		{
			// We were stopping a track; continue stopping, but schedule the new track to start playing afterwards
			m_State = psFadeOutToTrack;
		return;
	}
		case psFadeOutToTrack:
		{
			// We're already scheduled to start playing the new track, ignore the request
			return;
		}
		case psPlaying:
		{
			// We're already playing, ignore the request
			return;
		}
		case psStopped:
		{
			// We're stopped, start the playback:
			qDebug() << "Player: Starting playback of track " << track->displayName();
			emit startingPlayback(track.get());
			auto decoder = track->startDecoding(m_Format);
			if (decoder == nullptr)
			{
				qDebug() << "Cannot start playback, decoder returned failure";
				// TODO: Next song?
				return;
			}
			if (!decoder->waitForData())
			{
				qDebug() << "Cannot start playback, decoder didn't produce any initial data.";
				return;
			}
			auto audioDataSource =
				std::make_unique<AudioFadeOut>(
				std::make_unique<AudioTempoChange>(
				std::move(decoder)
			));
			audioDataSource->setTempo(m_Tempo);
			m_AudioDataSource = std::make_unique<AudioDataSourceIO>(std::move(audioDataSource));
			auto bufSize = m_Format.bytesForDuration(300 * 1000);  // 300 msec buffer
			qDebug() << "Setting audio output buffer size to " << bufSize;
			m_Output->setBufferSize(bufSize);
			m_Output->start(m_AudioDataSource.get());
			m_State = psPlaying;
			emit startedPlayback(track.get());
		}
	}
}





void Player::pause()
{
	if (m_State != psPlaying)
	{
		return;
	}

	fadeOut(psFadeOutToStop);
}





void Player::jumpTo(int a_ItemIdx)
{
	if (!m_Playlist->setCurrentItem(a_ItemIdx))
	{
		// Invalid index
		return;
	}
	switch (m_State)
	{
		case psPlaying:
		{
			fadeOut(psFadeOutToTrack);
			break;
		}
		case psStopped:
		{
			start();
			break;
		}
		case psFadeOutToStop:
		{
			m_State = psFadeOutToTrack;
			break;
		}
		case psFadeOutToTrack:
		{
			// Nothing needed
			break;
		}
	}
}





void Player::outputStateChanged(QAudio::State a_NewState)
{
	if (a_NewState == QAudio::IdleState)
	{
		// The playe has become idle, which means there's no more audio to play.
		// Either the song finished, or the fadeout was completed.
		switch (m_State)
		{
			case psPlaying:
			{
				// Play the next song in the playlist, if any:
				m_State = psStopped;
				emit finishedPlayback();
				if (m_Playlist->nextItem())
				{
					start();
				}
				return;
			}
			case psFadeOutToStop:
			{
				// Stop playing completely:
				m_State = psStopped;
				emit finishedPlayback();
				return;
			}
			case psFadeOutToTrack:
			{
				// Start playing the next scheduled track:
				m_State = psStopped;
				emit finishedPlayback();
				start();
				return;
			}
			case psStopped:
			{
				// Nothing needed
				break;
			}
		}
		return;
	}
}

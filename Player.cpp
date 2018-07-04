#include "Player.h"
#include <assert.h>
#include <QDebug>
#include "Playlist.h"
#include "PlaybackBuffer.h"
#include "AudioEffects.h"





class Player::OutputThread:
	public QThread
{
public:

	OutputThread(Player & a_Player, QAudioFormat & a_Format);

	// QThread overrides:
	virtual void run() override;


protected slots:

	/** Starts playing the specified track.
	Connects self to Player's startingPlayback() in the class constructor. */
	void startPlaying(IPlaylistItemPtr a_Track);

	/** Emitted by the player when the AudioOutput signals that it is idle.
	Used to finalize the AudioDataSource (#118). */
	void finishedPlayback(AudioDataSourcePtr a_Src);


protected:

	friend class Player;

	/** The player that owns this thread. */
	Player & m_Player;

	/** The format that the audio output should consume.*/
	QAudioFormat m_Format;

	/** The actual audio output. */
	std::unique_ptr<QAudioOutput> m_Output;
};





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
	m_OutputThread = std::make_unique<OutputThread>(*this, m_Format);
	m_OutputThread->start(QThread::HighPriority);

	connect(m_Playlist.get(), &Playlist::itemDeleting, this, &Player::deletePlaylistItem);
}





Player::~Player()
{
	auto outputThread = std::move(m_OutputThread);
	if (outputThread != nullptr)
	{
		outputThread->quit();
		outputThread->wait();
	}
}





double Player::currentPosition() const
{
	if (m_AudioDataSource == nullptr)
	{
		return 0;
	}
	return m_AudioDataSource->currentSongPosition();
}





double Player::remainingTime() const
{
	if (m_AudioDataSource == nullptr)
	{
		return 0;
	}
	return m_AudioDataSource->remainingTime();
}





double Player::totalTime() const
{
	if (m_AudioDataSource == nullptr)
	{
		return 0;
	}
	return static_cast<double>(m_Elapsed.elapsed()) / 1000;
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





IPlaylistItemPtr Player::currentTrack()
{
	switch (m_State)
	{
		case psPlaying:
		case psFadeOutToStop:
		case psFadeOutToTrack:
		case psPaused:
		{
			return m_Playlist->currentItem();
		}
		case psStopped:
		{
			return nullptr;
		}
	}
	assert(!"Invalid state");
	return nullptr;
}





bool Player::isPlaying() const
{
	switch (m_State)
	{
		case psPlaying:
		case psFadeOutToStop:
		case psFadeOutToTrack:
		{
			return true;
		}
		case psStopped:
		case psPaused:
		{
			return false;
		}
	}
	assert(!"Invalid state");
	return false;
}





bool Player::isTrackLoaded() const
{
	switch (m_State)
	{
		case psPlaying:
		case psFadeOutToStop:
		case psFadeOutToTrack:
		case psPaused:
		{
			return true;
		}
		case psStopped:
		{
			return false;
		}
	}
	assert(!"Invalid state");
	return false;
}





void Player::setVolume(qreal a_NewVolume)
{
	m_OutputThread->m_Output->setVolume(a_NewVolume);
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
		case psPaused:
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
		case psPaused:
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





void Player::startPausePlayback()
{
	switch (m_State)
	{
		case psStopped:
		case psPaused:
		{
			startPlayback();
			return;
		}
		case psPlaying:
		{
			pausePlayback();
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





void Player::startPlayback()
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
			m_Elapsed.start();
			emit startingPlayback(track);
			return;
		}
		case psPaused:
		{
			m_OutputThread->m_Output->resume();
			return;
		}
	}
}





void Player::pausePlayback()
{
	if (m_State != psPlaying)
	{
		return;
	}
	m_OutputThread->m_Output->suspend();
}





void Player::stopPlayback()
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
		case psPaused:
		{
			startPlayback();
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





void Player::deletePlaylistItem(IPlaylistItem * a_Item)
{
	if (m_State != psPlaying)
	{
		return;
	}
	if (m_CurrentTrack.get() == a_Item)
	{
		// Fade out and start playing the next track if any (the same track index!):
		fadeOut(psFadeOutToTrack);
	}
}





void Player::outputStateChanged(QAudio::State a_NewState)
{
	switch (a_NewState)
	{
		case QAudio::StoppedState:
		case QAudio::IdleState:
		{
			// The player has become idle, which means there's no more audio to play.
			// Either the song finished, or the fadeout was completed.
			switch (m_State)
			{
				case psPlaying:
				{
					// Play the next song in the playlist, if any:
					m_State = psStopped;
					emit finishedPlayback(m_AudioDataSource);
					m_AudioDataSource.reset();
					m_CurrentTrack.reset();
					if (m_Playlist->nextItem())
					{
						startPlayback();
					}
					return;
				}
				case psFadeOutToStop:
				case psPaused:
				{
					// Stop playing completely:
					m_State = psStopped;
					emit finishedPlayback(m_AudioDataSource);
					m_AudioDataSource.reset();
					m_CurrentTrack.reset();
					return;
				}
				case psFadeOutToTrack:
				{
					// Start playing the next scheduled track:
					m_State = psStopped;
					emit finishedPlayback(m_AudioDataSource);
					m_AudioDataSource.reset();
					m_CurrentTrack.reset();
					startPlayback();
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

		case QAudio::SuspendedState:
		{
			m_State = psPaused;
			return;
		}

		case QAudio::ActiveState:
		{
			if (m_State == psPaused)
			{
				m_State = psPlaying;
			}
			return;
		}

		#if (QT_VERSION >= 0x050a00)
			case QAudio::InterruptedState:  // Added in Qt 5.10
			{
				// No processing needed
				return;
			}
		#endif
	}
}





////////////////////////////////////////////////////////////////////////////////
// Player::OutputThread:

Player::OutputThread::OutputThread(Player & a_Player, QAudioFormat & a_Format):
	m_Player(a_Player),
	m_Format(a_Format)
{
	setObjectName("Player::OutputThread");
	moveToThread(this);
	connect(&m_Player, &Player::startingPlayback, this, &Player::OutputThread::startPlaying, Qt::QueuedConnection);
	connect(&m_Player, &Player::finishedPlayback, this, &Player::OutputThread::finishedPlayback, Qt::QueuedConnection);
}





void Player::OutputThread::run()
{
	m_Output.reset(new QAudioOutput(m_Format));
	connect(m_Output.get(), &QAudioOutput::stateChanged, &m_Player, &Player::outputStateChanged, Qt::BlockingQueuedConnection);
	connect(m_Output.get(), &QAudioOutput::notify, [this]()
		{
			if (m_Player.m_State != psPlaying)
			{
				return;
			}
			auto currTrack = m_Player.currentTrack();
			if (currTrack == nullptr)
			{
				return;
			}
			auto durationLimit = currTrack->durationLimit();
			if (durationLimit < 0)
			{
				// No limit to enforce
				return;
			}
			if (m_Player.m_Elapsed.hasExpired(static_cast<qint64>(1000 * durationLimit)))
			{
				if (m_Player.playlist().hasNextSong())
				{
					m_Player.nextTrack();
				}
				else
				{
					m_Player.pausePlayback();
				}
			}
		}
	);
	exec();
}





void Player::OutputThread::startPlaying(IPlaylistItemPtr a_Track)
{
	m_Player.m_CurrentTrack = a_Track;
	m_Player.m_PlaybackBuffer.reset(a_Track->startDecoding(m_Format));
	if (m_Player.m_PlaybackBuffer == nullptr)
	{
		qDebug() << "Cannot start playback, decoder returned failure";
		// TODO: Next song?
		return;
	}
	if (!m_Player.m_PlaybackBuffer->waitForData())
	{
		qDebug() << "Cannot start playback, decoder didn't produce any initial data.";
		return;
	}
	auto audioDataSource =
		std::make_shared<AudioFadeOut>(
		std::make_shared<AudioTempoChange>(
		m_Player.m_PlaybackBuffer
	));
	audioDataSource->setTempo(m_Player.m_Tempo);
	m_Player.m_AudioDataSource = std::make_shared<AudioDataSourceIO>(audioDataSource);
	auto bufSize = m_Format.bytesForDuration(300 * 1000);  // 300 msec buffer
	qDebug() << "Setting audio output buffer size to " << bufSize;
	m_Output->setBufferSize(bufSize);
	m_Output->start(m_Player.m_AudioDataSource.get());
	m_Output->setNotifyInterval(100);
	m_Player.m_State = psPlaying;
	QMetaObject::invokeMethod(
		&m_Player, "startedPlayback",
		Q_ARG(IPlaylistItemPtr, a_Track), Q_ARG(PlaybackBufferPtr, m_Player.m_PlaybackBuffer)
	);
}





void Player::OutputThread::finishedPlayback(AudioDataSourcePtr a_Src)
{
	a_Src.reset();  // Remove the last reference to the AudioDataSource here in the output thread.
}

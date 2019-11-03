#include "Player.hpp"
#include <cassert>
#include <QDebug>
#include "../Playlist.hpp"
#include "PlaybackBuffer.hpp"
#include "AudioEffects.hpp"





class Player::OutputThread:
	public QThread
{
public:

	OutputThread(Player & aPlayer, QAudioFormat & aFormat);

	// QThread overrides:
	virtual void run() override;


protected slots:

	/** Starts playing the specified track.
	Connects self to Player's startingPlayback() in the class constructor. */
	void startPlaying(IPlaylistItemPtr aTrack);

	/** Emitted by the player when the AudioOutput signals that it is idle.
	Used to finalize the AudioDataSource (#118). */
	void finishedPlayback(AudioDataSourcePtr aSrc);


protected:

	friend class Player;

	/** The player that owns this thread. */
	Player & mPlayer;

	/** The format that the audio output should consume.*/
	QAudioFormat mFormat;

	/** The actual audio output. */
	std::unique_ptr<QAudioOutput> mOutput;
};





Player::Player(QObject * aParent):
	Super(aParent),
	mPlaylist(new Playlist),
	mState(psStopped),
	mFadeoutProgress(0),
	mTempo(1)
{
	// Pick an audio format:
	mFormat.setSampleRate(48000);
	mFormat.setChannelCount(2);
	mFormat.setSampleSize(16);
	mFormat.setSampleType(QAudioFormat::SignedInt);
	mFormat.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
	mFormat.setCodec("audio/pcm");
	if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(mFormat))
	{
		qDebug() << "Using 48 kHz output format";
	}
	else
	{
		mFormat.setSampleRate(44100);
		if (QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(mFormat))
		{
			qDebug() << "Using 44.1 kHz output format";
		}
		else
		{
			qWarning() << "No basic output format supported, attempting default";
			mFormat = QAudioDeviceInfo::defaultOutputDevice().preferredFormat();
			qDebug() << "Output audio format: " << mFormat;
		}
	}
	mOutputThread = std::make_unique<OutputThread>(*this, mFormat);
	mOutputThread->start(QThread::HighPriority);

	connect(mPlaylist.get(), &Playlist::itemDeleting,  this,             &Player::deletePlaylistItem);
	connect(this,             &Player::startedPlayback, mPlaylist.get(), &Playlist::updateItemTimesFromCurrent);
	connect(this,             &Player::stoppedPlayback, mPlaylist.get(), &Playlist::eraseItemTimesAfterCurrent);
}





Player::~Player()
{
	auto outputThread = std::move(mOutputThread);
	if (outputThread != nullptr)
	{
		outputThread->quit();
		if (!outputThread->wait(5000))
		{
			assert(!"Player output thread cannot be terminated, abandoning.");
			qWarning("Player output thread cannot be terminated, abandoning.");
		}
	}
}





double Player::currentPosition() const
{
	if (mAudioDataSource == nullptr)
	{
		return 0;
	}
	return mAudioDataSource->currentSongPosition();
}





double Player::remainingTime() const
{
	if (mAudioDataSource == nullptr)
	{
		return 0;
	}
	return mAudioDataSource->remainingTime();
}





double Player::totalTime() const
{
	if (mAudioDataSource == nullptr)
	{
		return 0;
	}
	return static_cast<double>(mElapsed.elapsed()) / 1000;
}





void Player::fadeOut(Player::State aFadeOutState)
{
	assert((aFadeOutState == psFadeOutToStop) || (aFadeOutState == psFadeOutToTrack));  // Requested a fadeout
	assert((mState != psFadeOutToStop) && (mState != psFadeOutToTrack));  // Not already fading out

	mFadeoutProgress = 0;
	mState = aFadeOutState;
	if (mAudioDataSource != nullptr)
	{
		mAudioDataSource->fadeOut(2000);  // TODO: Settable FadeOut length
	}
}





void Player::invalidCurrentTrack()
{
	assert(mCurrentTrack != nullptr);

	mCurrentTrack->markAsUnplayable();
	emit invalidTrack(mCurrentTrack);
}





void Player::seekTo(double aTime)
{
	if (mState != psPlaying)
	{
		return;
	}
	if (mAudioDataSource == nullptr)
	{
		return;
	}
	mAudioDataSource->seekTo(aTime);
	if (mCurrentTrack->updateEndTimeFromRemainingTime(mAudioDataSource->remainingTime()))
	{
		mPlaylist->updateItemTimesFromCurrent();
	}
}





IPlaylistItemPtr Player::currentTrack()
{
	switch (mState)
	{
		case psPlaying:
		case psFadeOutToStop:
		case psFadeOutToTrack:
		case psPaused:
		case psStartingPlayback:
		{
			return mPlaylist->currentItem();
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
	switch (mState)
	{
		case psPlaying:
		case psFadeOutToStop:
		case psFadeOutToTrack:
		case psStartingPlayback:
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
	switch (mState)
	{
		case psPlaying:
		case psFadeOutToStop:
		case psFadeOutToTrack:
		case psPaused:
		case psStartingPlayback:
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





void Player::setVolume(qreal aNewVolume)
{
	mOutputThread->mOutput->setVolume(aNewVolume);
	emit volumeChanged(aNewVolume);
}





void Player::setTempo(qreal aNewTempo)
{
	mTempo = aNewTempo;
	if (mAudioDataSource == nullptr)
	{
		emit tempoCoeffChanged(aNewTempo);
		return;
	}
	mAudioDataSource->setTempo(aNewTempo);
	assert(mCurrentTrack != nullptr);
	mCurrentTrack->setTempoCoeff(aNewTempo);
	if (mCurrentTrack->updateEndTimeFromRemainingTime(mAudioDataSource->remainingTime()))
	{
		mPlaylist->updateItemTimesFromCurrent();
	}
	emit tempoCoeffChanged(aNewTempo);
}





void Player::nextTrack()
{
	if (mState == psFadeOutToTrack)
	{
		// Already fading out to a track, abort the fade-out:
		auto ads = mAudioDataSource;
		if (ads != nullptr)
		{
			ads->fadeOut(1);
		}
		return;
	}

	if (!mPlaylist->nextItem())
	{
		// There's no next track in the playlist
		return;
	}
	switch (mState)
	{
		case psStopped:
		case psPaused:
		{
			return;
		}
		case psPlaying:
		case psStartingPlayback:
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
			mState = psFadeOutToTrack;
			break;
		}
	}
}





void Player::prevTrack()
{
	if (mState == psFadeOutToTrack)
	{
		// Already fading out to a track, abort the fade-out:
		auto ads = mAudioDataSource;
		if (ads != nullptr)
		{
			ads->fadeOut(1);
		}
		return;
	}

	if (!mPlaylist->prevItem())
	{
		// There's no prev track in the playlist
		return;
	}
	switch (mState)
	{
		case psStopped:
		case psPaused:
		{
			return;
		}
		case psPlaying:
		case psStartingPlayback:
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
			mState = psFadeOutToTrack;
			break;
		}
	}
}





void Player::startPausePlayback()
{
	switch (mState)
	{
		case psStopped:
		case psPaused:
		{
			startPlayback();
			return;
		}
		case psPlaying:
		case psStartingPlayback:
		{
			pausePlayback();
			return;
		}
		case psFadeOutToTrack:
		{
			// Already fading out, just stop afterwards
			mState = psFadeOutToStop;
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
	auto track = mPlaylist->currentItem();
	if (track == nullptr)
	{
		return;
	}
	switch (mState)
	{
		case psFadeOutToStop:
		{
			// We were stopping a track; continue stopping, but schedule the new track to start playing afterwards
			mState = psFadeOutToTrack;
			return;
		}
		case psFadeOutToTrack:
		{
			// We're already scheduled to start playing the new track, ignore the request
			return;
		}
		case psPlaying:
		case psStartingPlayback:
		{
			// We're already playing, ignore the request
			return;
		}
		case psStopped:
		{
			// We're stopped, start the playback:
			qDebug() << "Player: Starting playback of track " << track->displayName();
			mElapsed.start();
			emit startingPlayback(track);
			mState = psStartingPlayback;
			return;
		}
		case psPaused:
		{
			mOutputThread->mOutput->resume();
			return;
		}
	}
}





void Player::pausePlayback()
{
	if ((mState != psPlaying) && (mState != psStartingPlayback))
	{
		return;
	}
	mOutputThread->mOutput->suspend();
}





void Player::stopPlayback()
{
	switch (mState)
	{
		case psPlaying:
		case psStartingPlayback:
		{
			fadeOut(psFadeOutToStop);
			break;
		}
		case psFadeOutToStop:
		case psFadeOutToTrack:
		{
			// Stop completely without finishing the fadeout
			qDebug() << "Stopping playback immediately";
			auto ads = mAudioDataSource;
			if (ads != nullptr)
			{
				ads->fadeOut(1);
			}
			break;
		}
		case psStopped:
		case psPaused:
		{
			// Do nothing, already stopped
			break;
		}
	}
}





void Player::startNextTrack()
{
	if (mPlaylist->nextItem())
	{
		startPlayback();
	}
}





void Player::jumpTo(int aItemIdx)
{
	if (!mPlaylist->setCurrentItem(aItemIdx))
	{
		// Invalid index
		return;
	}
	switch (mState)
	{
		case psPlaying:
		case psStartingPlayback:
		{
			fadeOut(psFadeOutToTrack);
			break;
		}
		case psPaused:
		{
			mState = psStopped;
			startPlayback();
			break;
		}
		case psStopped:
		{
			startPlayback();
			break;
		}
		case psFadeOutToStop:
		{
			mState = psFadeOutToTrack;
			break;
		}
		case psFadeOutToTrack:
		{
			// Nothing needed
			break;
		}
	}
}







void Player::setKeepTempo(bool aKeepTempo)
{
	mShouldKeepTempo = aKeepTempo;
}





void Player::setKeepVolume(bool aKeepVolume)
{
	mShouldKeepVolume = aKeepVolume;
}





void Player::deletePlaylistItem(IPlaylistItem * aItem)
{
	if ((mState != psPlaying) && (mState != psStartingPlayback))
	{
		return;
	}
	if (mCurrentTrack.get() == aItem)
	{
		// Fade out and start playing the next track if any (the same track index!):
		fadeOut(psFadeOutToTrack);
	}
}





void Player::updateTrackTimesFromCurrent()
{
	if ((mCurrentTrack == nullptr) || (mAudioDataSource == nullptr))
	{
		return;
	}
	if (mCurrentTrack->updateEndTimeFromRemainingTime(mAudioDataSource->remainingTime()))
	mPlaylist->updateItemTimesFromCurrent();
}





void Player::outputStateChanged(QAudio::State aNewState)
{
	switch (aNewState)
	{
		case QAudio::StoppedState:
		case QAudio::IdleState:
		{
			// The player has become idle, which means there's no more audio to play.
			// Either the song finished, or the fadeout was completed.
			if (mCurrentTrack != nullptr)
			{
				mCurrentTrack->mPlaybackEnded = QDateTime::currentDateTimeUtc();
			}
			switch (mState)
			{
				case psPlaying:
				{
					// Play the next song in the playlist, if any:
					mState = psStopped;
					emit finishedPlayback(mAudioDataSource);
					mAudioDataSource.reset();
					mCurrentTrack.reset();
					if (mPlaylist->nextItem())
					{
						startPlayback();
					}
					else
					{
						emit stoppedPlayback();
					}
					return;
				}
				case psFadeOutToStop:
				case psPaused:
				{
					// Stop playing completely:
					mState = psStopped;
					emit finishedPlayback(mAudioDataSource);
					mAudioDataSource.reset();
					mCurrentTrack.reset();
					emit stoppedPlayback();
					return;
				}
				case psFadeOutToTrack:
				{
					// Start playing the next scheduled track:
					mState = psStopped;
					emit finishedPlayback(mAudioDataSource);
					mAudioDataSource.reset();
					mCurrentTrack.reset();
					startPlayback();
					return;
				}
				case psStopped:
				case psStartingPlayback:
				{
					// Nothing needed
					break;
				}
			}
			return;
		}

		case QAudio::SuspendedState:
		{
			mState = psPaused;
			return;
		}

		case QAudio::ActiveState:
		{
			if (mState == psPaused)
			{
				mState = psPlaying;
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

Player::OutputThread::OutputThread(Player & aPlayer, QAudioFormat & aFormat):
	mPlayer(aPlayer),
	mFormat(aFormat)
{
	setObjectName("Player::OutputThread");
	moveToThread(this);
	connect(&mPlayer, &Player::startingPlayback, this, &Player::OutputThread::startPlaying, Qt::QueuedConnection);
	connect(&mPlayer, &Player::finishedPlayback, this, &Player::OutputThread::finishedPlayback, Qt::QueuedConnection);
}





void Player::OutputThread::run()
{
	mOutput.reset(new QAudioOutput(mFormat));
	connect(mOutput.get(), &QAudioOutput::stateChanged, &mPlayer, &Player::outputStateChanged, Qt::BlockingQueuedConnection);
	connect(mOutput.get(), &QAudioOutput::notify, [this]()
		{
			if ((mPlayer.mState != psPlaying) && (mPlayer.mState != psStartingPlayback))
			{
				return;
			}
			auto currTrack = mPlayer.currentTrack();
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
			if (mPlayer.mElapsed.hasExpired(static_cast<qint64>(1000 * durationLimit)))
			{
				if (mPlayer.playlist().hasNextSong())
				{
					mPlayer.nextTrack();
				}
				else
				{
					mPlayer.stopPlayback();
				}
			}
		}
	);
	exec();
}





void Player::OutputThread::startPlaying(IPlaylistItemPtr aTrack)
{
	mPlayer.mCurrentTrack = aTrack;
	mPlayer.mPlaybackBuffer.reset(aTrack->startDecoding(mFormat));
	if (mPlayer.mPlaybackBuffer == nullptr)
	{
		qDebug() << "Cannot start playback, decoder returned failure";
		mPlayer.mState = psStopped;
		QMetaObject::invokeMethod(&mPlayer, "invalidCurrentTrack", Qt::BlockingQueuedConnection);
		QMetaObject::invokeMethod(&mPlayer, "startNextTrack", Qt::BlockingQueuedConnection);
		return;
	}
	if (!mPlayer.mPlaybackBuffer->waitForData())
	{
		qDebug() << "Cannot start playback, decoder didn't produce any initial data.";
		mPlayer.mState = psStopped;
		QMetaObject::invokeMethod(&mPlayer, "invalidCurrentTrack", Qt::BlockingQueuedConnection);
		QMetaObject::invokeMethod(&mPlayer, "startNextTrack", Qt::BlockingQueuedConnection);
		return;
	}
	auto audioDataSource =
		std::make_shared<AudioFadeOut>(
		std::make_shared<AudioTempoChange>(
		mPlayer.mPlaybackBuffer
	));
	if (mPlayer.mShouldKeepTempo.load())
	{
		audioDataSource->setTempo(mPlayer.mTempo);
	}
	else
	{
		auto tempoCoeff = aTrack->tempoCoeff();
		audioDataSource->setTempo(tempoCoeff);
		QMetaObject::invokeMethod(
			&mPlayer, "tempoCoeffChanged",
			Q_ARG(qreal, tempoCoeff)
		);
	}
	mPlayer.mAudioDataSource = std::make_shared<AudioDataSourceIO>(audioDataSource);
	auto bufSize = mFormat.bytesForDuration(300 * 1000);  // 300 msec buffer
	qDebug() << "Setting audio output buffer size to " << bufSize;
	mOutput->setBufferSize(bufSize);
	mOutput->start(mPlayer.mAudioDataSource.get());
	mOutput->setNotifyInterval(100);
	mPlayer.mState = psPlaying;
	aTrack->mPlaybackStarted = QDateTime::currentDateTimeUtc();
	QMetaObject::invokeMethod(&mPlayer, "updateTrackTimesFromCurrent");
	QMetaObject::invokeMethod(
		&mPlayer, "startedPlayback",
		Q_ARG(IPlaylistItemPtr, aTrack), Q_ARG(PlaybackBufferPtr, mPlayer.mPlaybackBuffer)
	);
}





void Player::OutputThread::finishedPlayback(AudioDataSourcePtr aSrc)
{
	aSrc.reset();  // Remove the last reference to the AudioDataSource here in the output thread.
}

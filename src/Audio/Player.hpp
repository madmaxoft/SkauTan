#pragma once

#include <memory>

#include <QAudioOutput>
#include <QThread>
#include <QElapsedTimer>

#include "../IPlaylistItem.hpp"
#include "../ComponentCollection.hpp"
#include "PlaybackBuffer.hpp"





// fwd:
class Playlist;
class AudioDataSourceIO;





class Player:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckPlayer>
{
	using Super = QObject;
	Q_OBJECT


public:

	explicit Player(QObject * aParent = nullptr);

	~Player();

	/** Returns the playlist that this player follows. */
	Playlist & playlist() { return *mPlaylist; }

	/** Returns the current playback position in the current track, in seconds.
	Returns 0 if not playing back anything. */
	double currentPosition() const;

	/** Returns the number of seconds remaining of playback for the current item, using its current settings.
	Returns 0 if not playing back anything.
	Takes playback tempo into account. */
	double remainingTime() const;

	/** Returns the total number of seconds that has elapsed since the current item started playing.
	Returns 0 if not playing back anything. */
	double totalTime() const;

	/** Sets the volume for playback. */
	void setVolume(qreal aNewVolume);

	/** Sets the tempo for playback. */
	void setTempo(qreal aNewTempo);

	/** Seeks to the specified timestamp.
	Ignored if not playing. */
	void seekTo(double aTime);

	/** Returns the track that is currently being played.
	Returns nullptr if no track. */
	IPlaylistItemPtr currentTrack();

	/** Returns the buffer for the audio data decoded from the source track, before effect processing.
	Mainly used for visualisation. */
	PlaybackBufferPtr playbackBuffer() const { return mPlaybackBuffer; }

	/** Returns true iff the player is currently playing back a track (or psStartingPlayback).
	If the player is paused, returns false. */
	bool isPlaying() const;

	/** Returns true iff the player currently has a track loaded (is playing it, or is paused while playing it) */
	bool isTrackLoaded() const;


protected:

	/** The internal state of the player. */
	enum State
	{
		psStopped,           ///< The player is not playing anything
		psPlaying,           ///< The player is playing a track
		psFadeOutToTrack,    ///< The player is fading out a track and will start playing mPlaylist's CurrentTrack once done.
		psFadeOutToStop,     ///< The player is fading out a track and will stop playing once done.
		psPaused,            ///< Playback has started and then been paused, can continue
		psStartingPlayback,  ///< The player has been asked to play a song, but it's not playing yet (preparing the song in a background thread)
	};


	// fwd:
	class OutputThread;


	/** The playlist on which this instance is operating. */
	std::shared_ptr<Playlist> mPlaylist;

	/** The current internal state of the player. */
	State mState;

	/** The progress of the current fadeout.
	0 = fadeout just started. */
	int mFadeoutProgress;

	/** The thread that does the audio output processing. */
	std::unique_ptr<OutputThread> mOutputThread;

	/** The audio data decoded from the source file, before effect processing.
	Used mainly for visualisation. */
	std::shared_ptr<PlaybackBuffer> mPlaybackBuffer;

	/** The source of the audio data, adapted into QIODevice interface. */
	std::shared_ptr<AudioDataSourceIO> mAudioDataSource;

	/** The format used by the audio output. */
	QAudioFormat mFormat;

	/** The tempo to be used for playback. */
	qreal mTempo;

	/** Measures the time since the playback for the current item was started. */
	QElapsedTimer mElapsed;

	/** The track that is currently playing. */
	IPlaylistItemPtr mCurrentTrack;

	/** If true, the tempo in mTempo is set for all tracks played.
	If false, the tempo is reset to each track's default tempo on playback start. */
	std::atomic<bool> mShouldKeepTempo;

	/** If true, the tempo in mVolume is set for all tracks played.
	If false, the volume is reset to each track's default volume on playback start. */
	std::atomic<bool> mShouldKeepVolume;


	/** Starts a fadeout, sets the state to the specified fadeout state.
	Must not be fading out already. */
	void fadeOut(State aFadeOutState);

	/** Invoked by the internal playback thread if the current track cannot start playing due to invalid data.
	Emits the invalidTrack() signal. */
	Q_INVOKABLE void invalidCurrentTrack();


signals:

	/** Emitted before starting to play the specified item.
	The item is not yet set in the player, so values such as currentPosition() still give old values. */
	void startingPlayback(IPlaylistItemPtr aItem);

	/** Emitted just after starting to play the specified item.
	The item is already set in the player, values such as currentPosition() give valid values. */
	void startedPlayback(IPlaylistItemPtr aItem, PlaybackBufferPtr aPlaybackBuffer);

	/** Emitted just after an item has finished playing.
	aSource is the AudioDataSource chain that was playing. Used for proper termination (#118). */
	void finishedPlayback(AudioDataSourcePtr aSource);

	/** Emitted after playback has completely stopped, either running out of playlist, or after psFadeOutToStop.
	Note that this is different from finishedPlayback(), which is called after each track finishes playing. */
	void stoppedPlayback();

	/** Emitted when unable to start playing the specified track.
	The receiver should mark the track as invalid in the UI. */
	void invalidTrack(IPlaylistItemPtr aItem);

	/** Emitted when the current tempo is changed from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off. */
	void tempoCoeffChanged(qreal aTempoCoeff);

	/** Emitted when the current volume is changed from within the player,
	such as loading a new track with pre-set default volume and KeepVolume turned off. */
	void volumeChanged(qreal aNewVolume);


public slots:

	/** Starts playing the next track in the playlist, if available.
	Fades the current track out first.
	Ignored if there's no next track.
	Ignored if the player is not playing anything. */
	void nextTrack();

	/** Starts playing the previous track in the playlist, if available.
	Fades the current track out first.
	Ignored if there's no previous track.
	Ignored if the player is not playing anything. */
	void prevTrack();

	/** Starts playing the current track (in mPlaylist), if currently stopped / paused.
	Pause playing (with no fadeout) if the player is already playing something. */
	void startPausePlayback();

	/** Starts playing the current track (in mPlaylist), if currently stopped / paused.
	Ignored if the player is already playing something. */
	void startPlayback();

	/** Pauses the current playback.
	Doesn't fade out, pauses immediately.
	Ignored is the player is not playing anything. */
	void pausePlayback();

	/** Stops the current playback.
	Fades the current track out first.
	Ignored is the player is not playing anything. */
	void stopPlayback();

	/** If a next track is available, starts playing it; otherwise does nothing.
	Used mainly when encountering an invalid track, to resume playing the next song conditionally. */
	void startNextTrack();

	/** Starts playing back the playlist item at the specified index.
	Fades the current track first, if playing.
	Ignored if the index is invalid. */
	void jumpTo(int aItemIdx);

	/** If the specified track is currently playing, stops playback. */
	void deletePlaylistItem(IPlaylistItem * aTrack);

	/** Updates the current playlist item's PlaybackEnded time based on the remaining time,
	and all subsequent tracks' PlaybackStarted and PlaybackEnded times.
	Ignored if not playing anything. */
	void updateTrackTimesFromCurrent();

	/** Sets the KeepTempo feature on or off. */
	void setKeepTempo(bool aKeepTempo);

	/** Sets the KeepVolume feature on or off. */
	void setKeepVolume(bool aKeepVolume);


protected slots:

	/** Emitted by mOutput when its state changes.
	Used to detect end-of-track and end-of-fade. */
	void outputStateChanged(QAudio::State aNewState);
};

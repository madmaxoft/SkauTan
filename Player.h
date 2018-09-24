#ifndef PLAYER_H
#define PLAYER_H





#include <memory>

#include <QAudioOutput>
#include <QThread>
#include <QElapsedTimer>

#include "IPlaylistItem.h"
#include "PlaybackBuffer.h"
#include "ComponentCollection.h"





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

	explicit Player(QObject * a_Parent = nullptr);

	~Player();

	/** Returns the playlist that this player follows. */
	Playlist & playlist() { return *m_Playlist; }

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
	void setVolume(qreal a_NewVolume);

	/** Sets the tempo for playback. */
	void setTempo(qreal a_NewTempo);

	/** Seeks to the specified timestamp.
	Ignored if not playing. */
	void seekTo(double a_Time);

	/** Returns the track that is currently being played.
	Returns nullptr if no track. */
	IPlaylistItemPtr currentTrack();

	/** Returns the buffer for the audio data decoded from the source track, before effect processing.
	Mainly used for visualisation. */
	PlaybackBufferPtr playbackBuffer() const { return m_PlaybackBuffer; }

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
		psFadeOutToTrack,    ///< The player is fading out a track and will start playing m_Playlist's CurrentTrack once done.
		psFadeOutToStop,     ///< The player is fading out a track and will stop playing once done.
		psPaused,            ///< Playback has started and then been paused, can continue
		psStartingPlayback,  ///< The player has been asked to play a song, but it's not playing yet (preparing the song in a background thread)
	};


	// fwd:
	class OutputThread;


	/** The playlist on which this instance is operating. */
	std::shared_ptr<Playlist> m_Playlist;

	/** The current internal state of the player. */
	State m_State;

	/** The progress of the current fadeout.
	0 = fadeout just started. */
	int m_FadeoutProgress;

	/** The thread that does the audio output processing. */
	std::unique_ptr<OutputThread> m_OutputThread;

	/** The audio data decoded from the source file, before effect processing.
	Used mainly for visualisation. */
	std::shared_ptr<PlaybackBuffer> m_PlaybackBuffer;

	/** The source of the audio data, adapted into QIODevice interface. */
	std::shared_ptr<AudioDataSourceIO> m_AudioDataSource;

	/** The format used by the audio output. */
	QAudioFormat m_Format;

	/** The tempo to be used for playback. */
	qreal m_Tempo;

	/** Measures the time since the playback for the current item was started. */
	QElapsedTimer m_Elapsed;

	/** The track that is currently playing. */
	IPlaylistItemPtr m_CurrentTrack;

	/** If true, the tempo in m_Tempo is set for all tracks played.
	If false, the tempo is reset to each track's default tempo on playback start. */
	std::atomic<bool> m_ShouldKeepTempo;

	/** If true, the tempo in m_Volume is set for all tracks played.
	If false, the volume is reset to each track's default volume on playback start. */
	std::atomic<bool> m_ShouldKeepVolume;


	/** Starts a fadeout, sets the state to the specified fadeout state.
	Must not be fading out already. */
	void fadeOut(State a_FadeOutState);


signals:

	/** Emitted before starting to play the specified item.
	The item is not yet set in the player, so values such as currentPosition() still give old values. */
	void startingPlayback(IPlaylistItemPtr a_Item);

	/** Emitted just after starting to play the specified item.
	The item is already set in the player, values such as currentPosition() give valid values. */
	void startedPlayback(IPlaylistItemPtr a_Item, PlaybackBufferPtr a_PlaybackBuffer);

	/** Emitted just after an item has finished playing.
	a_Source is the AudioDataSource chain that was playing. Used for proper termination (#118). */
	void finishedPlayback(AudioDataSourcePtr a_Source);

	/** Emitted after playback has completely stopped, either running out of playlist, or after psFadeOutToStop.
	Note that this is different from finishedPlayback(), which is called after each track finishes playing. */
	void stoppedPlayback();

	/** Emitted when the current tempo is changed from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off. */
	void tempoCoeffChanged(qreal a_TempoCoeff);


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

	/** Starts playing the current track (in m_Playlist), if currently stopped / paused.
	Pause playing (with no fadeout) if the player is already playing something. */
	void startPausePlayback();

	/** Starts playing the current track (in m_Playlist), if currently stopped / paused.
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

	/** Starts playing back the playlist item at the specified index.
	Fades the current track first, if playing.
	Ignored if the index is invalid. */
	void jumpTo(int a_ItemIdx);

	/** If the specified track is currently playing, stops playback. */
	void deletePlaylistItem(IPlaylistItem * a_Track);

	/** Updates the current playlist item's PlaybackEnded time based on the remaining time,
	and all subsequent tracks' PlaybackStarted and PlaybackEnded times.
	Ignored if not playing anything. */
	void updateTrackTimesFromCurrent();

	/** Sets the KeepTempo feature on or off. */
	void setKeepTempo(bool a_KeepTempo);

	/** Sets the KeepVolume feature on or off. */
	void setKeepVolume(bool a_KeepVolume);


protected slots:

	/** Emitted by m_Output when its state changes.
	Used to detect end-of-track and end-of-fade. */
	void outputStateChanged(QAudio::State a_NewState);
};





#endif // PLAYER_H

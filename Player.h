#ifndef PLAYER_H
#define PLAYER_H





#include <memory>
#include <QAudioOutput>
#include <QThread>





// fwd:
class PlaybackBuffer;
class Playlist;





class Player:
	public QObject
{
	using Super = QObject;
	Q_OBJECT


public:

	explicit Player(std::shared_ptr<Playlist> a_Playlist, QObject * a_Parent = nullptr);

	~Player();

	/** Returns the current playback position in the current track, in seconds. */
	double currentPosition() const { return m_CurrentPosition; }

protected:

	/** The internal state of the player. */
	enum State
	{
		psStopped,         ///< The player is not playing anything
		psPlaying,         ///< The player is playing a track
		psFadeOutToTrack,  ///< The player is fading out a track and will start playing m_Playlist's CurrentTrack once done.
		psFadeOutToStop,   ///< The player is fading out a track and will stop playing once done.
	};

	/** The playlist on which this instance is operating. */
	std::shared_ptr<Playlist> m_Playlist;

	/** The current internal state of the player. */
	State m_State;

	/** The progress of the current fadeout.
	0 = fadeout just started. */
	int m_FadeoutProgress;

	/** The current playback position in the current track, in seconds.
	Only valid when m_State is psPlaying. */
	double m_CurrentPosition;

	/** The actual audio output. */
	std::unique_ptr<QAudioOutput> m_Output;

	/** The source of the audio data (from the decoder). */
	std::unique_ptr<PlaybackBuffer> m_OutputIO;

	/** The format used by the audio output. */
	QAudioFormat m_Format;

	/** The thread in which the audio output is run. */
	QThread m_Thread;

	/** Starts a fadeout, sets the state to the specified fadeout state.
	Must not be fading out already. */
	void fadeOut(State a_FadeOutState);

signals:


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

	/** Starts playing the current track (in m_Playlist), if currently stopped.
	Stops playing if the player is already playing something. */
	void startPause();

	/** Starts playing the current track (in m_Playlist), if currently stopped.
	Ignored if the player is already playing something. */
	void start();

	/** Pauses the current playback.
	Fades the current track out first.
	Ignored is the player is not playing anything. */
	void pause();


protected slots:

	/** Emitted by m_Output when its state changes.
	Used to detect end-of-track and end-of-fade. */
	void outputStateChanged(QAudio::State a_NewState);
};





#endif // PLAYER_H

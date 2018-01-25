#ifndef PLAYER_H
#define PLAYER_H





#include <memory>
#include <QObject>
#include <QAudioOutput>





// fwd:
class Playlist;
class PlaybackBuffer;





/** Feeds the audio data into the audio output device.
Takes care of playlist advancement and fadeout when changing songs manually.
For fadeout, the next item to play is stored in m_Playlist's current song.
For regular advancement, the m_Playlist's current song is advanced upon reaching the end of the song. */
class Player:
	public QObject
{
	using Super = QObject;
	Q_OBJECT


public:
	enum State
	{
		psStopped,
		psPlaying,
		psFadeOut,  // The currently played song is being faded out, new song about to play
	};


	/** Creates a new instance, bound to the specified playlist. */
	explicit Player(
		std::shared_ptr<Playlist> a_Playlist,
		QObject * a_Parent = nullptr
	);


protected:

	/** The current state of the player. */
	State m_State;

	/** The actual audio output. */
	std::unique_ptr<QAudioOutput> m_Output;

	/** The playlist being played.
	Within the playlist, the song currently played (psPlaying) or to be played next (psFadeOut). */
	std::shared_ptr<Playlist> m_Playlist;

	/** The currently played audio data. */
	std::shared_ptr<PlaybackBuffer> m_CurrentBuffer;


	/** Starts playing the song marked in m_Playlist as current. */
	void startPlaying();

signals:

public slots:

	void startStop();
	void prevItem();
	void nextItem();

protected slots:

	/** Emitted by m_Output when its state changes. */
	void audioOutputStateChanged(QAudio::State a_NewState);

	/** Emitted by m_CurrentBuffer when audio data becomes available. */
	void bufferDataReady();
};





#endif // PLAYER_H

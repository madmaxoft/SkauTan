#ifndef WAVEFORMDISPLAY_H
#define WAVEFORMDISPLAY_H





#include <QWidget>
#include <QTimer>
#include <QSize>

#include "../Audio/PlaybackBuffer.hpp"
#include "../IPlaylistItem.hpp"





// fwd:
class Player;
class PlayerWidget;
class QPainter;
class Song;
using SongPtr = std::shared_ptr<Song>;





/** Takes care of calculating and drawing a display of a Player's waveform. */
class WaveformDisplay:
	public QWidget
{
	using Super = QWidget;

	Q_OBJECT


public:

	explicit WaveformDisplay(QWidget * a_Parent = nullptr);

	/** Sets the player whose waveform to display. */
	void setPlayer(Player & a_Player);

	/** Paints the display for the specified paint event. */
	void paint(QPainter & a_Painter, const QPoint & a_Origin, int a_Height);

	/** Sets the new width and height for the display.
	Recalculates the peaks. */
	void resize(int a_Width);


protected:

	/** The player whose waveform to display. */
	Player * m_Player;

	/** The playback buffer that is being played by the player currently.
	This object provides the waveform data.
	May be nullptr. */
	PlaybackBufferPtr m_PlaybackBuffer;

	/** The song currently displayed.
	If the current track is not a song, this member is set to nullptr. */
	SongPtr m_CurrentSong;

	/** The calculated waveform peaks. */
	std::vector<short> m_Peaks;

	std::vector<int> m_Sums;

	/** The timer used to provide updates while song decoding is in progress. */
	QTimer m_UpdateTimer;

	/** The width of the display, in pixels. Also the count of m_Peaks. */
	int m_Width;

	/** True if the peak data has been rendered for the complete playback data (entire song). */
	bool m_IsPeakDataComplete;


	/** Calculates the peaks for the current widget size and current PlaybackBuffer fill mark. */
	void calculatePeaks();

	/** Sets the song's skip-start to the time corresponding to the specified X position.
	If the current track is not a song, ignored. */
	void setSkipStart(int a_PosX);

	/** Removes the song's skip-start.
	If the current track is not a song, ignored. */
	void delSkipStart();


	// QWidget overrides:
	virtual void paintEvent(QPaintEvent * a_Event) override;
	virtual void resizeEvent(QResizeEvent * a_Event) override;
	virtual QSize sizeHint() const override;
	virtual void mouseReleaseEvent(QMouseEvent * a_Event) override;
	virtual void contextMenuEvent(QContextMenuEvent * a_Event) override;


signals:

	/** Emitted after a song is changed as a result of user's operation within this widget. */
	void songChanged(SongPtr a_Song);


public slots:


protected slots:

	/** Emitted by the player when it starts playing back an item. */
	void playerStartedPlayback(IPlaylistItemPtr a_Item, PlaybackBufferPtr a_PlaybackBuffer);

	/** Emitted by the player when it finishes playing back an item. */
	void playerFinishedPlayback();

	/** Emitted by m_UpdateTimer to update periodically. */
	void updateOnTimer();

};





#endif // WAVEFORMDISPLAY_H

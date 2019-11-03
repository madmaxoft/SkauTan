#pragma once

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

	explicit WaveformDisplay(QWidget * aParent = nullptr);

	/** Sets the player whose waveform to display. */
	void setPlayer(Player & aPlayer);

	/** Paints the display for the specified paint event. */
	void paint(QPainter & aPainter, int aHeight);

	/** Sets the new width and height for the display.
	Recalculates the peaks. */
	void resize(int aWidth);


protected:

	/** The player whose waveform to display. */
	Player * mPlayer;

	/** The playback buffer that is being played by the player currently.
	This object provides the waveform data.
	May be nullptr. */
	PlaybackBufferPtr mPlaybackBuffer;

	/** The song currently displayed.
	If the current track is not a song, this member is set to nullptr. */
	SongPtr mCurrentSong;

	/** The calculated waveform peaks. */
	std::vector<short> mPeaks;

	std::vector<int> mSums;

	/** The timer used to provide updates while song decoding is in progress. */
	QTimer mUpdateTimer;

	/** The width of the display, in pixels. Also the count of mPeaks. */
	int mWidth;

	/** True if the peak data has been rendered for the complete playback data (entire song). */
	bool mIsPeakDataComplete;


	/** Calculates the peaks for the current widget size and current PlaybackBuffer fill mark. */
	void calculatePeaks();

	/** Sets the song's skip-start to the time corresponding to the specified X position.
	If the current track is not a song, ignored. */
	void setSkipStart(int aPosX);

	/** Removes the song's skip-start.
	If the current track is not a song, ignored. */
	void delSkipStart();


	// QWidget overrides:
	virtual void paintEvent(QPaintEvent * aEvent) override;
	virtual void resizeEvent(QResizeEvent * aEvent) override;
	virtual QSize sizeHint() const override;
	virtual void mouseReleaseEvent(QMouseEvent * aEvent) override;
	virtual void contextMenuEvent(QContextMenuEvent * aEvent) override;

	/** Converts the time within the song into a screen X coord. */
	int timeToScreen(double aSeconds);

	/** Converts the screen X coord into timestamp within the song.
	Clamps the X coord into the valid range, if needed. */
	double screenToTime(int aPosX);


signals:

	/** Emitted after a song is changed as a result of user's operation within this widget. */
	void songChanged(SongPtr aSong);


public slots:


protected slots:

	/** Emitted by the player when it starts playing back an item. */
	void playerStartedPlayback(IPlaylistItemPtr aItem, PlaybackBufferPtr aPlaybackBuffer);

	/** Emitted by the player when it finishes playing back an item. */
	void playerFinishedPlayback();

	/** Emitted by mUpdateTimer to update periodically. */
	void updateOnTimer();

};

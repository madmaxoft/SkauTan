#ifndef CLASSROOMWINDOW_HPP
#define CLASSROOMWINDOW_HPP

#include <memory>
#include <QMainWindow>
#include <QTimer>





// fwd:
class QListWidgetItem;
class ComponentCollection;
class Filter;
class Song;
namespace Ui
{
	class ClassroomWindow;
}





class ClassroomWindow:
	public QMainWindow
{
	Q_OBJECT

public:

	explicit ClassroomWindow(ComponentCollection & a_Components);

	~ClassroomWindow();

	/** Sets the window to use for switching to the Playlist mode. */
	void setPlaylistWindow(QWidget & a_PlaylistWindow);


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::ClassroomWindow> m_UI;

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The timer used for periodic UI updates. */
	QTimer m_UpdateTimer;

	/** The window that will be shown when asked to switch to Playlist mode. */
	QWidget * m_PlaylistWindow;

	/** Set to true while updating the UI as a reaction to an internal event.
	If true, the changes in the UI shouldn't be propagated further. */
	bool m_IsInternalChange;


	/** Updates the list of filters in lwFilters. */
	void updateFilterList();

	/** Returns the filter currently selected in lwFilters, or nullptr if none. */
	std::shared_ptr<Filter> selectedFilter();

	/** Adds the specified song to the playlist and starts playing it.
	If another song is currently playing, fade-out is used for it. */
	void startPlayingSong(std::shared_ptr<Song> a_Song);

private slots:

	/** Switches to Playlist mode, if available, and hides this window. */
	void switchToPlaylistMode();

	/** Updates the list of song matching the currently selected filter. */
	void updateSongList();

	/** The user has dbl-clicked on the specified item.
	Starts playing the song, applying a fadeout if already playing another before. */
	void songItemDoubleClicked(QListWidgetItem * a_Item);

	/** Called periodically to update the UI. */
	void periodicUIUpdate();

	/** Emitted by the global volume control slider; updates the player volume. */
	void volumeSliderMoved(int a_NewValue);

	/** Emitted by the global tempo slider; updates the playback tempo. */
	void tempoValueChanged(int a_NewValue);

	/** Emitted by Player after changing the volume from within the player,
	such as loading a new track with pre-set default volume and KeepVolume turned off.
	Updates the Volume UI. */
	void playerVolumeChanged(qreal a_Volume);

	/** Emitted by Player after changing the tempo from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off.
	Updates the Tempo UI. */
	void playerTempoChanged(qreal a_TempoCoeff);
};





#endif // CLASSROOMWINDOW_HPP

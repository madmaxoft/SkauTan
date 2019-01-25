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

	/** If positive, the number of m_UpdateTimer ticks until the current Duration Limit settings are applied.
	Used by the leDurationLimit to apply the settings only after some time after the last edit,
	to avoid applying a limit parsed from number in the middle of editing. */
	int m_TicksToDurationLimitApply;

	/** Updates the list of filters in lwFilters. */
	void updateFilterList();

	/** Returns the filter currently selected in lwFilters, or nullptr if none. */
	std::shared_ptr<Filter> selectedFilter();

	/** Adds the specified song to the playlist and starts playing it.
	If another song is currently playing, fade-out is used for it. */
	void startPlayingSong(std::shared_ptr<Song> a_Song);

	/** Applies the settings in the UI related to Duration Limit to the currently playing track. */
	void applyDurationLimitSettings();

	/** Updates the list item by the current data from its linked Song. */
	void updateSongItem(QListWidgetItem & a_Item);

	/** Sets the background color of the selected song. */
	void setSelectedSongBgColor(QColor a_BgColor);

	void rateSelectedSongs(double a_LocalRating);


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

	/** The user clicked the "reset tempo" button. */
	void tempoResetClicked();

	/** Emitted by Player after changing the volume from within the player,
	such as loading a new track with pre-set default volume and KeepVolume turned off.
	Updates the Volume UI. */
	void playerVolumeChanged(qreal a_Volume);

	/** Emitted by Player after changing the tempo from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off.
	Updates the Tempo UI. */
	void playerTempoChanged(qreal a_TempoCoeff);

	/** The Duration Limit checkbox was clicked, applies or removes the duration limit. */
	void durationLimitClicked();

	/** The Duration Limit lineedit was edited, applies the duration limit.
	If the string is invalid, colors the text red and refuses to apply the limit. */
	void durationLimitEdited(const QString & a_NewText);

	/** Plays the song that is currently selected.
	If another song is already playing, applies a fade-out to it first. */
	void playSelectedSong();

	/** Removes from library the selected song (all its instances via SharedData). */
	void removeFromLibrary();

	/** Deletes from the disk the selected song's files (all their instances via SharedData). */
	void deleteFromDisk();

	/** Shows the Properties dialog for the selected song. */
	void showSongProperties();

	/** Shows the context menu for lwSongs items at the specified position. */
	void showSongContextMenu(const QPoint & a_Pos);
};





#endif // CLASSROOMWINDOW_HPP

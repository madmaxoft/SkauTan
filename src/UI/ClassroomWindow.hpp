#ifndef CLASSROOMWINDOW_HPP
#define CLASSROOMWINDOW_HPP

#include <memory>
#include <QMainWindow>
#include <QTimer>
#include <QRegularExpression>





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

	/** The song on which the context menu actions are applied. */
	std::shared_ptr<Song> m_ContextSong;

	/** All the songs matching the current filter.
	Updated when the filter is chosen by the user, in updateAllFilterSongs().
	Used to populate the song list, after post-filtering with m_SearchFilter, in applySearchFilterToSongs(). */
	std::vector<std::shared_ptr<Song>> m_AllFilterSongs;

	/** The search string entered by the user to filter out songs.
	Updated immediately on UI change, but applied with a delay not to slow down the UI. */
	QString m_NewSearchText;

	/** The compiled search string entered by the user to filter out songs. */
	QRegularExpression m_SearchFilter;

	int m_TicksUntilSetSearchText;



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

	/** Updates the list item representing the specified song by the song current data.
	If the song is not represented by any item, ignored silently. */
	void updateSongItem(Song & a_Song);

	/** Sets the background color of m_ContextSong. */
	void setContextSongBgColor(QColor a_BgColor);

	/** Sets the rating of m_ContextSong. */
	void rateContextSong(double a_LocalRating);

	/** Returns the list item representing the given song, or nullptr if no such item. */
	QListWidgetItem * itemFromSong(Song & a_Song);

	/** Shows the context menu for a song at the specified position.
	The menu items act on m_ContextSong (so that this can be reused from multiple song sources). */
	void showSongContextMenu(const QPoint & a_Pos, std::shared_ptr<Song> a_Song);

	/** Updates lwSongs with all songs from m_AllFilterSongs that match m_SearchFilter.
	Called when the user selects a different template-filter, or if they edit the search filter. */
	void applySearchFilterToSongs();


private slots:

	/** Switches to Playlist mode, if available, and hides this window. */
	void switchToPlaylistMode();

	/** Updates the list of song matching the currently selected filter. */
	void filterItemSelected();

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
	void showSongListContextMenu(const QPoint & a_Pos);

	/** Shows the context menu for lblCurrentlyPlaying at the specified position.
	The context menu is the same as for lwSongs. */
	void showCurSongContextMenu(const QPoint & a_Pos);

	/** The user has changed the search text.
	Updates the m_SearchFilter and starts a countdown for it to be applied.
	Does not apply the filter immediately, so that the UI doesn't slow down waiting for the filtering. */
	void searchTextEdited(const QString & a_NewSearchText);
};





#endif // CLASSROOMWINDOW_HPP

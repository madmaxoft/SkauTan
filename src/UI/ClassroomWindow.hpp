#pragma once

#include <memory>
#include <QMainWindow>
#include <QTimer>
#include <QRegularExpression>
#include "../DJControllers.hpp"
#include "../IPlaylistItem.hpp"
#include "../Song.hpp"





// fwd:
class QListWidgetItem;
class ComponentCollection;
class Filter;
namespace Ui
{
	class ClassroomWindow;
}





/** Wrapper for the UI that handles the classroom-face of the main window.
Displays a list of filters on the left, and for the selected filter, the matching songs in the main area.
Has a player at the bottom, songs can be played by dbl-clicking them. */
class ClassroomWindow:
	public QMainWindow
{
	Q_OBJECT

public:

	explicit ClassroomWindow(ComponentCollection & aComponents);

	~ClassroomWindow();

	/** Sets the window to use for switching to the Playlist mode. */
	void setPlaylistWindow(QWidget & aPlaylistWindow);


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::ClassroomWindow> mUI;

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The timer used for periodic UI updates. */
	QTimer mUpdateTimer;

	/** The window that will be shown when asked to switch to Playlist mode. */
	QWidget * mPlaylistWindow;

	/** If positive, the number of mUpdateTimer ticks until the current Duration Limit settings are applied.
	Used by the leDurationLimit to apply the settings only after some time after the last edit,
	to avoid applying a limit parsed from number in the middle of editing. */
	int mTicksToDurationLimitApply;

	/** The song (SharedData) on which the context menu actions are applied. */
	std::shared_ptr<Song::SharedData> mContextSongSD;

	/** The song (SharedData) currently playing. */
	std::shared_ptr<Song::SharedData> mCurrentSongSD;

	/** All the songs (SharedDatas) matching the current filter.
	Updated when the filter is chosen by the user, in updateAllFilterSongs().
	Used to populate the song list, after post-filtering with mSearchFilter, in applySearchFilterToSongs(). */
	std::vector<std::shared_ptr<Song::SharedData>> mAllFilterSharedDatas;

	/** The search string entered by the user to filter out songs.
	Updated immediately on UI change, but applied with a delay not to slow down the UI. */
	QString mNewSearchText;

	/** The compiled search string entered by the user to filter out songs. */
	QRegularExpression mSearchFilter;

	/** Number of periodicUpdateUi()'s ticks before mNewSearchText is applied from the lineedit to
	the song list. */
	int mTicksUntilSetSearchText;

	/** Number of periodicUpdateUi()'s ticks before the tempo shown for all songs is updated by the current
	tempo adjustment. */
	int  mTicksUntilUpdateTempo;

	/** The current tempo coefficient, as set by the tempo adjust slider. */
	double mCurrentTempoCoeff;

	// The DJControllers registrations:
	DJControllers::KeyHandlerRegPtr    mDjKeyHandler;
	DJControllers::SliderHandlerRegPtr mDjSliderHandler;
	DJControllers::WheelHandlerRegPtr  mDjWheelHandler;


	/** Updates the list of filters in lwFilters. */
	void updateFilterList();

	/** Returns the filter currently selected in lwFilters, or nullptr if none. */
	std::shared_ptr<Filter> selectedFilter();

	/** Adds the specified song (SharedData) to the playlist and starts playing it.
	If another song is currently playing, fade-out is used for it. */
	void startPlayingSong(std::shared_ptr<Song::SharedData> aSongSD);

	/** Applies the settings in the UI related to Duration Limit to the currently playing track. */
	void applyDurationLimitSettings();

	/** Updates the list item by the current data from its linked Song. */
	void updateSongItem(QListWidgetItem & aItem);

	/** Updates the list item representing the specified song (SharedData) by the song current data.
	If the song is not represented by any item, ignored silently. */
	void updateSongSDItem(Song::SharedData & aSongSD);

	/** Sets the background color of mContextSong. */
	void setContextSongSDBgColor(QColor aBgColor);

	/** Sets the rating of mContextSong. */
	void rateContextSongSD(double aLocalRating);

	/** Returns the list item representing the given song, or nullptr if no such item. */
	QListWidgetItem * itemFromSongSD(Song::SharedData & aSongSD);

	/** Shows the context menu for a song (SharedData) at the specified position.
	Sets mContextSongSD to aSongSD.
	The menu items act on mContextSongSD (so that this can be reused from multiple song sources (song list,
	song name in player, ...)). */
	void showSongSDContextMenu(const QPoint & aPos, std::shared_ptr<Song::SharedData> aSongSD);

	/** Updates lwSongs with all songs from mAllFilterSongs that match mSearchFilter.
	Called when the user selects a different template-filter, or if they edit the search filter. */
	void applySearchFilterToSongs();

	/** Updates the songs listed in lwSongs with their tempo adjusted by the current tempo adjustment value. */
	void applyTempoAdjustToSongs();

	/** Initializes the window to accept input by DJ controllers. */
	void setUpDjControllers();


public slots:

	/** Handler for keypresses on the DJ controller. */
	void handleDjControllerKey(int aKey);

	/** Handler for slider changes on the DJ controller. */
	void handleDjControllerSlider(int aSlider, qreal aValue);

	/** Handler for wheel moves on the DJ controller. */
	void handleDjControllerWheel(int aWheel, int aNumSteps);


private slots:

	/** Switches to Playlist mode, if available, and hides this window. */
	void switchToPlaylistMode();

	/** Updates the list of song matching the currently selected filter. */
	void filterItemSelected();

	/** The user has dbl-clicked on the specified item.
	Starts playing the song, applying a fadeout if already playing another before. */
	void songItemDoubleClicked(QListWidgetItem * aItem);

	/** Called periodically to update the UI. */
	void periodicUIUpdate();

	/** Emitted by the global volume control slider; updates the player volume. */
	void volumeSliderMoved(int aNewValue);

	/** Emitted by the global tempo slider; updates the playback tempo. */
	void tempoValueChanged(int aNewValue);

	/** The user clicked the "reset tempo" button. */
	void tempoResetClicked();

	/** Emitted by Player after changing the volume from within the player,
	such as loading a new track with pre-set default volume and KeepVolume turned off.
	Updates the Volume UI. */
	void playerVolumeChanged(qreal aVolume);

	/** Emitted by Player after changing the tempo from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off.
	Updates the Tempo UI. */
	void playerTempoChanged(qreal aTempoCoeff);

	/** Emitted by Player when it encounters an invalid track.
	Updates the UI to indicate the failure. */
	void playerInvalidTrack(IPlaylistItemPtr aTrack);

	/** The Duration Limit checkbox was clicked, applies or removes the duration limit. */
	void durationLimitClicked();

	/** The Duration Limit lineedit was edited, applies the duration limit.
	If the string is invalid, colors the text red and refuses to apply the limit. */
	void durationLimitEdited(const QString & aNewText);

	/** Plays the song that is currently selected.
	If another song is already playing, applies a fade-out to it first. */
	void playSelectedSong();

	/** Removes from library the selected song (all its instances via SharedData). */
	void removeFromLibrary();

	/** Deletes from the disk the selected song's files (all their instances via SharedData). */
	void deleteFromDisk();

	/** Shows the Properties dialog for the selected song. */
	void showSongProperties();

	/** Shows a dialog with all songs. */
	void showSongs();

	/** Shows the Manage filters dialog. */
	void showFilters();

	/** Shows the Background tasks dialog */
	void showBackgroundTasks();

	/** Shows a dialog listing all removed songs. */
	void showRemovedSongs();

	/** Shows a dialog for importing DB data. */
	void importDB();

	/** Shows a dialog with library maintenance options. */
	void libraryMaintenance();

	/** Shows the dialog that lists the contents of the debug log. */
	void showDebugLog();

	/** Shows the context menu for lwSongs items at the specified position. */
	void showSongListContextMenu(const QPoint & aPos);

	/** Shows the context menu for lblCurrentlyPlaying at the specified position.
	The context menu is the same as for lwSongs. */
	void showCurSongContextMenu(const QPoint & aPos);

	/** The user has changed the search text.
	Updates the mSearchFilter and starts a countdown for it to be applied.
	Does not apply the filter immediately, so that the UI doesn't slow down waiting for the filtering. */
	void searchTextEdited(const QString & aNewSearchText);
};

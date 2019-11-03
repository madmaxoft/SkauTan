#pragma once

#include <memory>

#include <QMainWindow>
#include <QTimer>

#include "PlaylistItemModel.hpp"
#include "../DJControllers.hpp"





// fwd:
class QListWidgetItem;
class Player;
class Song;
class ComponentCollection;
namespace Ui
{
	class PlayerWindow;
}
using SongPtr = std::shared_ptr<Song>;





/** Main window of the application.
Provides playlist,playback control and entrypoints to other dialogs. */
class PlayerWindow:
	public QMainWindow
{
	Q_OBJECT
	using Super = QMainWindow;


public:

	explicit PlayerWindow(ComponentCollection & aComponents);

	~PlayerWindow();

	/** Sets the window to use for switching to the Classroom mode. */
	void setClassroomWindow(QWidget & aClassroomWindow);


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PlayerWindow> mUI;

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The model used to display the playlist. */
	std::unique_ptr<PlaylistItemModel> mPlaylistModel;

	/** The delegate used for drawing individual playlist items. */
	std::unique_ptr<PlaylistItemDelegate> mPlaylistDelegate;

	/** The timer used for periodic UI updates. */
	QTimer mUpdateTimer;

	/** Stores whether the LibraryRescan progress is shown or not; updated periodically. */
	bool mIsLibraryRescanShown;

	/** The total number of songs that were in the LibraryRescan UI on the last update.
	Used for detecting whether to change the UI. */
	int mLastLibraryRescanTotal;

	/** The queue length in the LibraryRescan UI on the last update.
	Used for detecting whether to change the UI. */
	int mLastLibraryRescanQueue;

	/** Set to true while updating the UI as a reaction to an internal event.
	If true, the changes in the UI shouldn't be propagated further. */
	bool mIsInternalChange;

	/** The window that is shown when asked to switch to the Classroom mode. */
	QWidget * mClassroomWindow;

	// The DJControllers registrations:
	DJControllers::KeyHandlerRegPtr    mDjKeyHandler;
	DJControllers::SliderHandlerRegPtr mDjSliderHandler;
	DJControllers::WheelHandlerRegPtr  mDjWheelHandler;


	/** Sets the local rating for the selected songs. */
	void rateSelectedSongs(double aLocalRating);

	/** Returns all songs that are selected in the playlist, in the playlist's order. */
	std::vector<SongPtr> selectedPlaylistSongs() const;

	/** Refreshes the items in lwQuickPlayer to match the favorite template items in the DB. */
	void refreshQuickPlay();

	/** Refreshes the items in cbAppendUponCompletion to match the favorite templates in the DB. */
	void refreshAppendUponCompletion();

	/** Sets the specified duration limit for all playlist items selected in mtblPlaylist. */
	void setSelectedItemsDurationLimit(double aNewDurationLimit);

	/** Returns the template or template item that the user has chosen to append upon playlist completion.
	Returns a valid template / item even if the checkbox is unchecked (so that it can be used for preserving
	the combobox selection when refreshing). */
	QVariant objectToAppendUponCompletion() const;

	/** Sets up all that is needed to support the DJ controllers in this window. */
	void setUpDjControllers();


public slots:

	/** Handler for keypresses on the DJ controller. */
	void handleDjControllerKey(int aKey);

	/** Handler for slider changes on the DJ controller. */
	void handleDjControllerSlider(int aSlider, qreal aValue);

	/** Handler for wheel moves on the DJ controller. */
	void handleDjControllerWheel(int aWheel, int aNumSteps);


private slots:

	/** Shows the Songs dialog. */
	void showSongs();

	/** Shows the TemplateList dialog. */
	void showTemplates();

	/** Shows the History dialog. */
	void showHistory();

	/** Adds the specified song to the playlist.
	The song is assumed to be present in mDB (but not checked). */
	void addSongToPlaylist(SongPtr aSong);

	/** Inserts the specified song to the playlist after the current selection.
	The song is assumed to be present in mDB (but not checked). */
	void insertSongToPlaylist(SongPtr aSong);

	/** Adds the specified item to the playlist. */
	void addPlaylistItem(std::shared_ptr<IPlaylistItem> aItem);

	/** Deletes the selected items from the playlist. */
	void deleteSelectedPlaylistItems();

	/** Emitted by tblPlaylist when the user dblclicks a track. */
	void trackDoubleClicked(const QModelIndex & aTrack);

	/** Emitted by the global volume control slider; updates the player volume. */
	void volumeSliderMoved(int aNewValue);

	/** Shows the list of templates, after choosing one, adds songs using that template to playlist. */
	void addFromTemplate();

	/** Switches to the classroom mode, if available, and hides this window. */
	void switchToClassroomMode();

	/** Emitted by the global tempo slider; updates the playback tempo. */
	void tempoValueChanged(int aNewValue);

	/** The user clicker the Reset tempo button, resets tempo to 100 %. */
	void resetTempo();

	/** Shows the BackgroundTasks dialog. */
	void showBackgroundTasks();

	/** Called periodically by a timer.
	Updates the player-related UI. */
	void periodicUIUpdate();

	/** Emitted by Qt when the user r-clicks in tblPlaylist.
	Shows the playlist context menu. */
	void showPlaylistContextMenu(const QPoint & aPos);

	/** The user wants to see the properties of the selected playlist item. */
	void showSongProperties();

	/** Deletes the files of the selected songs; removes from library and playlist. */
	void deleteSongsFromDisk();

	/** Removes the selected songs from the library; removes from playlist. */
	void removeSongsFromLibrary();

	/** Jumps to the first selected playlist item and starts playing it.
	If the item is already playing, ignored.
	If another item is already playing, fades it out first. */
	void jumpToAndPlay();

	/** The user has clicked a QuickPlayer item, insert it into the playlist and maybe start playing. */
	void quickPlayItemClicked(QListWidgetItem * aItem);

	/** Asks the user for the new duration limit, then applies it to all selected playlist items. */
	void setDurationLimit();

	/** Removes the duration limit from all selected playlist items. */
	void removeDurationLimit();

	/** Replaces the song at the specified index with another song matching the template.
	If the playlist item doesn't have a template item attached, does nothing. */
	void replaceSong(const QModelIndex & aIndex);

	/** Shows the DlgRemovedSongs. */
	void showRemovedSongs();

	/** Emitted by Player after starting playback of a new track.
	Used to append another round from template in cbAppendUponCompletion, if at playlist end and allowed by the user. */
	void playerStartedPlayback();

	/** Emitted by Player after changing the tempo from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off.
	Updates the Tempo UI. */
	void tempoCoeffChanged(qreal aTempoCoeff);

	/** Emitted by Player after changing the volume from within the player,
	such as loading a new track with pre-set default volume and KeepVolume turned off.
	Updates the Volume UI. */
	void playerVolumeChanged(qreal aVolume);

	/** Emitted by the player when it attempts to play a track and cannot do so (bad file, bad format, ...)
	Marks the item as unplayable in the UI. */
	void invalidTrack(IPlaylistItemPtr aItem);

	/** Shows DlgImportDB, then imports the data from the DB. */
	void importDB();

	/** Shows DlgLibraryMaintenance. */
	void showLibraryMaintenance();

	/** Emitted when the user selects Save Playlist from the Tools menu.
	Asks for filename, then saves the current playlist into the file. */
	void savePlaylist();

	/** Emitted when the user selects Load Playlist from the Tools menu.
	Asks for filename, then loads the playlist from the file and inserts it after the current selection. */
	void loadPlaylist();

	/** Toggles the LocalVoteServer on or off. */
	void toggleLvs();

	/** The user has connected a recognized DJ controller. */
	void djControllerConnected(const QString & aName);

	/** A DJ controller was disconnected. */
	void djControllerRemoved();

	/** Shows the status dialog for the LocalVoteServer. */
	void showLvsStatus();

	/** Shows the DebugLog dialog. */
	void showDebugLog();
};

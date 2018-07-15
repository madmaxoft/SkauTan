#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H





#include <memory>

#include <QMainWindow>
#include <QTimer>

#include "PlaylistItemModel.h"





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

	explicit PlayerWindow(ComponentCollection & a_Components);

	~PlayerWindow();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PlayerWindow> m_UI;

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The model used to display the playlist. */
	std::unique_ptr<PlaylistItemModel> m_PlaylistModel;

	/** The delegate used for drawing individual playlist items. */
	std::unique_ptr<PlaylistItemDelegate> m_PlaylistDelegate;

	/** The timer used for periodic UI updates. */
	QTimer m_UpdateTimer;

	/** Stores whether the LibraryRescan progress is shown or not; updated periodically. */
	bool m_IsLibraryRescanShown;

	/** The total number of songs that were in the LibraryRescan UI on the last update.
	Used for detecting whether to change the UI. */
	int m_LastLibraryRescanTotal;

	/** The queue length in the LibraryRescan UI on the last update.
	Used for detecting whether to change the UI. */
	int m_LastLibraryRescanQueue;

	/** Set to true while updating the UI as a reaction to an internal event.
	If true, the changes in the UI shouldn't be propagated further. */
	bool m_IsInternalChange;


	/** Sets the local rating for the selected songs. */
	void rateSelectedSongs(double a_LocalRating);

	/** Returns all songs that are selected in the playlist, in the playlist's order. */
	std::vector<SongPtr> selectedPlaylistSongs() const;

	/** Refreshes the items in lwQuickPlayer to match the favorite template items in the DB. */
	void refreshQuickPlayer();

	/** Refreshes the items in cbAppendUponCompletion to match the favorite templates in the DB. */
	void refreshAppendUponCompletion();

	/** Sets the specified duration limit for all playlist items selected in m_tblPlaylist. */
	void setSelectedItemsDurationLimit(double a_NewDurationLimit);

	/** Returns the template that the user has chosen to append upon playlist completion.
	Returns a valid template even if the checkbox is unchecked (so that it can be used for preserving
	the combobox selection when refreshing). */
	TemplatePtr templateToAppendUponCompletion() const;


private slots:

	/** Shows the Songs dialog. */
	void showSongs();

	/** Shows the TemplateList dialog. */
	void showTemplates();

	/** Shows the History dialog. */
	void showHistory();

	/** Adds the specified song to the playlist.
	The song is assumed to be present in m_DB (but not checked). */
	void addSongToPlaylist(SongPtr a_Song);

	/** Inserts the specified song to the playlist after the current selection.
	The song is assumed to be present in m_DB (but not checked). */
	void insertSongToPlaylist(SongPtr a_Song);

	/** Adds the specified item to the playlist. */
	void addPlaylistItem(std::shared_ptr<IPlaylistItem> a_Item);

	/** Deletes the selected items from the playlist. */
	void deleteSelectedPlaylistItems();

	/** Emitted by tblPlaylist when the user dblclicks a track. */
	void trackDoubleClicked(const QModelIndex & a_Track);

	/** Emitted by the global volume control slider; updates the player volume. */
	void volumeSliderMoved(int a_NewValue);

	/** Shows the list of templates, after choosing one, adds songs using that template to playlist. */
	void addFromTemplate();

	/** Emitted by the global tempo slider; updates the playback tempo. */
	void tempoValueChanged(int a_NewValue);

	/** The user clicker the Reset tempo button, resets tempo to 100 %. */
	void resetTempo();

	/** Shows the BackgroundTasks dialog. */
	void showBackgroundTasks();

	/** Called periodically by a timer.
	Updates the player-related UI. */
	void periodicUIUpdate();

	/** Emitted by Qt when the user r-clicks in tblPlaylist.
	Shows the playlist context menu. */
	void showPlaylistContextMenu(const QPoint & a_Pos);

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
	void quickPlayerItemClicked(QListWidgetItem * a_Item);

	/** Asks the user for the new duration limit, then applies it to all selected playlist items. */
	void setDurationLimit();

	/** Removes the duration limit from all selected playlist items. */
	void removeDurationLimit();

	/** Replaces the song at the specified index with another song matching the template.
	If the playlist item doesn't have a template item attached, does nothing. */
	void replaceSong(const QModelIndex & a_Index);

	/** Shows the DlgRemovedSongs. */
	void showRemovedSongs();

	/** Emitted by Player after starting playback of a new track.
	Used to append another round from template in cbAppendUponCompletion, if at playlist end and allowed by the user. */
	void playerStartedPlayback();

	/** Emitted by Player after changing the tempo from within the player,
	such as loading a new track with pre-set default tempo and KeepTempo turned off.
	Updates the Tempo UI. */
	void tempoCoeffChanged(qreal a_TempoCoeff);
};





#endif // PLAYERWINDOW_H

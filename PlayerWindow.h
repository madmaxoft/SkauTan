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
class Database;
class MetadataScanner;
class HashCalculator;
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

	explicit PlayerWindow(
		Database & a_DB,
		MetadataScanner & a_Scanner,
		HashCalculator & a_Hasher,
		Player & a_Player
	);

	~PlayerWindow();


private:

	std::unique_ptr<Ui::PlayerWindow> m_UI;

	/** The DB of all the songs and templates. */
	Database & m_DB;

	/** The scanner for ID3 and other metadata. */
	MetadataScanner & m_MetadataScanner;

	/** Calculates hashes for song files.
	Used to query queue length. */
	HashCalculator & m_HashCalculator;

	/** The player that sends the audio data to the output and manages the playlist. */
	Player & m_Player;

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

	/** Sets the local rating for the selected songs. */
	void rateSelectedSongs(double a_LocalRating);

	/** Returns all songs that are selected in the playlist, in the playlist's order. */
	std::vector<SongPtr> selectedPlaylistSongs() const;

	/** Refreshes the items in lwQuickPlayer to match the favorite template items in the DB. */
	void refreshQuickPlayer();

	/** Sets the specified duration limit for all playlist items selected in m_tblPlaylist. */
	void setSelectedItemsDurationLimit(double a_NewDurationLimit);


private slots:

	/** Shows the Songs dialog.
	Signature must match QPushButton::clicked(). */
	void showSongs(bool a_IsChecked);

	/** Shows the TemplateList dialog.
	Signature must match QPushButton::clicked(). */
	void showTemplates(bool a_IsChecked);

	/** Shows the History dialog.
	Signature must match QPushButton::clicked(). */
	void showHistory(bool a_IsChecked);

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

	/** Advances the playlist backwards.
	Signature must match QPushButton::clicked(). */
	void prevTrack(bool a_IsChecked);

	/** Starts or stops the playback.
	Signature must match QPushButton::clicked(). */
	void playPause(bool a_IsChecked);

	/** Advances the playlist forward.
	Signature must match QPushButton::clicked(). */
	void nextTrack(bool a_IsChecked);

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

	/** Removes the selected songs from the playlist. */
	void removeSongsFromPlaylist();

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
};





#endif // PLAYERWINDOW_H

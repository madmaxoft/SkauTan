#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H





#include <memory>

#include <QMainWindow>
#include <QTimer>

#include "PlaylistItemModel.h"





// fwd:
class QShortcut;
class Player;
class Song;
class Database;
class MetadataScanner;
class HashCalculator;
namespace Ui
{
	class PlayerWindow;
}





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

	/** The shortcut for deleting playlist items using the Del key. */
	std::unique_ptr<QShortcut> m_scDel;

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


private slots:

	/** Shows the QuickPlayer dialog. */
	void showQuickPlayer();

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
	void addSong(std::shared_ptr<Song> a_Song);

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

	/** The QuickPlay dialog wants us to enqueue and play this item. */
	void addAndPlayTemplateItem(Template::Item * a_Item);

	/** Emitted by the global tempo slider; updates the playback tempo. */
	void tempoValueChanged(int a_NewValue);

	/** The user clicker the Reset tempo button, resets tempo to 100 %. */
	void resetTempo();

	/** Shows the Tools popup menu. */
	void showToolsMenu();

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
};





#endif // PLAYERWINDOW_H

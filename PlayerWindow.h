#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H





#include <QMainWindow>
#include <memory>
#include "PlaylistItemModel.h"





// fwd:
class QShortcut;
class Player;
class Song;
class Database;
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
	explicit PlayerWindow(QWidget * a_Parent = nullptr);
	~PlayerWindow();


private:

	std::unique_ptr<Ui::PlayerWindow> m_UI;

	std::unique_ptr<Database> m_DB;

	/** The current playlist. */
	std::shared_ptr<Playlist> m_Playlist;

	/** The model used to display the playlist. */
	std::unique_ptr<PlaylistItemModel> m_PlaylistModel;

	/** The shortcut for deleting playlist items using the Del key. */
	std::unique_ptr<QShortcut> m_scDel;

	/** The player that sends the audio data to the output. */
	std::unique_ptr<Player> m_Player;


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

	/** Emitted by m_Player before it starts playing the specified item. */
	void startingItemPlayback(IPlaylistItem * a_Item);
};





#endif // PLAYERWINDOW_H

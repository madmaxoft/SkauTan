#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H





#include <QMainWindow>
#include <memory>
#include "PlaylistItemModel.h"





// fwd:
class QShortcut;
class Song;
class SongDatabase;
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

	std::unique_ptr<SongDatabase> m_DB;

	/** The current playlist. */
	std::shared_ptr<Playlist> m_Playlist;

	/** The model used to display the playlist. */
	std::unique_ptr<PlaylistItemModel> m_PlaylistModel;

	/** The shortcut for deleting playlist items using the Del key. */
	std::unique_ptr<QShortcut> m_scDel;


private slots:

	/** Shows the Songs dialog.
	Signature must match QPushButton::clicked(). */
	void showSongs(bool a_IsChecked);

	/** Adds the specified song to the playlist.
	The song is assumed to be present in m_DB (but not checked). */
	void addSong(std::shared_ptr<Song> a_Song);

	/** Adds the specified item to the playlist. */
	void addPlaylistItem(std::shared_ptr<IPlaylistItem> a_Item);

	/** Deletes the selected items from the playlist. */
	void deleteSelectedPlaylistItems();
};





#endif // PLAYERWINDOW_H

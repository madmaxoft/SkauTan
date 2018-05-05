#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H





#include <QMainWindow>
#include <memory>
#include "PlaylistItemModel.h"





// fwd:
class QShortcut;
class QTimer;
class Player;
class Song;
class Database;
class MetadataScanner;
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

	explicit PlayerWindow(Database & a_DB, MetadataScanner & a_Scanner, Player & a_Player);

	~PlayerWindow();


private:

	std::unique_ptr<Ui::PlayerWindow> m_UI;

	/** The DB of all the songs and templates. */
	Database & m_DB;

	/** The scanner for ID3 and other metadata. */
	MetadataScanner & m_MetadataScanner;

	/** The player that sends the audio data to the output and manages the playlist. */
	Player & m_Player;

	/** The model used to display the playlist. */
	std::unique_ptr<PlaylistItemModel> m_PlaylistModel;

	/** The shortcut for deleting playlist items using the Del key. */
	std::unique_ptr<QShortcut> m_scDel;

	/** The timer that periodically updates the UI. */
	std::unique_ptr<QTimer> m_UpdateUITimer;

	/** Set to true if the position update is coming from the player internals (rather than user input). */
	bool m_IsInternalPositionUpdate;


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

	/** Updates the range on the position slider. */
	void updatePositionRange();

	/** Updates the time position indicators.
	Called regularly from a timer, and when changing tracks. */
	void updateTimePos();

	/** Sets the playback position to the specified value (in seconds). */
	void setTimePos(int a_NewValue);

	/** The QuickPlay dialog wants us to enqueue and play this item. */
	void addAndPlayTemplateItem(Template::Item * a_Item);

	/** Emitted by the global tempo slider; updates the playback tempo. */
	void tempoValueChanged(int a_NewValue);

	/** The user clicker the Reset tempo button, resets tempo to 100 %. */
	void resetTempo();
};





#endif // PLAYERWINDOW_H

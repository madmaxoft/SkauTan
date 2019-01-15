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


	/** Updates the list of filters in lwFilters. */
	void updateFilterList();

	/** Returns the filter currently selected in lwFilters, or nullptr if none. */
	std::shared_ptr<Filter> selectedFilter();

	/** Adds the specified song to the playlist and starts playing it.
	If another song is currently playing, fade-out is used for it. */
	void startPlayingSong(std::shared_ptr<Song> a_Song);

private slots:

	/** Switches to Playlist mode, if available, and hides this window. */
	void switchToPlaylistMode();

	/** Updates the list of song matching the currently selected filter. */
	void updateSongList();

	/** The user has clicked on the specified item.
	If there's nothing playing, starts playing the song, otherwise only selects the item. */
	void songItemClicked(QListWidgetItem * a_Item);

	/** The user has dbl-clicked on the specified item.
	Starts playing the song, applying a fadeout if already playing another before. */
	void songItemDoubleClicked(QListWidgetItem * a_Item);

	/** Called periodically to update the UI. */
	void periodicUIUpdate();
};





#endif // CLASSROOMWINDOW_HPP

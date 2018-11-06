#ifndef DLGSONGS_H
#define DLGSONGS_H





#include <memory>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QTimer>
#include "../SongModel.hpp"





// fwd:
class Song;
class ComponentCollection;
class QMenu;
namespace Ui
{
	class DlgSongs;
}





/** Dialog that presents a list of all currently known songs to the user. */
class DlgSongs:
	public QDialog
{
	Q_OBJECT
	using Super = QDialog;


public:

	/** Creates a new dialog instance.
	a_FilterModel is the filter to apply to songs before displaying, nullptr to show all songs.
	If a_ShowManipulators is true, the buttons for adding songs / adding to playlist are shown. */
	explicit DlgSongs(
		ComponentCollection & a_Components,
		std::unique_ptr<QSortFilterProxyModel> && a_FilterModel,
		bool a_ShowManipulators,
		QWidget * a_Parent
	);

	virtual ~DlgSongs() override;

	/** Adds songs from the specified files to the DB, skipping non-existent files.
	Duplicate files are skipped. */
	void addFiles(const QStringList & a_FileNames);

	/** Adds songs from the specified path to the DB, recursively.
	Skips duplicate files. */
	void addFolderRecursive(const QString & a_Path);


private:

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The Qt-managed UI.  */
	std::unique_ptr<Ui::DlgSongs> m_UI;

	/** The filter that is applied to m_SongModel before displaying. */
	std::unique_ptr<QSortFilterProxyModel> m_FilterModel;

	/** The songs, displayed in the UI. */
	SongModel m_SongModel;

	/** The advanced filter for the song model. */
	SongModelFilter m_SongModelFilter;

	/** Timer for updating the UI periodically with background-generated information. */
	QTimer m_PeriodicUiUpdate;

	/** Stores whether the LibraryRescan progress is shown or not; updated periodically. */
	bool m_IsLibraryRescanShown;

	/** The total number of songs that were in the LibraryRescan UI on the last update.
	Used for detecting whether to change the UI. */
	int m_LastLibraryRescanTotal;

	/** The queue length in the LibraryRescan UI on the last update.
	Used for detecting whether to change the UI. */
	int m_LastLibraryRescanQueue;

	/** The new search text to be set into m_SongModelFilter in periodic UI update.
	The text isn't set immediately to avoid slowdowns while still typing the string. */
	QString m_NewSearchText;

	/** Number of ticks until m_NewSearchText is set into m_SongModelFilter in the periodic UI update.
	The text isn't set immediately to avoid slowdowns while still typing the string,
	this counter goes from a fixed value down to zero on each periodic UI update and only when reaching zero
	is the search text applied. */
	int m_TicksUntilSetSearchText;

	/** The context menu to display for rclk in tblSongs. */
	std::unique_ptr<QMenu> m_ContextMenu;


	/** Updates the UI related to song stats (count, filter) */
	void updateSongStats();

	/** Adds the filter items into the cbFilter UI, and sets up connections for the search and filter. */
	void initFilterSearch();

	/** Creates the m_ContextMenu. */
	void createContextMenu();

	/** Returns the song represented by the specified index in tblSongs.
	Translates the index through m_FilterModel and m_SongModelFilter. */
	SongPtr songFromIndex(const QModelIndex & a_Index);

	/** Sets the selected songs' local rating to the specified value. */
	void rateSelectedSongs(double a_Rating);


private slots:

	/** Opens up a file picker, then adds the selected song files into the DB. */
	void chooseAddFile();

	/** Opens up a folder picker, then adds the songs from the picked folder into the DB. */
	void chooseAddFolder();

	/** After confirmation, removes the selected songs from the DB. */
	void removeSelected();

	/** After confirmation, deletes the selected songs' files from the disk
	(as well as the songs from the DB). */
	void deleteFromDisk();

	/** Adds the selected songs into the playlist.
	Emits the addSongToPlaylist() signal for each selected song. */
	void addSelectedToPlaylist();

	/** Inserts the selected songs into the playlist.
	Emits the insertSongToPlaylist() signal for each selected song, in reverse order,
	so that the songs are inserted in the original order. */
	void insertSelectedToPlaylist();

	/** Queues the selected songs for metadata rescan. */
	void rescanMetadata();

	/** Emitted by the model after the user edits a song. */
	void modelSongEdited(SongPtr a_Song);

	/** Called periodically to update the UI with background-generated information. */
	void periodicUiUpdate();

	/** The user has chosen a filter from cbFilter.
	Applies the filter to m_SongModelFilter. */
	void filterChosen(int a_Index);

	/** The user has edited the search text in leSearch.
	Applies the search text to m_SongModelFilter. */
	void searchTextEdited(const QString & a_NewText);

	/** Shows the tblSongs context menu at the specified position. */
	void showSongsContextMenu(const QPoint & a_Pos);

	/** Shows the DlgSongProperties for the (first) selected song. */
	void showProperties();

	/** Asks for a (local) rating, then assigns it to all selected songs. */
	void rateSelected();

	/** Shows the tempo detector dialog for the first selected song. */
	void showTempoDetector();

	/** Shows the TapTempo dialog fo the first selected song. */
	void showTapTempo();

	/** Moves the Manual tag values to ID3 tag values, for all selected songs. */
	void moveManualToId3();


signals:

	/** Emitted when the user asks to add songs to the playlist. */
	void addSongToPlaylist(SongPtr a_Song);

	/** Emitted when the user asks to insert songs into the playlist (after the current playlist selection). */
	void insertSongToPlaylist(SongPtr a_Song);
};





#endif // DLGSONGS_H

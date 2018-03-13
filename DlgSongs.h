#ifndef DLGSONGS_H
#define DLGSONGS_H





#include <memory>
#include <QDialog>
#include <QSortFilterProxyModel>
#include "SongModel.h"





// fwd:
class Song;
class Database;
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
		Database & a_DB,
		std::unique_ptr<QSortFilterProxyModel> && a_FilterModel,
		bool a_ShowManipulators,
		QWidget * a_Parent
	);

	~DlgSongs();


	/** Adds songs from the specified path to the DB, recursively.
	Skips duplicate files. Schedules the added files for metadata re-scan. */
	void addFolder(const QString & a_Path);


private:

	/** The Song DB that is being displayed and manipulated. */
	Database & m_DB;

	/** The Qt-managed UI.  */
	std::unique_ptr<Ui::DlgSongs> m_UI;

	/** The filter that is applied to m_SongModel before displaying. */
	std::unique_ptr<QSortFilterProxyModel> m_FilterModel;

	/** The songs, displayed in the UI. */
	SongModel m_SongModel;


private slots:

	/** Opens up a folder picker, then adds the songs from the picked folder into the DB. */
	void chooseAddFolder();

	/** Adds the selected songs into the playlist.
	Emits the addSongToPlaylist() signal for each selected song. */
	void addSelectedToPlaylist();

	/** Queues the selected songs for metadata rescan. */
	void rescanMetadata();

	/** Emitted by the model after the user edits a song. */
	void modelSongEdited(Song * a_Song);

signals:

	/** Emitted when the user asks to add songs to the playlist. */
	void addSongToPlaylist(std::shared_ptr<Song> a_Song);
};





#endif // DLGSONGS_H

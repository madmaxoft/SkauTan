#ifndef DLGSONGS_H
#define DLGSONGS_H





#include <memory>
#include <QDialog>
#include "SongModel.h"





// fwd:
class Song;
class SongDatabase;
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

	explicit DlgSongs(SongDatabase & a_SongDB, QWidget * a_Parent = nullptr);

	~DlgSongs();


	/** Adds songs from the specified path to the DB, recursively.
	Skips duplicate files. Schedules the added files for metadata re-scan. */
	void addFolder(const QString & a_Path);

private:

	/** The Song DB that is being displayed and manipulated. */
	SongDatabase & m_SongDB;

	/** The Qt-managed UI.  */
	std::unique_ptr<Ui::DlgSongs> m_UI;

	/** The songs, displayed in the UI. */
	SongModel m_SongModel;


private slots:

	/** Opens up a folder picker, then adds the songs from the picked folder into the DB.
	Signature needs to match QPushButton::clicked(bool). */
	void chooseAddFolder(bool a_IsChecked);

	/** Closes the dialog.
	Signature needs to match QPushButton::clicked(bool). */
	void closeByButton(bool a_IsChecked);

	/** Adds the selected songs into the playlist.
	Emits the addSong() signal for each selected song.
	Signature needs to match QPushButton::clicked(bool). */
	void addSelectedToPlaylist(bool a_IsChecked);

	/** Emitted by the model after the user edits a song. */
	void modelSongEdited(Song * a_Song);

signals:

	/** Emitted when the user asks to add songs to the playlist. */
	void addSongToPlaylist(std::shared_ptr<Song> a_Song);
};





#endif // DLGSONGS_H

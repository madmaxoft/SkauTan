#ifndef DLGSONGS_H
#define DLGSONGS_H





#include <memory>
#include <QDialog>
#include "SongModel.h"





// fwd:
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
};





#endif // DLGSONGS_H

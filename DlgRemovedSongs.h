#ifndef DLGREMOVEDSONGS_H
#define DLGREMOVEDSONGS_H





#include <memory>
#include <QDialog>





// fwd:
class Database;
namespace Ui
{
	class DlgRemovedSongs;
}





class DlgRemovedSongs:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgRemovedSongs(Database & a_DB, QWidget * a_Parent);
	~DlgRemovedSongs();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgRemovedSongs> m_UI;

	/** The DB from which to show the removed songs. */
	Database & m_DB;


protected slots:

	/** The user clicked the Clear button, remove the logs from the DB. */
	void clearDB();

	/** The user clicked the Export list, export the data to a file. */
	void exportList();
};





#endif // DLGREMOVEDSONGS_H

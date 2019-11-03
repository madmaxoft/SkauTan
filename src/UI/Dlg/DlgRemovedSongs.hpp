#pragma once

#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
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

	explicit DlgRemovedSongs(ComponentCollection & aComponents, QWidget * aParent);
	~DlgRemovedSongs();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgRemovedSongs> mUI;

	/** The components of the entire program. */
	ComponentCollection & mComponents;


protected slots:

	/** The user clicked the Clear button, remove the logs from the DB. */
	void clearDB();

	/** The user clicked the Export list, export the data to a file. */
	void exportList();
};

#ifndef DLGHISTORY_H
#define DLGHISTORY_H





#include <memory>
#include <QDialog>





// fwd:
class SongDatabase;
namespace Ui
{
	class DlgHistory;
}





class DlgHistory:\
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgHistory(SongDatabase & a_SongDB, QWidget * a_Parent = nullptr);
	~DlgHistory();


private:

	/** The Qt-managed UI.  */
	std::unique_ptr<Ui::DlgHistory> m_UI;


private slots:

	/** Closes the dialog.
	Signature must match QPushButton::clicked(). */
	void closeClicked(bool a_IsChecked);

};





#endif // DLGHISTORY_H

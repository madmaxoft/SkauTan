#ifndef DLGVOTESERVER_H
#define DLGVOTESERVER_H





#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
namespace Ui
{
	class DlgVoteServer;
}





class DlgLvsStatus:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgLvsStatus(ComponentCollection & a_Components, QWidget * a_Parent = nullptr);
	~DlgLvsStatus();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgVoteServer> m_UI;

	/** All the components of the app. */
	ComponentCollection & m_Components;


private slots:

	/** Handles doubleclicks in the address list.
	Opens the browser on the dblclicked address. */
	void cellDblClicked(const QModelIndex & a_Index);
};

#endif // DLGVOTESERVER_H

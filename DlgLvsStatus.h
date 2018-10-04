#ifndef DLGLVSSTATUS_H
#define DLGLVSSTATUS_H





#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
namespace Ui
{
	class DlgLvsStatus;
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
	std::unique_ptr<Ui::DlgLvsStatus> m_UI;

	/** All the components of the app. */
	ComponentCollection & m_Components;


private slots:

	/** Handles doubleclicks in the address list.
	Opens the browser on the dblclicked address. */
	void cellDblClicked(const QModelIndex & a_Index);

	/** Shows the QR code for the URL in the current row of the address list. */
	void displayQrCode();

	/** Updates the GUI counter of votes within the current session. */
	void updateVoteCount();
};





#endif // DLGLVSSTATUS_H

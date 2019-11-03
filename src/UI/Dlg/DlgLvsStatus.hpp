#pragma once

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

	explicit DlgLvsStatus(ComponentCollection & aComponents, QWidget * aParent = nullptr);
	~DlgLvsStatus();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgLvsStatus> mUI;

	/** All the components of the app. */
	ComponentCollection & mComponents;


private slots:

	/** Handles doubleclicks in the address list.
	Opens the browser on the dblclicked address. */
	void cellDblClicked(const QModelIndex & aIndex);

	/** Shows the QR code for the URL in the current row of the address list. */
	void displayQrCode();

	/** Updates the GUI counter of votes within the current session. */
	void updateVoteCount();
};

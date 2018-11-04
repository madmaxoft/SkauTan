#ifndef DLGLIBRARYMAINTENANCE_HPP
#define DLGLIBRARYMAINTENANCE_HPP





#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
namespace Ui
{
	class DlgLibraryMaintenance;
}





class DlgLibraryMaintenance:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgLibraryMaintenance(ComponentCollection & a_Components, QWidget * a_Parent = nullptr);
	~DlgLibraryMaintenance();


private:

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgLibraryMaintenance> m_UI;


	/** Invoked by the background thread after inaccessible songs are removed.
	Shows an information message box to the user. */
	Q_INVOKABLE void inaccessibleSongsRemoved(quint32 a_NumRemoved);

private slots:

	/** Removes the songs that don't have a file. */
	void removeInaccessibleSongs();
};





#endif // DLGLIBRARYMAINTENANCE_HPP

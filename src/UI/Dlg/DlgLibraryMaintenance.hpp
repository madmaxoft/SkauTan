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

	/** Invoked by the background thread after failing to export tags.
	Shows a warning dialog to the user. */
	Q_INVOKABLE void tagExportError(const QString & a_FileName, const QString & a_Error);

	/** Invoked by the background thread after finishing the tag export.
	Shows an information message to the user. */
	Q_INVOKABLE void tagExportFinished(const QString & a_FileName);

	/** Invoked by the background thread after failing to import tags.
	Shows a warning dialog to the user. */
	Q_INVOKABLE void tagImportError(const QString & a_FileName, const QString & a_Error);

	/** Invoked by the background thread after finishing the tag import.
	Shows an information message to the user. */
	Q_INVOKABLE void tagImportFinished(const QString & a_FileName);


private slots:

	/** Removes the songs that don't have a file. */
	void removeInaccessibleSongs();

	/** Asks for the destination file, then exports the Primary tags of each library's SongSharedData. */
	void exportAllTags();

	/** Asks for the source file, then imports tags from the file and fills them into the Library. */
	void importTags();
};





#endif // DLGLIBRARYMAINTENANCE_HPP

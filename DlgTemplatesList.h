#ifndef DLGTEMPLATESLIST_H
#define DLGTEMPLATESLIST_H





#include <memory>
#include <QDialog>
#include "Template.h"





// fwd:
class Database;
namespace Ui
{
	class DlgTemplatesList;
}





class DlgTemplatesList:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgTemplatesList(Database & a_DB, QWidget * a_Parent = nullptr);

	virtual ~DlgTemplatesList() override;


private:

	/** The database where the templates are stored. */
	Database & m_DB;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgTemplatesList> m_UI;


	/** Returns the template that is at the specified row in the table. */
	TemplatePtr templateFromRow(int a_Row);

	/** Updates the specified row in the template list with the current template data. */
	void updateTemplateRow(int a_Row, const Template & a_Template);

	/** Exports the selected templates to the specified file. */
	void exportTemplatesTo(const QString & a_FileName);

	/** Imports the templates from the specified file, lets the user choose which ones to import. */
	void importTemplatesFrom(const QString & a_FileName);


protected slots:

	/** Adds a new template, shows the template editor with the new template. */
	void addTemplate();

	/** Opens the template editor for the currently selected template. */
	void editTemplate();

	/** Removes the currently selected templates. */
	void removeTemplates();

	/** Opens the template editor for the template at the specified index. */
	void editTemplateAt(const QModelIndex & a_Index);

	/** A template in the table has been selected, display its items in the details table. */
	void templateSelected(int a_CurrentRow, int a_CurrentColumn);

	/** The selection in the template list has changed. */
	void templateSelectionChanged();

	/** The user clicked the Export button.
	Shows a file selection dialog, then exports selected templates into that file. */
	void exportTemplates();

	/** The user clicked the Import button.
	Shows a file selection dialog, then imports templates from the file. */
	void importTemplates();
};





#endif // DLGTEMPLATESLIST_H

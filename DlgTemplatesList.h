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

	~DlgTemplatesList();


private:

	/** The database where the templates are stored. */
	Database & m_DB;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgTemplatesList> m_UI;


	/** Returns the template that is at the specified row in the table. */
	TemplatePtr templateFromRow(int a_Row);

	/** Updates the specified row in the template list with the current template data. */
	void updateTemplateRow(int a_Row, const Template & a_Template);


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
};





#endif // DLGTEMPLATESLIST_H

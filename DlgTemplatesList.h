#ifndef DLGTEMPLATESLIST_H
#define DLGTEMPLATESLIST_H





#include <memory>
#include <QDialog>
#include "Template.h"





// fwd:
class Database;
class MetadataScanner;
class LengthHashCalculator;
class QTableWidgetItem;
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

	explicit DlgTemplatesList(
		Database & a_DB,
		MetadataScanner & a_Scanner,
		LengthHashCalculator & a_Hasher,
		QWidget * a_Parent = nullptr
	);

	virtual ~DlgTemplatesList() override;


private:

	/** The database where the templates are stored. */
	Database & m_DB;

	/** The scanner that can update the songs' metadata upon request.
	Used when displaying songs matching a filter. */
	MetadataScanner & m_MetadataScanner;

	LengthHashCalculator & m_LengthHashCalculator;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgTemplatesList> m_UI;

	/** Set to true if the UI is updating internally; false when the changes come from the user. */
	bool m_IsInternalChange;


	/** Returns the template that is at the specified row in the table. */
	TemplatePtr templateFromRow(int a_Row);

	/** Returns the template item that is at the specified row in the table.
	a_Template is the template currently being displayed in the Items table.
	a_ItemRow is the row number of the item in the Items table. */
	Template::ItemPtr templateItemFromRow(TemplatePtr a_Template, int a_ItemRow);

	/** Updates the specified row in the template table with the current template data. */
	void updateTemplateRow(int a_Row, const Template & a_Template);

	/** Updates the specified row in the template item table with the current template item data. */
	void updateTemplateItemRow(int a_Row, const Template::Item & a_Item);

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

	/** The selection in the template list has changed. */
	void templateSelectionChanged();

	/** The template in the table has changed, either by us (m_IsInternalChange) or by the user.
	If the change was by the user, save the new contents to the template. */
	void templateChanged(QTableWidgetItem * a_Item);

	/** The template item in the table has changed, either by us (m_IsInternalChange) or by the user.
	If the change was by the user, save the new contents to the template item. */
	void itemChanged(QTableWidgetItem * a_Item);

	/** Opens the template item editor for the template item at the specified index. */
	void editTemplateItemAt(const QModelIndex & a_Index);

	/** The user clicked the Export button.
	Shows a file selection dialog, then exports selected templates into that file. */
	void exportTemplates();

	/** The user clicked the Import button.
	Shows a file selection dialog, then imports templates from the file. */
	void importTemplates();
};





#endif // DLGTEMPLATESLIST_H

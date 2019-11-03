#pragma once

#include <memory>
#include <QDialog>
#include "../../Template.hpp"





// fwd:
class ComponentCollection;
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
		ComponentCollection & aComponents,
		QWidget * aParent = nullptr
	);

	virtual ~DlgTemplatesList() override;


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgTemplatesList> mUI;

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** Set to true if the UI is updating internally; false when the changes come from the user. */
	bool mIsInternalChange;


	/** Returns the template that is at the specified row in the table. */
	TemplatePtr templateFromRow(int aRow);

	/** Updates the specified row in the template table with the current template data. */
	void updateTemplateRow(int aRow, const Template & aTemplate);

	/** Updates the specified row in the template item table with the current template item data. */
	void updateTemplateItemRow(int aRow, const Filter & aItem);

	/** Exports the selected templates to the specified file. */
	void exportTemplatesTo(const QString & aFileName);

	/** Imports the templates from the specified file, lets the user choose which ones to import. */
	void importTemplatesFrom(const QString & aFileName);

	/** Updates the menu for the Add Item button. */
	void updateFilterMenu();

	/** Inserts the specified filter to the current template's items, after the current selection. */
	void insertFilter(FilterPtr aFilter);

	/** Returns the currently selected template.
	Returns nullptr if no selection or multiple templates selected. */
	TemplatePtr currentTemplate();

	/** Swaps the templates on the specified rows both in the UI and in the DB,
	and selects the new row aRow2.
	Asserts that the rows are valid. */
	void swapTemplatesAndSelectSecond(int aRow1, int aRow2);

	/** Swaps the template items on the specified rows both in the UI and in the DB,
	and selects the new row aRow2.
	Asserts that the template and the rows are valid. */
	void swapItemsAndSelectSecond(int aRow1, int aRow2);



protected slots:

	/** Adds a new template, shows the template editor with the new template. */
	void addTemplate();

	/** Removes the currently selected templates. */
	void removeTemplates();

	/** Moves the current template up by one.
	Ignored if multiple templates are selected. */
	void moveTemplateUp();

	/** Moves the current template down by one.
	Ignored if multiple templates are selected. */
	void moveTemplateDown();

	/** The selection in the template list has changed. */
	void templateSelectionChanged();

	/** The template in the table has changed, either by us (mIsInternalChange) or by the user.
	If the change was by the user, save the new contents to the template. */
	void templateChanged(QTableWidgetItem * aItem);

	/** The selection in the item / filter list has changed. */
	void itemSelectionChanged();

	/** The template item at the specified index was doubleclicked.
	Displays the UI for editing the filter. */
	void itemDoubleClicked(int aRow, int aColumn);

	/** The item in the table has changed, either by us (mIsInternalChange) or by the user.
	If the change was by the user, save the new contents to the filter. */
	void itemChanged(QTableWidgetItem * aItem);

	/** The user clicked the Export button.
	Shows a file selection dialog, then exports selected templates into that file. */
	void exportTemplates();

	/** The user clicked the Import button.
	Shows a file selection dialog, then imports templates from the file. */
	void importTemplates();

	/** The user clicked the Manage (filters) button.
	Shows the filter list. */
	void manageFilters();

	/** Removes the currently selected template items, after confirmation. */
	void removeItems();

	/** Moves the current template up by one.
	Ignored if multiple templates are selected. */
	void moveItemUp();

	/** Moves the current template down by one.
	Ignored if multiple templates are selected. */
	void moveItemDown();
};

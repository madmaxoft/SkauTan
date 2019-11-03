#pragma once

#include <memory>
#include <QDialog>
#include <QMenu>





// fwd:
class ComponentCollection;
class Filter;
class QTableWidgetItem;
using FilterPtr = std::shared_ptr<Filter>;
namespace Ui
{
	class DlgManageFilters;
}





class DlgManageFilters:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgManageFilters(
		ComponentCollection & aComponents,
		QWidget * aParent = nullptr
	);

	~DlgManageFilters();


private:

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgManageFilters> mUI;

	/** Set to true when updating the tblFilters' items programmatically.
	Used in filterItemChanged to distinguish between user and program editing the item. */
	bool mIsInternalChange;

	/** The menu used with the Replace button.
	Contains all filters that are currently not selected. */
	QMenu mReplaceMenu;


	/** Updates the table row with the info from the specified filter. */
	void updateFilterRow(int aRow, const Filter & aFilter);

	/** Opens the filter-editing UI on the filter on the specified table row. */
	void editFilterOnRow(int aRow);

	/** Swaps the filters on the specified rows both in the UI and in the DB,
	and selects the new row aRow2.
	Asserts that the rows are valid. */
	void swapFiltersAndSelectSecond(int aRow1, int aRow2);

	/** Updates the menu displayed with the Replace button with all the unselected filters.
	Takes into account the currently selected items, which are NOT added to the menu. */
	void updateReplaceMenu();

	/** Replaces the currently selected filters in all templates with the specified filter.*/
	void replaceWithFilter(FilterPtr aFilter);


private slots:

	/** Adds a new filter at the bottom of the list, then shows the filter editing UI. */
	void addFilter();

	/** Opens the filter-editing UI on the currently selected filter. */
	void editFilter();

	/** Removes the currently selected filters. */
	void removeFilter();

	/** Moves the currently selected filter up. */
	void moveFilterUp();

	/** Moves the currently selected filter down. */
	void moveFilterDown();

	/** Duplicates the selected filter.
	Ignored if multiple filters are selected. */
	void duplicateFilter();

	/** The selection in tblFilters has changed, update the UI to reflect the current selection size. */
	void filterSelectionChanged();

	/** The specified item was edited, either by the user or the program.
	If the change was from the user, update the filter in the DB. */
	void filterChanged(QTableWidgetItem * aItem);

	/** The specified table cell in tblFilters was dblclicked, show the filter editor. */
	void filterDoubleClicked(int aRow, int aColumn);
};

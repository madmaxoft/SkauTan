#ifndef DLGMANAGEFILTERS_H
#define DLGMANAGEFILTERS_H





#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
class Filter;
class QTableWidgetItem;
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
		ComponentCollection & a_Components,
		QWidget * a_Parent = nullptr
	);

	~DlgManageFilters();


private:

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgManageFilters> m_UI;

	/** Set to true when updating the tblFilters' items programmatically.
	Used in filterItemChanged to distinguish between user and program editing the item. */
	bool m_IsInternalChange;


	/** Updates the table row with the info from the specified filter. */
	void updateFilterRow(int a_Row, const Filter & a_Filter);

	/** Opens the filter-editing UI on the filter on the specified table row. */
	void editFilterOnRow(int a_Row);

	/** Swaps the filters on the specified rows both in the UI and in the DB,
	and selects the new row a_Row2.
	Asserts that the rows are valid. */
	void swapFiltersAndSelectSecond(int a_Row1, int a_Row2);


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
	void filterChanged(QTableWidgetItem * a_Item);
};





#endif // DLGMANAGEFILTERS_H

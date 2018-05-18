#ifndef DLGEDITTEMPLATEITEM_H
#define DLGEDITTEMPLATEITEM_H





#include <memory>
#include <QDialog>
#include "Template.h"





// fwd:
class QTreeWidgetItem;
class MetadataScanner;
class HashCalculator;
namespace Ui
{
	class DlgEditTemplateItem;
}





/** Represents a dialog for editing a single Template's Item, including its filters. */
class DlgEditTemplateItem:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	/** Creates a new dialog instance for editing the specified item. */
	explicit DlgEditTemplateItem(
		Database & a_DB,
		MetadataScanner & a_Scanner,
		HashCalculator & a_Hasher,
		Template::Item & a_Item,
		QWidget * a_Parent = nullptr
	);

	virtual ~DlgEditTemplateItem() override;


private:

	/** The DB on which the operations take place. */
	Database & m_DB;

	/** The scanner that can update the songs' metadata upon request.
	Used when displaying songs matching a filter. */
	MetadataScanner & m_MetadataScanner;

	HashCalculator & m_HashCalculator;

	/** The item being edited. */
	Template::Item & m_Item;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgEditTemplateItem> m_UI;


protected:

	/** Called by Qt when the dialog is closed by the OS's [X] button. */
	virtual void reject() override;

	/** Saves the direct UI values into m_Item. */
	void save();

	/** Returns the filter that is represented by the currently selected Filter tree item.
	Returns nullptr if no selection. */
	Template::Filter * selectedFilter() const;

	/** Clears twFilters, then inserts the filter tree into it. */
	void rebuildFilterModel();

	/** Recursively adds children of the specified filter to the specified item. */
	void addFilterChildren(Template::Filter & a_ParentFilter, QTreeWidgetItem & a_ParentItem);

	/** Creates a QStandardItem representing the specified filter. */
	QTreeWidgetItem * createItemFromFilter(const Template::Filter & a_Filter);

	/** Returns a user-visible caption for the specified filter. */
	QString getFilterCaption(const Template::Filter & a_Filter);

	/** Selects the item in twFilters that corresponds to the specified filter.
	If the filter is not found, selection is unchanged. */
	void selectFilterItem(const Template::Filter & a_Filter);

	/** Returns the twFilters' item representing the specified filter.
	Returns nullptr if not found. */
	QTreeWidgetItem * getFilterItem(const Template::Filter & a_Filter);


protected slots:

	/** Saves the direct UI values into m_Item and closes the dialog. */
	void saveAndClose();

	/** Adds a sibling of the currently selected filter, inserting a combinator parent if needed.
	The combinator parent, if created, is initialized to fkOr.
	The sibling is initialized based on its parent: to a "none pass" filter if fkOr or "all pass" if fkAnd,
	so that it doesn't change the overall filter if not edited. */
	void addFilterSibling();

	/** Replaces the currently selected filter subtree with a fkAnd combinator that has the
	subtree as its first child and an "all pass" filter as its second child. */
	void insertFilterCombinator();

	/** Removes the currently selected filter.
	Propagates the deletion as far as needed so that the filter tree is normalized.
	If the propagation causes the filter to become empty, re-initializes the filter to "all pass". */
	void removeFilter();

	/** Shows a preview of the filter - lists all matching songs. */
	void previewFilter();

	/** Called when the selection in the Filters tree changes. */
	void filterSelectionChanged();

	/** Updates the display of matching songs count display. */
	void updateFilterStats();

	/** The text in the BgColor LineEdit has changed.
	Applies the new color to the LineEdit's background. */
	void bgColorTextChanged(const QString & a_NewText);

	/** Opens the color chooser, sets the selected color to leBgColor. */
	void chooseBgColor();
};





#endif // DLGEDITTEMPLATEITEM_H

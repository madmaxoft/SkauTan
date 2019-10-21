#ifndef DLGEDITTEMPLATEITEM_H
#define DLGEDITTEMPLATEITEM_H





#include <memory>
#include <QDialog>
#include "../../Filter.hpp"





// fwd:
class QTreeWidgetItem;
class ComponentCollection;
namespace Ui
{
	class DlgEditFilter;
}





/** Represents a dialog for editing a single Template's Item, including its filters. */
class DlgEditFilter:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	/** Creates a new dialog instance for editing the specified filter. */
	explicit DlgEditFilter(
		ComponentCollection & aComponents,
		Filter & aFilter,
		QWidget * aParent = nullptr
	);

	virtual ~DlgEditFilter() override;


private:

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The filter being edited. */
	Filter & mFilter;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgEditFilter> mUI;


protected:

	/** Called by Qt when the dialog is closed by the OS's [X] button. */
	virtual void reject() override;

	/** Saves the direct UI values into mItem. */
	void save();

	/** Returns the node that is represented by the currently selected Node tree item.
	Returns nullptr if no selection. */
	Filter::Node * selectedNode() const;

	/** Clears twFilters, then inserts the filter tree into it. */
	void rebuildFilterModel();

	/** Recursively adds children of the specified filter to the specified item. */
	void addNodeChildren(Filter::Node & aParentNode, QTreeWidgetItem & aParentItem);

	/** Creates a QStandardItem representing the specified node. */
	QTreeWidgetItem * createItemFromNode(const Filter::Node & aNode);

	/** Returns a user-visible caption for the specified node. */
	QString getNodeCaption(const Filter::Node & aNode);

	/** Selects the item in twNodes that corresponds to the specified filter node.
	If the node is not found, selection is unchanged. */
	void selectNodeItem(const Filter::Node & aNode);

	/** Returns the twNodes' item representing the specified filter node.
	Returns nullptr if not found. */
	QTreeWidgetItem * getNodeItem(const Filter::Node & aNode);


protected slots:

	/** Saves the direct UI values into mItem and closes the dialog. */
	void saveAndClose();

	/** Adds a sibling of the currently selected node, inserting a combinator parent if needed.
	The combinator parent, if created, is initialized to nkOr.
	The sibling is initialized based on its parent: to a "none pass" filter if nkOr or "all pass" if nkAnd,
	so that it doesn't change the overall filter if not edited. */
	void addNodeSibling();

	/** Replaces the currently selected node (subtree) with a nkAnd combinator that has the
	subtree as its first child and an "all pass" node as its second child. */
	void insertNodeCombinator();

	/** Removes the currently selected filter.
	Propagates the deletion as far as needed so that the filter tree is normalized.
	If the propagation causes the filter to become empty, re-initializes the filter to "all pass". */
	void removeNode();

	/** Shows a preview of the filter - lists all matching songs. */
	void previewFilter();

	/** Called when the selection in the Filters tree changes. */
	void nodeSelectionChanged();

	/** Updates the display of matching songs count display. */
	void updateFilterStats();

	/** The text in the BgColor LineEdit has changed.
	Applies the new color to the LineEdit's background. */
	void bgColorTextChanged(const QString & aNewText);

	/** Opens the color chooser, sets the selected color to leBgColor. */
	void chooseBgColor();

	/** The text in the DurationLimit QLineEdit has been edited.
	Try to parse it and change the QLineEdit's background to indicate success / failure.
	If a valid duration is input, set chbDurationLimit to checked. */
	void durationLimitEdited(const QString & aNewText);
};





#endif // DLGEDITTEMPLATEITEM_H

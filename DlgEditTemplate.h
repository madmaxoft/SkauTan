#ifndef DLGEDITTEMPLATE_H
#define DLGEDITTEMPLATE_H





#include <memory>
#include <QDialog>
#include "Template.h"





// fwd:
class Database;
class MetadataScanner;
class HashCalculator;
class QTableWidgetItem;
namespace Ui
{
	class DlgEditTemplate;
}





class DlgEditTemplate:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgEditTemplate(
		Database & a_DB,
		MetadataScanner & a_Scanner,
		HashCalculator & a_Hasher,
		Template & a_Template,
		QWidget * a_Parent = nullptr
	);

	virtual ~DlgEditTemplate() override;


private:

	/** The database in which the edited template resides. */
	Database & m_DB;

	/** The scanner that can update the songs' metadata upon request.
	Used when displaying songs matching a filter. */
	MetadataScanner & m_MetadataScanner;

	HashCalculator & m_HashCalculator;

	/** The template being edited. */
	Template & m_Template;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgEditTemplate> m_UI;

	/** Set to true if the UI is updating internally; false when the changes come from the user. */
	bool m_IsInternalChange;


protected:

	/** Called by Qt when the dialog is closed by the OS's [X] button. */
	virtual void reject() override;

	/** Saves the direct UI values into m_Template. */
	void save();

	/** Updates the UI at the specified item index with the data from the specified item. */
	void updateTemplateItemRow(int a_Row, const Template::Item & a_Item);


protected slots:

	/** Updates the template's direct properties from the UI elements and closes the dialog. */
	void saveAndClose();

	/** Adds a new item to the template and opens the editor for it. */
	void addItem();

	/** Opens a list of favorite items from all templates, lets user pick one and adds its copy
	to the template. */
	void addFavoriteItem();

	/** Opens the item editor for the currently selected item.
	Ignored if selection is not exactly 1 item. */
	void editSelectedItem();

	/** Removes the selected items from the template. */
	void removeSelectedItems();

	/** Called when the user dbl-clicks an item in the table.
	Opens up the item editor for that item. */
	void cellDoubleClicked(int a_Row, int a_Column);

	/** Called when the selection in tblItems changes.
	Updates the button availability. */
	void itemSelectionChanged();

	/** The text in the BgColor LineEdit has changed.
	Applies the new color to the LineEdit's background. */
	void bgColorTextChanged(const QString & a_NewText);

	/** Opens the color chooser, sets the selected color to leBgColor. */
	void chooseBgColor();

	/** The template item in the table has changed, either by us (m_IsInternalChange) or by the user.
	If the change was by the user, save the new contents to the template item. */
	void itemChanged(QTableWidgetItem * a_Item);
};





#endif // DLGEDITTEMPLATE_H

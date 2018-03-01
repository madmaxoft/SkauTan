#ifndef DLGPICKTEMPLATEFAVORITEITEM_H
#define DLGPICKTEMPLATEFAVORITEITEM_H





#include <memory>
#include <QDialog>
#include "Template.h"





namespace Ui
{
	class DlgPickTemplateFavoriteItem;
}





class DlgPickTemplateFavoriteItem:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	/** Creates a new dialog to pick from the specified items.
	a_Items is moved into m_Items. */
	explicit DlgPickTemplateFavoriteItem(
		std::vector<Template::ItemPtr> && a_Items,
		QWidget * a_Parent = nullptr
	);

	~DlgPickTemplateFavoriteItem();

	/** Returns the item selected by the user. */
	Template::ItemPtr itemSelected() const { return m_ItemSelected; }


private:

	/** The favorite items to pick from. */
	std::vector<Template::ItemPtr> m_Items;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgPickTemplateFavoriteItem> m_UI;

	/** The item that has been selected by the user. */
	Template::ItemPtr m_ItemSelected;


	/** Called for each keypress in the dialog, unless it is handled by the currently focused widget.
	Used to monitor for the Enter and Esc keypresses. */
	virtual void keyPressEvent(QKeyEvent * a_Event) override;
};





#endif // DLGPICKTEMPLATEFAVORITEITEM_H

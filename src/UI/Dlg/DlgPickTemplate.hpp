#ifndef DLGPICKTEMPLATE_H
#define DLGPICKTEMPLATE_H





#include <memory>
#include <QDialog>
#include "../../Template.hpp"





// fwd:
class ComponentCollection;
namespace Ui
{
	class DlgPickTemplate;
}





class DlgPickTemplate:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgPickTemplate(ComponentCollection & aComponents, QWidget * aParent = nullptr);

	virtual ~DlgPickTemplate() override;

	/** Returns the template that the user selected, or nullptr if none selected. */
	TemplatePtr selectedTemplate() const { return mSelectedTemplate; }


private:

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The template that has been selected. */
	TemplatePtr mSelectedTemplate;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgPickTemplate> mUI;


	/** Called for each keypress in the dialog, unless it is handled by the currently focused widget.
	Used to monitor for the Enter and Esc keypresses. */
	virtual void keyPressEvent(QKeyEvent * aEvent) override;


protected slots:

	/** Emitted by tblTemplates when a cell in it is dblclicked.
	aRow and aColumn are the cell coords. */
	void cellDblClicked(int aRow, int aColumn);
};





#endif // DLGPICKTEMPLATE_H

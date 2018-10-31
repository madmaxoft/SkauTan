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

	explicit DlgPickTemplate(ComponentCollection & a_Components, QWidget * a_Parent = nullptr);

	virtual ~DlgPickTemplate() override;

	/** Returns the template that the user selected, or nullptr if none selected. */
	TemplatePtr selectedTemplate() const { return m_SelectedTemplate; }


private:

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The template that has been selected. */
	TemplatePtr m_SelectedTemplate;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgPickTemplate> m_UI;


	/** Called for each keypress in the dialog, unless it is handled by the currently focused widget.
	Used to monitor for the Enter and Esc keypresses. */
	virtual void keyPressEvent(QKeyEvent * a_Event) override;


protected slots:

	/** Emitted by tblTemplates when a cell in it is dblclicked.
	a_Row and a_Column are the cell coords. */
	void cellDblClicked(int a_Row, int a_Column);
};





#endif // DLGPICKTEMPLATE_H

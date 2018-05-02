#ifndef DLGCHOOSEIMPORTTEMPLATES_H
#define DLGCHOOSEIMPORTTEMPLATES_H





#include <memory>
#include <QDialog>
#include "Template.h"





namespace Ui
{
	class DlgChooseImportTemplates;
}





class DlgChooseImportTemplates:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgChooseImportTemplates(std::vector<TemplatePtr> && a_Templates, QWidget * a_Parent = nullptr);

	virtual ~DlgChooseImportTemplates() override;

	/** Returns the templates that the user chose for import. */
	const std::vector<TemplatePtr> & chosenTemplates() const { return m_ChosenTemplates; }


private:

	/** The templates to choose from. */
	std::vector<TemplatePtr> m_Templates;

	/** The chosen templates (after accepting the dialog). */
	std::vector<TemplatePtr> m_ChosenTemplates;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgChooseImportTemplates> m_UI;


	/** Updates the row in m_UI->tblTemplates for the specified template. */
	void updateTemplateRow(int a_Row, const Template & a_Template);


private slots:

	/** The user clicked the Import button.
	Save their choice and close the dialog. */
	void import();

	/** The selection in the template list has changed.
	Updates the list of template items. */
	void templateSelectionChanged();

	/** Enables the Import button based on whether any template is checked. */
	void templateCellChanged(int a_Row, int a_Column);
};





#endif // DLGCHOOSEIMPORTTEMPLATES_H

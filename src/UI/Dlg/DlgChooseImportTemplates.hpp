#pragma once

#include <memory>
#include <QDialog>
#include "../../Template.hpp"





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

	explicit DlgChooseImportTemplates(std::vector<TemplatePtr> && aTemplates, QWidget * aParent = nullptr);

	virtual ~DlgChooseImportTemplates() override;

	/** Returns the templates that the user chose for import. */
	const std::vector<TemplatePtr> & chosenTemplates() const { return mChosenTemplates; }


private:

	/** The templates to choose from. */
	std::vector<TemplatePtr> mTemplates;

	/** The chosen templates (after accepting the dialog). */
	std::vector<TemplatePtr> mChosenTemplates;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgChooseImportTemplates> mUI;


	/** Updates the row in mUI->tblTemplates for the specified template. */
	void updateTemplateRow(int aRow, const Template & aTemplate);


private slots:

	/** The user clicked the Import button.
	Save their choice and close the dialog. */
	void import();

	/** The selection in the template list has changed.
	Updates the list of template items. */
	void templateSelectionChanged();

	/** Enables the Import button based on whether any template is checked. */
	void templateCellChanged(int aRow, int aColumn);
};

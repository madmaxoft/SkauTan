#include "DlgTemplatesList.h"
#include <assert.h>
#include <QMessageBox>
#include "ui_DlgTemplatesList.h"
#include "DlgEditTemplate.h"
#include "Database.h"





DlgTemplatesList::DlgTemplatesList(Database & a_DB, QWidget * a_Parent):
	Super(a_Parent),
	m_DB(a_DB),
	m_UI(new Ui::DlgTemplatesList)
{
	m_UI->setupUi(this);

	// Connect the signals:
	connect(m_UI->btnClose,          &QPushButton::clicked,               this, &DlgTemplatesList::close);
	connect(m_UI->btnAddTemplate,    &QPushButton::clicked,               this, &DlgTemplatesList::addTemplate);
	connect(m_UI->btnEditTemplate,   &QPushButton::clicked,               this, &DlgTemplatesList::editTemplate);
	connect(m_UI->btnRemoveTemplate, &QPushButton::clicked,               this, &DlgTemplatesList::removeTemplates);
	connect(m_UI->tblTemplates,      &QTableWidget::doubleClicked,        this, &DlgTemplatesList::editTemplateAt);
	connect(m_UI->tblTemplates,      &QTableWidget::currentCellChanged,   this, &DlgTemplatesList::templateSelected);
	connect(m_UI->tblTemplates,      &QTableWidget::itemSelectionChanged, this, &DlgTemplatesList::templateSelectionChanged);

	// Update the UI state:
	const auto & templates = m_DB.templates();
	m_UI->tblTemplates->setRowCount(static_cast<int>(templates.size()));
	m_UI->tblTemplates->setColumnCount(3);
	m_UI->tblTemplates->setHorizontalHeaderLabels({tr("Name"), tr("#"), tr("Notes")});
	m_UI->tblItems->setRowCount(0);
	m_UI->tblItems->setColumnCount(4);
	m_UI->tblItems->setHorizontalHeaderLabels({tr("Name"), tr("Notes"), tr("Fav"), tr("Filter")});
	templateSelectionChanged();

	// Insert all the templates into the table view:
	for (size_t i = 0; i < templates.size(); ++i)
	{
		const auto & tmpl = templates[i];
		updateTemplateRow(static_cast<int>(i), *tmpl);
	}
	m_UI->tblTemplates->resizeColumnsToContents();
}





DlgTemplatesList::~DlgTemplatesList()
{
	// Nothing explicit needed
}





TemplatePtr DlgTemplatesList::templateFromRow(int a_Row)
{
	if (a_Row < 0)
	{
		assert(!"Invalid row");
		return nullptr;
	}
	auto row = static_cast<size_t>(a_Row);
	if (row >= m_DB.templates().size())
	{
		assert(!"Invalid row");
		return nullptr;
	}
	return m_DB.templates()[row];
}





void DlgTemplatesList::updateTemplateRow(int a_Row, const Template & a_Template)
{
	auto numItems = QString::number(a_Template.items().size());
	m_UI->tblTemplates->setItem(a_Row, 0, new QTableWidgetItem(a_Template.displayName()));
	m_UI->tblTemplates->setItem(a_Row, 1, new QTableWidgetItem(numItems));
	m_UI->tblTemplates->setItem(a_Row, 2, new QTableWidgetItem(a_Template.notes()));
}





void DlgTemplatesList::addTemplate()
{
	auto tmpl = m_DB.addTemplate();
	if (tmpl == nullptr)
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot add template"),  // title
			tr("Cannot add template possibly DB failure? Inspect the log.")
		);
		return;
	}
	DlgEditTemplate dlg(m_DB, *tmpl, this);
	dlg.exec();
	m_DB.saveTemplate(*tmpl);

	// Update the UI:
	int idx = static_cast<int>(m_DB.templates().size()) - 1;
	m_UI->tblTemplates->setRowCount(idx + 1);
	updateTemplateRow(idx, *tmpl);
}





void DlgTemplatesList::editTemplate()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		// No selection, or too many selected
		return;
	}
	editTemplateAt(selection[0]);
}





void DlgTemplatesList::removeTemplates()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	if (selection.isEmpty())
	{
		return;
	}

	// Ask the user if they're sure:
	if (QMessageBox::Yes != QMessageBox::question(
		this,  // parent
		tr("SkauTan: Remove templates?"),
		tr("Are you sure you want to remove the selected templates?"),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	))
	{
		return;
	}

	// Delete both from the UI and from the DB:
	for (const auto & row: selection)
	{
		m_DB.delTemplate(templateFromRow(row.row()).get());
		m_UI->tblTemplates->removeRow(row.row());
	}
}






void DlgTemplatesList::editTemplateAt(const QModelIndex & a_Index)
{
	auto tmpl = templateFromRow(a_Index.row());
	if (tmpl == nullptr)
	{
		return;
	}
	DlgEditTemplate dlg(m_DB, *tmpl, this);
	dlg.exec();
	m_DB.saveTemplate(*tmpl);

	// Update the UI after the changes:
	updateTemplateRow(a_Index.row(), *tmpl);
	templateSelectionChanged();
}





void DlgTemplatesList::templateSelected(int a_CurrentRow, int a_CurrentColumn)
{
	Q_UNUSED(a_CurrentColumn);
	auto tmpl = templateFromRow(a_CurrentRow);
	m_UI->tblItems->clearContents();
	if (tmpl == nullptr)
	{
		return;
	}
	// The template items are filled in the templateSelectionChanged handler
}





void DlgTemplatesList::templateSelectionChanged()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	m_UI->btnEditTemplate->setEnabled(selection.size() == 1);
	m_UI->btnRemoveTemplate->setEnabled(selection.size() > 0);

	// Update the list of items in a template:
	if (selection.size() == 1)
	{
		const auto & tmpl = templateFromRow(selection[0].row());
		m_UI->tblItems->setRowCount(static_cast<int>(tmpl->items().size()));
		int idx = 0;
		for (const auto & item: tmpl->items())
		{
			auto favDesc = item->isFavorite() ? tr("Y", "IsFavorite") : QString();
			auto filterDesc = item->filter()->getDescription();
			m_UI->tblItems->setItem(idx, 0, new QTableWidgetItem(item->displayName()));
			m_UI->tblItems->setItem(idx, 1, new QTableWidgetItem(item->notes()));
			m_UI->tblItems->setItem(idx, 2, new QTableWidgetItem(favDesc));
			m_UI->tblItems->setItem(idx, 3, new QTableWidgetItem(filterDesc));
			idx += 1;
		}
		m_UI->tblItems->resizeColumnsToContents();
	}
	else
	{
		m_UI->tblItems->setRowCount(0);
	}
}

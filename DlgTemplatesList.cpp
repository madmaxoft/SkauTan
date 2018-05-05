#include "DlgTemplatesList.h"
#include <assert.h>
#include <QMessageBox>
#include <QFileDialog>
#include <QDomDocument>
#include "ui_DlgTemplatesList.h"
#include "DlgEditTemplate.h"
#include "DlgChooseImportTemplates.h"
#include "Database.h"





DlgTemplatesList::DlgTemplatesList(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
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
	connect(m_UI->btnExport,         &QPushButton::clicked,               this, &DlgTemplatesList::exportTemplates);
	connect(m_UI->btnImport,         &QPushButton::clicked,               this, &DlgTemplatesList::importTemplates);

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
	auto colCount = m_UI->tblTemplates->columnCount();
	for (int col = 0; col < colCount; ++col)
	{
		m_UI->tblTemplates->item(a_Row, col)->setBackgroundColor(a_Template.bgColor());
	}
}





void DlgTemplatesList::exportTemplatesTo(const QString & a_FileName)
{
	QFile f(a_FileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Export templates"),
			tr("SkauTan could not export templates to the specified file, because the file cannot be written to.")
		);
		return;
	}
	std::vector<TemplatePtr> templates;
	for (const auto & row: m_UI->tblTemplates->selectionModel()->selectedRows())
	{
		templates.push_back(m_DB.templates()[static_cast<size_t>(row.row())]);
	}
	f.write(TemplateXmlExport::run(templates));
	f.close();
}





void DlgTemplatesList::importTemplatesFrom(const QString & a_FileName)
{
	QFile f(a_FileName);
	if (!f.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Import templates"),
			tr("SkauTan could not import templates from \"%1\", because the file cannot be read from.").arg(a_FileName)
		);
		return;
	}
	auto templates = TemplateXmlImport::run(f.readAll());
	if (templates.empty())
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Import templates"),
			tr("There are not SkauTan templates in \"%1\".").arg(a_FileName)
		);
		return;
	}

	// Ask the user which templates to import:
	DlgChooseImportTemplates dlg(std::move(templates), this);
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}

	// Import the chosen templates:
	for (const auto & tmpl: dlg.chosenTemplates())
	{
		m_DB.addTemplate(tmpl);
		m_DB.saveTemplate(*tmpl);
	}

	// Insert the imported templates into the UI:
	m_UI->tblTemplates->setRowCount(static_cast<int>(m_DB.templates().size()));
	int row = 0;
	for (const auto & tmpl: m_DB.templates())
	{
		updateTemplateRow(row, *tmpl);
		row += 1;
	}
}





void DlgTemplatesList::addTemplate()
{
	auto tmpl = m_DB.createTemplate();
	if (tmpl == nullptr)
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot add template"),
			tr("Cannot add template, possibly due to DB failure? Inspect the log.")
		);
		return;
	}
	DlgEditTemplate dlg(m_DB, m_MetadataScanner, *tmpl, this);
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
	DlgEditTemplate dlg(m_DB, m_MetadataScanner, *tmpl, this);
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
	m_UI->btnRemoveTemplate->setEnabled(!selection.isEmpty());
	m_UI->btnExport->setEnabled(!selection.isEmpty());

	// Update the list of items in a template:
	if (selection.size() == 1)
	{
		const auto & tmpl = templateFromRow(selection[0].row());
		m_UI->tblItems->setRowCount(static_cast<int>(tmpl->items().size()));
		int idx = 0;
		auto colCount = m_UI->tblItems->columnCount();
		for (const auto & item: tmpl->items())
		{
			auto favDesc = item->isFavorite() ? tr("Y", "IsFavorite") : QString();
			auto filterDesc = item->filter()->getDescription();
			m_UI->tblItems->setItem(idx, 0, new QTableWidgetItem(item->displayName()));
			m_UI->tblItems->setItem(idx, 1, new QTableWidgetItem(item->notes()));
			m_UI->tblItems->setItem(idx, 2, new QTableWidgetItem(favDesc));
			m_UI->tblItems->setItem(idx, 3, new QTableWidgetItem(filterDesc));
			for (int col = 0; col < colCount; ++col)
			{
				m_UI->tblItems->item(idx, col)->setBackgroundColor(item->bgColor());
			}
			idx += 1;
		}
		m_UI->tblItems->resizeColumnsToContents();
	}
	else
	{
		m_UI->tblItems->setRowCount(0);
	}
}





void DlgTemplatesList::exportTemplates()
{
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Export templates"),
		{},
		tr("SkauTan templates (*.SkauTanTemplates)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	exportTemplatesTo(fileName);
	QMessageBox::information(
		this,
		tr("SkauTan: Export complete"),
		tr("Templates have been exported to \"%1\".").arg(fileName)
	);
}





void DlgTemplatesList::importTemplates()
{
	auto fileName = QFileDialog::getOpenFileName(
		this,
		tr("SkauTan: Import templates"),
		{},
		tr("SkauTan templates (*.SkauTanTemplates)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	importTemplatesFrom(fileName);
}

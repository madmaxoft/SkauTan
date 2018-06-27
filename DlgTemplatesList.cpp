#include "DlgTemplatesList.h"
#include <assert.h>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include "ui_DlgTemplatesList.h"
#include "DlgEditTemplate.h"
#include "DlgChooseImportTemplates.h"
#include "Database.h"
#include "DlgEditTemplateItem.h"
#include "Settings.h"
#include "Utils.h"
#include "ComponentCollection.h"





DlgTemplatesList::DlgTemplatesList(
	ComponentCollection & a_Components,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_UI(new Ui::DlgTemplatesList),
	m_Components(a_Components),
	m_IsInternalChange(false)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgTemplatesList", *this);

	// Connect the signals:
	connect(m_UI->btnClose,          &QPushButton::clicked,               this, &DlgTemplatesList::close);
	connect(m_UI->btnAddTemplate,    &QPushButton::clicked,               this, &DlgTemplatesList::addTemplate);
	connect(m_UI->btnEditTemplate,   &QPushButton::clicked,               this, &DlgTemplatesList::editTemplate);
	connect(m_UI->btnRemoveTemplate, &QPushButton::clicked,               this, &DlgTemplatesList::removeTemplates);
	connect(m_UI->tblTemplates,      &QTableWidget::doubleClicked,        this, &DlgTemplatesList::editTemplateAt);
	connect(m_UI->tblTemplates,      &QTableWidget::itemSelectionChanged, this, &DlgTemplatesList::templateSelectionChanged);
	connect(m_UI->tblTemplates,      &QTableWidget::itemChanged,          this, &DlgTemplatesList::templateChanged);
	connect(m_UI->tblItems,          &QTableWidget::itemChanged,          this, &DlgTemplatesList::itemChanged);
	connect(m_UI->tblItems,          &QTableWidget::doubleClicked,        this, &DlgTemplatesList::editTemplateItemAt);
	connect(m_UI->btnExport,         &QPushButton::clicked,               this, &DlgTemplatesList::exportTemplates);
	connect(m_UI->btnImport,         &QPushButton::clicked,               this, &DlgTemplatesList::importTemplates);

	// Update the UI state:
	const auto & templates = m_Components.get<Database>()->templates();
	m_UI->tblTemplates->setRowCount(static_cast<int>(templates.size()));
	m_UI->tblTemplates->setColumnCount(3);
	m_UI->tblTemplates->setHorizontalHeaderLabels({tr("Name"), tr("#"), tr("Notes")});
	m_UI->tblItems->setRowCount(0);
	m_UI->tblItems->setColumnCount(6);
	m_UI->tblItems->setHorizontalHeaderLabels({
		tr("Name"),
		tr("Notes"),
		tr("Fav", "Favorite"),
		tr("Dur", "Duration"),
		tr("# Songs", "Number of matching songs"),
		tr("Filter")
	});
	templateSelectionChanged();

	// Insert all the templates into the table view:
	for (size_t i = 0; i < templates.size(); ++i)
	{
		const auto & tmpl = templates[i];
		updateTemplateRow(static_cast<int>(i), *tmpl);
	}

	Settings::loadHeaderView("DlgTemplatesList", "tblTemplates", *m_UI->tblTemplates->horizontalHeader());
	Settings::loadHeaderView("DlgTemplatesList", "tblItems",     *m_UI->tblItems->horizontalHeader());
}





DlgTemplatesList::~DlgTemplatesList()
{
	Settings::saveHeaderView("DlgTemplatesList", "tblTemplates", *m_UI->tblTemplates->horizontalHeader());
	Settings::saveHeaderView("DlgTemplatesList", "tblItems",     *m_UI->tblItems->horizontalHeader());
	Settings::saveWindowPos("DlgTemplatesList", *this);
}





TemplatePtr DlgTemplatesList::templateFromRow(int a_Row)
{
	if (a_Row < 0)
	{
		assert(!"Invalid row");
		return nullptr;
	}
	auto row = static_cast<size_t>(a_Row);
	if (row >= m_Components.get<Database>()->templates().size())
	{
		assert(!"Invalid row");
		return nullptr;
	}
	return m_Components.get<Database>()->templates()[row];
}





Template::ItemPtr DlgTemplatesList::templateItemFromRow(TemplatePtr a_Template, int a_ItemRow)
{
	assert(a_Template != nullptr);
	if (a_ItemRow < 0)
	{
		assert(!"Invalid row");
		return nullptr;
	}
	auto row = static_cast<size_t>(a_ItemRow);
	if (row >= a_Template->items().size())
	{
		assert(!"Invalid row");
		return nullptr;
	}
	return a_Template->items()[row];
}





void DlgTemplatesList::updateTemplateRow(int a_Row, const Template & a_Template)
{
	m_IsInternalChange = true;
	auto item = new QTableWidgetItem(a_Template.displayName());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(a_Template.bgColor());
	m_UI->tblTemplates->setItem(a_Row, 0, item);

	auto numItems = QString::number(a_Template.items().size());
	item = new QTableWidgetItem(numItems);
	item->setBackgroundColor(a_Template.bgColor());
	m_UI->tblTemplates->setItem(a_Row, 1, item);

	item = new QTableWidgetItem(a_Template.notes());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(a_Template.bgColor());
	m_UI->tblTemplates->setItem(a_Row, 2, item);
	m_IsInternalChange = false;
}





void DlgTemplatesList::updateTemplateItemRow(int a_Row, const Template::Item & a_Item)
{
	m_IsInternalChange = true;
	auto wi = new QTableWidgetItem(a_Item.displayName());
	wi->setFlags(wi->flags() | Qt::ItemIsEditable);
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 0, wi);

	wi = new QTableWidgetItem(a_Item.notes());
	wi->setFlags(wi->flags() | Qt::ItemIsEditable);
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 1, wi);

	wi = new QTableWidgetItem();
	wi->setFlags(wi->flags() | Qt::ItemIsUserCheckable);
	wi->setCheckState(a_Item.isFavorite() ? Qt::Checked : Qt::Unchecked);
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 2, wi);

	const auto & durationLimit = a_Item.durationLimit();
	wi = new QTableWidgetItem(durationLimit.isPresent() ? Utils::formatTime(durationLimit.value()) : "");
	wi->setBackgroundColor(a_Item.bgColor());
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_UI->tblItems->setItem(a_Row, 3, wi);

	auto numMatching = m_Components.get<Database>()->numSongsMatchingFilter(*a_Item.filter());
	wi = new QTableWidgetItem(QString::number(numMatching));
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor((numMatching > 0) ? a_Item.bgColor() : QColor(255, 192, 192));
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_UI->tblItems->setItem(a_Row, 4, wi);

	wi = new QTableWidgetItem(a_Item.filter()->getDescription());
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 5, wi);
	m_IsInternalChange = false;
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
		templates.push_back(m_Components.get<Database>()->templates()[static_cast<size_t>(row.row())]);
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
			tr("There are no SkauTan templates in \"%1\".").arg(a_FileName)
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
	auto db = m_Components.get<Database>();
	for (const auto & tmpl: dlg.chosenTemplates())
	{
		db->addTemplate(tmpl);
		db->saveTemplate(*tmpl);
	}

	// Insert the imported templates into the UI:
	m_UI->tblTemplates->setRowCount(static_cast<int>(db->templates().size()));
	int row = 0;
	for (const auto & tmpl: db->templates())
	{
		updateTemplateRow(row, *tmpl);
		row += 1;
	}
}





void DlgTemplatesList::addTemplate()
{
	auto tmpl = m_Components.get<Database>()->createTemplate();
	if (tmpl == nullptr)
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot add template"),
			tr("Cannot add template, possibly due to DB failure? Inspect the log.")
		);
		return;
	}
	DlgEditTemplate dlg(m_Components, *tmpl, this);
	dlg.exec();
	m_Components.get<Database>()->saveTemplate(*tmpl);

	// Update the UI:
	int idx = static_cast<int>(m_Components.get<Database>()->templates().size()) - 1;
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

	// Delete both from the UI and from the DB (starting from the last row):
	auto rowsToRemove = selection;
	std::sort(rowsToRemove.begin(), rowsToRemove.end(), [](const QModelIndex & a_Row1, const QModelIndex & a_Row2)
		{
			return (a_Row1.row() > a_Row2.row());
		}
	);
	auto db = m_Components.get<Database>();
	for (const auto & row: rowsToRemove)
	{
		db->delTemplate(templateFromRow(row.row()).get());
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
	DlgEditTemplate dlg(m_Components, *tmpl, this);
	dlg.exec();
	m_Components.get<Database>()->saveTemplate(*tmpl);

	// Update the UI after the changes:
	updateTemplateRow(a_Index.row(), *tmpl);
	templateSelectionChanged();
}





void DlgTemplatesList::templateSelectionChanged()
{
	m_IsInternalChange = true;
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
		for (const auto & item: tmpl->items())
		{
			updateTemplateItemRow(idx, *item);
			idx += 1;
		}
	}
	else
	{
		m_UI->tblItems->setRowCount(0);
	}
	m_IsInternalChange = false;
}





void DlgTemplatesList::templateChanged(QTableWidgetItem * a_Item)
{
	if (m_IsInternalChange)
	{
		return;
	}
	auto tmpl = templateFromRow(a_Item->row());
	if (tmpl == nullptr)
	{
		qWarning() << "Bad template pointer in item " << a_Item->text();
		return;
	}
	switch (a_Item->column())
	{
		case 0:
		{
			tmpl->setDisplayName(a_Item->text());
			break;
		}
		case 2:
		{
			tmpl->setNotes(a_Item->text());
			break;
		}
	}
	m_Components.get<Database>()->saveTemplate(*tmpl);
}





void DlgTemplatesList::itemChanged(QTableWidgetItem * a_Item)
{
	if (m_IsInternalChange)
	{
		return;
	}
	auto tmpl = templateFromRow(m_UI->tblTemplates->currentRow());
	if (tmpl == nullptr)
	{
		qWarning() << "Bad template pointer.";
		return;
	}
	auto item = templateItemFromRow(tmpl, a_Item->row());
	if (item == nullptr)
	{
		qWarning() << "Bad template item pointer in item " << a_Item->text();
		return;
	}
	switch (a_Item->column())
	{
		case 0:
		{
			item->setDisplayName(a_Item->text());
			break;
		}
		case 1:
		{
			item->setNotes(a_Item->text());
			break;
		}
		case 2:
		{
			item->setIsFavorite(a_Item->checkState() == Qt::Checked);
			break;
		}
		case 3:
		{
			bool isOK;
			auto durationLimit = Utils::parseTime(a_Item->text(), isOK);
			if (isOK)
			{
				item->setDurationLimit(durationLimit);
				m_IsInternalChange = true;
				a_Item->setText(Utils::formatTime(durationLimit));
				m_IsInternalChange = false;
			}
			else
			{
				item->resetDurationLimit();
				m_IsInternalChange = true;
				a_Item->setText({});
				m_IsInternalChange = false;
			}
			break;
		}
	}
	m_Components.get<Database>()->saveTemplate(*tmpl);
}





void DlgTemplatesList::editTemplateItemAt(const QModelIndex & a_Index)
{
	auto tmpl = templateFromRow(m_UI->tblTemplates->currentRow());
	if (tmpl == nullptr)
	{
		qWarning() << "Bad template pointer.";
		return;
	}
	auto item = templateItemFromRow(tmpl, a_Index.row());
	if (item == nullptr)
	{
		qWarning() << "Bad template item pointer in item " << a_Index;
		return;
	}

	DlgEditTemplateItem dlg(m_Components, *item, this);
	dlg.exec();
	m_Components.get<Database>()->saveTemplate(*tmpl);
	updateTemplateItemRow(a_Index.row(), *item);
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

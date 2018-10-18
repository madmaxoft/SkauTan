#include "DlgTemplatesList.h"
#include <assert.h>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QMenu>
#include "ui_DlgTemplatesList.h"
#include "DlgChooseImportTemplates.h"
#include "Database.h"
#include "Settings.h"
#include "Utils.h"
#include "ComponentCollection.h"
#include "Filter.h"
#include "DlgManageFilters.h"
#include "DlgEditFilter.h"





// TODO: Handle template colors





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
	connect(m_UI->btnClose,             &QPushButton::clicked,               this, &DlgTemplatesList::close);
	connect(m_UI->btnAddTemplate,       &QPushButton::clicked,               this, &DlgTemplatesList::addTemplate);
	connect(m_UI->btnRemoveTemplate,    &QPushButton::clicked,               this, &DlgTemplatesList::removeTemplates);
	connect(m_UI->btnMoveTemplatesUp,   &QPushButton::clicked,               this, &DlgTemplatesList::moveTemplateUp);
	connect(m_UI->btnMoveTemplatesDown, &QPushButton::clicked,               this, &DlgTemplatesList::moveTemplateDown);
	connect(m_UI->btnImport,            &QPushButton::clicked,               this, &DlgTemplatesList::importTemplates);
	connect(m_UI->btnExport,            &QPushButton::clicked,               this, &DlgTemplatesList::exportTemplates);
	connect(m_UI->btnRemoveItem,        &QPushButton::clicked,               this, &DlgTemplatesList::removeItems);
	connect(m_UI->btnMoveItemUp,        &QPushButton::clicked,               this, &DlgTemplatesList::moveItemUp);
	connect(m_UI->btnMoveItemDown,      &QPushButton::clicked,               this, &DlgTemplatesList::moveItemDown);
	connect(m_UI->btnManageFilters,     &QPushButton::clicked,               this, &DlgTemplatesList::manageFilters);
	connect(m_UI->tblTemplates,         &QTableWidget::itemSelectionChanged, this, &DlgTemplatesList::templateSelectionChanged);
	connect(m_UI->tblTemplates,         &QTableWidget::itemChanged,          this, &DlgTemplatesList::templateChanged);
	connect(m_UI->tblItems,             &QTableWidget::itemSelectionChanged, this, &DlgTemplatesList::itemSelectionChanged);
	connect(m_UI->tblItems,             &QTableWidget::cellDoubleClicked,    this, &DlgTemplatesList::itemDoubleClicked);
	// TODO: React to in-place editing

	// Update the UI state:
	const auto & templates = m_Components.get<Database>()->templates();
	m_UI->tblTemplates->setRowCount(static_cast<int>(templates.size()));
	m_UI->tblItems->setRowCount(0);
	templateSelectionChanged();

	// Insert all the templates into the table view:
	for (size_t i = 0; i < templates.size(); ++i)
	{
		const auto & tmpl = templates[i];
		updateTemplateRow(static_cast<int>(i), *tmpl);
	}

	// Insert all the filters into the Add menu:
	updateFilterMenu();

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





void DlgTemplatesList::updateTemplateRow(int a_Row, const Template & a_Template)
{
	m_IsInternalChange = true;
	auto item = new QTableWidgetItem(a_Template.displayName());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(a_Template.bgColor());
	m_UI->tblTemplates->setItem(a_Row, 0, item);

	auto numItems = QString::number(a_Template.items().size());
	item = new QTableWidgetItem(numItems);
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	item->setBackgroundColor(a_Template.bgColor());
	m_UI->tblTemplates->setItem(a_Row, 1, item);

	item = new QTableWidgetItem(a_Template.notes());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(a_Template.bgColor());
	m_UI->tblTemplates->setItem(a_Row, 2, item);
	m_IsInternalChange = false;
}





void DlgTemplatesList::updateTemplateItemRow(int a_Row, const Filter & a_Item)
{
	m_IsInternalChange = true;
	auto wi = new QTableWidgetItem(a_Item.displayName());
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 0, wi);

	wi = new QTableWidgetItem(a_Item.notes());
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 1, wi);

	const auto & durationLimit = a_Item.durationLimit();
	wi = new QTableWidgetItem(durationLimit.isPresent() ? Utils::formatTime(durationLimit.value()) : "");
	wi->setBackgroundColor(a_Item.bgColor());
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_UI->tblItems->setItem(a_Row, 2, wi);

	auto numMatching = m_Components.get<Database>()->numSongsMatchingFilter(a_Item);
	wi = new QTableWidgetItem(QString::number(numMatching));
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor((numMatching > 0) ? a_Item.bgColor() : QColor(255, 192, 192));
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_UI->tblItems->setItem(a_Row, 3, wi);

	wi = new QTableWidgetItem(a_Item.getFilterDescription());
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 4, wi);
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





void DlgTemplatesList::updateFilterMenu()
{
	auto menu = new QMenu(this);
	auto size = menu->style()->pixelMetric(QStyle::PM_SmallIconSize);
	for (const auto & filter: m_Components.get<Database>()->filters())
	{
		auto action = new QAction(filter->displayName(), this);
		QPixmap pixmap(size, size);
		pixmap.fill(filter->bgColor());
		action->setIcon(QIcon(pixmap));
		menu->addAction(action);
		connect(action, &QAction::triggered, [this, filter]() { this->insertFilter(filter); });
	}
	auto oldMenu = m_UI->btnAddItem->menu();
	m_UI->btnAddItem->setMenu(menu);
	oldMenu->deleteLater();
}





void DlgTemplatesList::insertFilter(FilterPtr a_Filter)
{
	auto tmpl = currentTemplate();
	if (tmpl == nullptr)
	{
		return;
	}

	const auto & selection = m_UI->tblItems->selectionModel()->selectedRows();
	int idxInsertBefore;
	if (selection.isEmpty())
	{
		idxInsertBefore = m_UI->tblItems->rowCount();
	}
	else
	{
		idxInsertBefore = selection.last().row();
	}

	tmpl->insertItem(a_Filter, idxInsertBefore);
	m_UI->tblItems->insertRow(idxInsertBefore);
	updateTemplateItemRow(idxInsertBefore, *a_Filter);
	m_UI->tblItems->selectRow(idxInsertBefore);
	m_Components.get<Database>()->saveTemplate(*tmpl);
}





TemplatePtr DlgTemplatesList::currentTemplate()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return nullptr;
	}
	return templateFromRow(selection[0].row());
}





void DlgTemplatesList::swapTemplatesAndSelectSecond(int a_Row1, int a_Row2)
{
	auto db = m_Components.get<Database>();
	db->swapTemplatesByIdx(static_cast<size_t>(a_Row1), static_cast<size_t>(a_Row2));
	const auto & templates = db->templates();
	updateTemplateRow(a_Row1, *templates[static_cast<size_t>(a_Row1)]);
	updateTemplateRow(a_Row2, *templates[static_cast<size_t>(a_Row2)]);
	m_UI->tblTemplates->selectRow(a_Row2);
}





void DlgTemplatesList::swapItemsAndSelectSecond(int a_Row1, int a_Row2)
{
	auto tmpl = currentTemplate();
	if (tmpl == nullptr)
	{
		assert(!"Invalid template");
		return;
	}
	tmpl->swapItemsByIdx(static_cast<size_t>(a_Row1), static_cast<size_t>(a_Row2));
	m_Components.get<Database>()->saveTemplate(*tmpl);
	const auto & items = tmpl->items();
	updateTemplateItemRow(a_Row1, *items[static_cast<size_t>(a_Row1)]);
	updateTemplateItemRow(a_Row2, *items[static_cast<size_t>(a_Row2)]);
	m_UI->tblItems->selectRow(a_Row2);
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

	// Update the UI:
	int idx = static_cast<int>(m_Components.get<Database>()->templates().size()) - 1;
	m_UI->tblTemplates->setRowCount(idx + 1);
	updateTemplateRow(idx, *tmpl);
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





void DlgTemplatesList::moveTemplateUp()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row < 1)
	{
		return;
	}
	swapTemplatesAndSelectSecond(row, row - 1);
}





void DlgTemplatesList::moveTemplateDown()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= m_UI->tblTemplates->rowCount())
	{
		return;
	}
	swapTemplatesAndSelectSecond(row, row + 1);
}






void DlgTemplatesList::templateSelectionChanged()
{
	m_IsInternalChange = true;
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();
	auto selSize = selection.size();
	m_UI->btnRemoveTemplate->setEnabled(!selection.isEmpty());
	m_UI->btnExport->setEnabled(!selection.isEmpty());
	m_UI->btnAddItem->setEnabled(selSize == 1);

	// Update the up / down controls:
	// Update the list of items in a template:
	if (selSize == 1)
	{
		const auto & tmpl = templateFromRow(selection[0].row());
		m_UI->tblItems->setRowCount(static_cast<int>(tmpl->items().size()));
		int idx = 0;
		for (const auto & item: tmpl->items())
		{
			updateTemplateItemRow(idx, *item);
			idx += 1;
		}
		auto numTemplates = static_cast<int>(m_Components.get<Database>()->templates().size());
		m_UI->btnMoveTemplatesDown->setEnabled(selection[0].row() + 1 < numTemplates);
		m_UI->btnMoveTemplatesUp->setEnabled(selection[0].row() > 0);
	}
	else
	{
		m_UI->btnMoveTemplatesDown->setEnabled(false);
		m_UI->btnMoveTemplatesUp->setEnabled(false);
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





void DlgTemplatesList::itemSelectionChanged()
{
	const auto & sel = m_UI->tblItems->selectionModel()->selectedRows();
	m_UI->btnRemoveItem->setEnabled(!sel.isEmpty());

	if (sel.size() == 1)
	{
		auto row = sel[0].row();
		m_UI->btnMoveItemUp->setEnabled(row > 0);
		m_UI->btnMoveItemDown->setEnabled(row + 1 < m_UI->tblItems->rowCount());
	}
	else
	{
		m_UI->btnMoveItemUp->setEnabled(false);
		m_UI->btnMoveItemDown->setEnabled(false);
	}
}





void DlgTemplatesList::itemDoubleClicked(int a_Row, int a_Column)
{
	Q_UNUSED(a_Column);

	// Get the filter represented by the table cell:
	const auto & selTemplate = currentTemplate();
	if (selTemplate == nullptr)
	{
		assert(!"Template item is visible with no selected template");
		return;
	}
	if ((a_Row < 0) || (static_cast<size_t>(a_Row) >= selTemplate->items().size()))
	{
		assert(!"Invalid template item index");
		return;
	}
	auto item = selTemplate->items()[static_cast<size_t>(a_Row)];

	// Edit the filter:
	DlgEditFilter dlg(m_Components, *item);
	dlg.exec();

	// Update the UI that may have been affected by a filter edit:
	updateTemplateItemRow(a_Row, *item);
	updateFilterMenu();
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




void DlgTemplatesList::manageFilters()
{
	DlgManageFilters dlg(m_Components, this);
	dlg.exec();

	// Update the filters in the "Add" popup menu:
	updateFilterMenu();

	// Update the item counts in the template list (in case a filter was deleted):
	const auto & templates = m_Components.get<Database>()->templates();
	auto num = templates.size();
	for (size_t i = 0; i < num; ++i)
	{
		updateTemplateRow(static_cast<int>(i), *templates[i]);
	}

	// Update the current template item list (in case a filter was deleted):
	templateSelectionChanged();
}





void DlgTemplatesList::removeItems()
{
	const auto & selection = m_UI->tblItems->selectionModel()->selectedRows();
	if (selection.isEmpty())
	{
		return;
	}

	// Ask the user if they're sure:
	if (QMessageBox::Yes != QMessageBox::question(
		this,  // parent
		tr("SkauTan: Remove template items?"),
		tr("Are you sure you want to remove the selected template items?"),
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
	auto tmpl = currentTemplate();
	for (const auto & row: rowsToRemove)
	{
		tmpl->delItem(row.row());
		m_UI->tblItems->removeRow(row.row());
	}
	m_Components.get<Database>()->saveTemplate(*tmpl);
}




void DlgTemplatesList::moveItemUp()
{
	const auto & selection = m_UI->tblItems->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row < 1)
	{
		return;
	}
	swapItemsAndSelectSecond(row, row - 1);
}





void DlgTemplatesList::moveItemDown()
{
	const auto & selection = m_UI->tblItems->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= m_UI->tblItems->rowCount())
	{
		return;
	}
	swapItemsAndSelectSecond(row, row + 1);
}







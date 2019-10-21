#include "DlgTemplatesList.hpp"
#include <cassert>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QMenu>
#include "ui_DlgTemplatesList.h"
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "../../Utils.hpp"
#include "../../ComponentCollection.hpp"
#include "../../Filter.hpp"
#include "../ColorDelegate.hpp"
#include "DlgManageFilters.hpp"
#include "DlgEditFilter.hpp"
#include "DlgChooseImportTemplates.hpp"





/** The columns in tblTemplates. */
enum
{
	colTemplateDisplayName = 0,
	colTemplateNumItems,
	colTemplateBgColor,
	colTemplateNotes,
};



/** The columns in tblItems. */
enum
{
	colItemDisplayName = 0,
	colItemNotes,
	colItemDurationLimit,
	colItemBgColor,
	colItemNumSongs,
	colItemDescription,
};





DlgTemplatesList::DlgTemplatesList(
	ComponentCollection & aComponents,
	QWidget * aParent
):
	Super(aParent),
	mUI(new Ui::DlgTemplatesList),
	mComponents(aComponents),
	mIsInternalChange(false)
{
	mUI->setupUi(this);
	auto delegate = new ColorDelegate(tr("SkauTan: Choose template color"));
	mUI->tblTemplates->setItemDelegateForColumn(colTemplateBgColor, delegate);
	delegate = new ColorDelegate(tr("SkauTan: Choose filter color"));
	mUI->tblItems->setItemDelegateForColumn(colItemBgColor, delegate);
	Settings::loadWindowPos("DlgTemplatesList", *this);

	// Connect the signals:
	connect(mUI->btnClose,             &QPushButton::clicked,               this, &DlgTemplatesList::close);
	connect(mUI->btnAddTemplate,       &QPushButton::clicked,               this, &DlgTemplatesList::addTemplate);
	connect(mUI->btnRemoveTemplate,    &QPushButton::clicked,               this, &DlgTemplatesList::removeTemplates);
	connect(mUI->btnMoveTemplatesUp,   &QPushButton::clicked,               this, &DlgTemplatesList::moveTemplateUp);
	connect(mUI->btnMoveTemplatesDown, &QPushButton::clicked,               this, &DlgTemplatesList::moveTemplateDown);
	connect(mUI->btnImport,            &QPushButton::clicked,               this, &DlgTemplatesList::importTemplates);
	connect(mUI->btnExport,            &QPushButton::clicked,               this, &DlgTemplatesList::exportTemplates);
	connect(mUI->btnRemoveItem,        &QPushButton::clicked,               this, &DlgTemplatesList::removeItems);
	connect(mUI->btnMoveItemUp,        &QPushButton::clicked,               this, &DlgTemplatesList::moveItemUp);
	connect(mUI->btnMoveItemDown,      &QPushButton::clicked,               this, &DlgTemplatesList::moveItemDown);
	connect(mUI->btnManageFilters,     &QPushButton::clicked,               this, &DlgTemplatesList::manageFilters);
	connect(mUI->tblTemplates,         &QTableWidget::itemSelectionChanged, this, &DlgTemplatesList::templateSelectionChanged);
	connect(mUI->tblTemplates,         &QTableWidget::itemChanged,          this, &DlgTemplatesList::templateChanged);
	connect(mUI->tblItems,             &QTableWidget::itemSelectionChanged, this, &DlgTemplatesList::itemSelectionChanged);
	connect(mUI->tblItems,             &QTableWidget::cellDoubleClicked,    this, &DlgTemplatesList::itemDoubleClicked);
	connect(mUI->tblItems,             &QTableWidget::itemChanged,          this, &DlgTemplatesList::itemChanged);

	// Update the UI state:
	const auto & templates = mComponents.get<Database>()->templates();
	mUI->tblTemplates->setRowCount(static_cast<int>(templates.size()));
	mUI->tblItems->setRowCount(0);
	templateSelectionChanged();

	// Insert all the templates into the table view:
	for (size_t i = 0; i < templates.size(); ++i)
	{
		const auto & tmpl = templates[i];
		updateTemplateRow(static_cast<int>(i), *tmpl);
	}

	// Insert all the filters into the Add menu:
	updateFilterMenu();

	Settings::loadHeaderView("DlgTemplatesList", "tblTemplates", *mUI->tblTemplates->horizontalHeader());
	Settings::loadHeaderView("DlgTemplatesList", "tblItems",     *mUI->tblItems->horizontalHeader());
}





DlgTemplatesList::~DlgTemplatesList()
{
	Settings::saveHeaderView("DlgTemplatesList", "tblTemplates", *mUI->tblTemplates->horizontalHeader());
	Settings::saveHeaderView("DlgTemplatesList", "tblItems",     *mUI->tblItems->horizontalHeader());
	Settings::saveWindowPos("DlgTemplatesList", *this);
}





TemplatePtr DlgTemplatesList::templateFromRow(int aRow)
{
	if (aRow < 0)
	{
		assert(!"Invalid row");
		return nullptr;
	}
	auto row = static_cast<size_t>(aRow);
	if (row >= mComponents.get<Database>()->templates().size())
	{
		assert(!"Invalid row");
		return nullptr;
	}
	return mComponents.get<Database>()->templates()[row];
}





void DlgTemplatesList::updateTemplateRow(int aRow, const Template & aTemplate)
{
	mIsInternalChange = true;
	auto item = new QTableWidgetItem(aTemplate.displayName());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(aTemplate.bgColor());
	mUI->tblTemplates->setItem(aRow, colTemplateDisplayName, item);

	auto numItems = QString::number(aTemplate.items().size());
	item = new QTableWidgetItem(numItems);
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	item->setBackgroundColor(aTemplate.bgColor());
	mUI->tblTemplates->setItem(aRow, colTemplateNumItems, item);

	item = new QTableWidgetItem(aTemplate.bgColor().name());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(aTemplate.bgColor());
	mUI->tblTemplates->setItem(aRow, colTemplateBgColor, item);
	mIsInternalChange = false;

	item = new QTableWidgetItem(aTemplate.notes());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	item->setBackgroundColor(aTemplate.bgColor());
	mUI->tblTemplates->setItem(aRow, colTemplateNotes, item);
	mIsInternalChange = false;
}





void DlgTemplatesList::updateTemplateItemRow(int aRow, const Filter & aItem)
{
	mIsInternalChange = true;
	auto wi = new QTableWidgetItem(aItem.displayName());
	wi->setFlags(wi->flags() | Qt::ItemIsEditable);
	wi->setBackgroundColor(aItem.bgColor());
	mUI->tblItems->setItem(aRow, colItemDisplayName, wi);

	wi = new QTableWidgetItem(aItem.notes());
	wi->setFlags(wi->flags() | Qt::ItemIsEditable);
	wi->setBackgroundColor(aItem.bgColor());
	mUI->tblItems->setItem(aRow, colItemNotes, wi);

	const auto & durationLimit = aItem.durationLimit();
	wi = new QTableWidgetItem(durationLimit.isPresent() ? Utils::formatTime(durationLimit.value()) : "");
	wi->setFlags(wi->flags() | Qt::ItemIsEditable);
	wi->setBackgroundColor(aItem.bgColor());
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	mUI->tblItems->setItem(aRow, colItemDurationLimit, wi);

	wi = new QTableWidgetItem(aItem.bgColor().name());
	wi->setFlags(wi->flags() | Qt::ItemIsEditable);
	wi->setBackgroundColor(aItem.bgColor());
	mUI->tblItems->setItem(aRow, colItemBgColor, wi);

	auto numMatching = mComponents.get<Database>()->numSongsMatchingFilter(aItem);
	wi = new QTableWidgetItem(QString::number(numMatching));
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor((numMatching > 0) ? aItem.bgColor() : QColor(255, 192, 192));
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	mUI->tblItems->setItem(aRow, colItemNumSongs, wi);

	wi = new QTableWidgetItem(aItem.getFilterDescription());
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor(aItem.bgColor());
	mUI->tblItems->setItem(aRow, colItemDescription, wi);
	mIsInternalChange = false;
}





void DlgTemplatesList::exportTemplatesTo(const QString & aFileName)
{
	QFile f(aFileName);
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
	for (const auto & row: mUI->tblTemplates->selectionModel()->selectedRows())
	{
		templates.push_back(mComponents.get<Database>()->templates()[static_cast<size_t>(row.row())]);
	}
	f.write(TemplateXmlExport::run(templates));
	f.close();
}





void DlgTemplatesList::importTemplatesFrom(const QString & aFileName)
{
	QFile f(aFileName);
	if (!f.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Import templates"),
			tr("SkauTan could not import templates from \"%1\", because the file cannot be read from.").arg(aFileName)
		);
		return;
	}
	auto templates = TemplateXmlImport::run(f.readAll());
	if (templates.empty())
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Import templates"),
			tr("There are no SkauTan templates in \"%1\".").arg(aFileName)
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
	auto db = mComponents.get<Database>();
	for (const auto & tmpl: dlg.chosenTemplates())
	{
		db->addTemplate(tmpl);
		db->saveTemplate(*tmpl);
	}

	// Insert the imported templates into the UI:
	mUI->tblTemplates->setRowCount(static_cast<int>(db->templates().size()));
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
	for (const auto & filter: mComponents.get<Database>()->filters())
	{
		auto action = new QAction(filter->displayName(), this);
		QPixmap pixmap(size, size);
		pixmap.fill(filter->bgColor());
		action->setIcon(QIcon(pixmap));
		menu->addAction(action);
		connect(action, &QAction::triggered, [this, filter]() { this->insertFilter(filter); });
	}
	auto oldMenu = mUI->btnAddItem->menu();
	mUI->btnAddItem->setMenu(menu);
	oldMenu->deleteLater();
}





void DlgTemplatesList::insertFilter(FilterPtr aFilter)
{
	auto tmpl = currentTemplate();
	if (tmpl == nullptr)
	{
		return;
	}

	const auto & selection = mUI->tblItems->selectionModel()->selectedRows();
	int idxInsertBefore;
	if (selection.isEmpty())
	{
		idxInsertBefore = mUI->tblItems->rowCount();
	}
	else
	{
		idxInsertBefore = selection.last().row();
	}

	tmpl->insertItem(aFilter, idxInsertBefore);
	mUI->tblItems->insertRow(idxInsertBefore);
	updateTemplateItemRow(idxInsertBefore, *aFilter);
	mUI->tblItems->selectRow(idxInsertBefore);
	mComponents.get<Database>()->saveTemplate(*tmpl);
}





TemplatePtr DlgTemplatesList::currentTemplate()
{
	const auto & selection = mUI->tblTemplates->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return nullptr;
	}
	return templateFromRow(selection[0].row());
}





void DlgTemplatesList::swapTemplatesAndSelectSecond(int aRow1, int aRow2)
{
	auto db = mComponents.get<Database>();
	db->swapTemplatesByIdx(static_cast<size_t>(aRow1), static_cast<size_t>(aRow2));
	const auto & templates = db->templates();
	updateTemplateRow(aRow1, *templates[static_cast<size_t>(aRow1)]);
	updateTemplateRow(aRow2, *templates[static_cast<size_t>(aRow2)]);
	mUI->tblTemplates->selectRow(aRow2);
}





void DlgTemplatesList::swapItemsAndSelectSecond(int aRow1, int aRow2)
{
	auto tmpl = currentTemplate();
	if (tmpl == nullptr)
	{
		assert(!"Invalid template");
		return;
	}
	tmpl->swapItemsByIdx(static_cast<size_t>(aRow1), static_cast<size_t>(aRow2));
	mComponents.get<Database>()->saveTemplate(*tmpl);
	const auto & items = tmpl->items();
	updateTemplateItemRow(aRow1, *items[static_cast<size_t>(aRow1)]);
	updateTemplateItemRow(aRow2, *items[static_cast<size_t>(aRow2)]);
	mUI->tblItems->selectRow(aRow2);
}





void DlgTemplatesList::addTemplate()
{
	auto tmpl = mComponents.get<Database>()->createTemplate();
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
	int idx = static_cast<int>(mComponents.get<Database>()->templates().size()) - 1;
	mUI->tblTemplates->setRowCount(idx + 1);
	updateTemplateRow(idx, *tmpl);
}





void DlgTemplatesList::removeTemplates()
{
	const auto & selection = mUI->tblTemplates->selectionModel()->selectedRows();
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
	std::sort(rowsToRemove.begin(), rowsToRemove.end(), [](const QModelIndex & aRow1, const QModelIndex & aRow2)
		{
			return (aRow1.row() > aRow2.row());
		}
	);
	auto db = mComponents.get<Database>();
	for (const auto & row: rowsToRemove)
	{
		db->delTemplate(templateFromRow(row.row()).get());
		mUI->tblTemplates->removeRow(row.row());
	}
}





void DlgTemplatesList::moveTemplateUp()
{
	const auto & selection = mUI->tblTemplates->selectionModel()->selectedRows();
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
	const auto & selection = mUI->tblTemplates->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= mUI->tblTemplates->rowCount())
	{
		return;
	}
	swapTemplatesAndSelectSecond(row, row + 1);
}






void DlgTemplatesList::templateSelectionChanged()
{
	mIsInternalChange = true;
	const auto & selection = mUI->tblTemplates->selectionModel()->selectedRows();
	auto selSize = selection.size();
	mUI->btnRemoveTemplate->setEnabled(!selection.isEmpty());
	mUI->btnExport->setEnabled(!selection.isEmpty());
	mUI->btnAddItem->setEnabled(selSize == 1);

	// Update the up / down controls:
	// Update the list of items in a template:
	if (selSize == 1)
	{
		const auto & tmpl = templateFromRow(selection[0].row());
		mUI->tblItems->setRowCount(static_cast<int>(tmpl->items().size()));
		int idx = 0;
		for (const auto & item: tmpl->items())
		{
			updateTemplateItemRow(idx, *item);
			idx += 1;
		}
		auto numTemplates = static_cast<int>(mComponents.get<Database>()->templates().size());
		mUI->btnMoveTemplatesDown->setEnabled(selection[0].row() + 1 < numTemplates);
		mUI->btnMoveTemplatesUp->setEnabled(selection[0].row() > 0);
	}
	else
	{
		mUI->btnMoveTemplatesDown->setEnabled(false);
		mUI->btnMoveTemplatesUp->setEnabled(false);
		mUI->tblItems->setRowCount(0);
	}
	mIsInternalChange = false;
}





void DlgTemplatesList::templateChanged(QTableWidgetItem * aItem)
{
	if (mIsInternalChange)
	{
		return;
	}
	auto tmpl = templateFromRow(aItem->row());
	if (tmpl == nullptr)
	{
		qWarning() << "Bad template pointer in item " << aItem->text();
		return;
	}
	switch (aItem->column())
	{
		case 0:
		{
			tmpl->setDisplayName(aItem->text());
			break;
		}
		case 2:
		{
			QColor c(aItem->text());
			if (c.isValid())
			{
				tmpl->setBgColor(c);
				updateTemplateRow(aItem->row(), *tmpl);
			}
			break;
		}
		case 3:
		{
			tmpl->setNotes(aItem->text());
			break;
		}
	}
	mComponents.get<Database>()->saveTemplate(*tmpl);
}





void DlgTemplatesList::itemSelectionChanged()
{
	const auto & sel = mUI->tblItems->selectionModel()->selectedRows();
	mUI->btnRemoveItem->setEnabled(!sel.isEmpty());

	if (sel.size() == 1)
	{
		auto row = sel[0].row();
		mUI->btnMoveItemUp->setEnabled(row > 0);
		mUI->btnMoveItemDown->setEnabled(row + 1 < mUI->tblItems->rowCount());
	}
	else
	{
		mUI->btnMoveItemUp->setEnabled(false);
		mUI->btnMoveItemDown->setEnabled(false);
	}
}





void DlgTemplatesList::itemDoubleClicked(int aRow, int aColumn)
{
	Q_UNUSED(aColumn);

	// Get the filter represented by the table cell:
	const auto & selTemplate = currentTemplate();
	if (selTemplate == nullptr)
	{
		assert(!"Template item is visible with no selected template");
		return;
	}
	if ((aRow < 0) || (static_cast<size_t>(aRow) >= selTemplate->items().size()))
	{
		assert(!"Invalid template item index");
		return;
	}
	auto item = selTemplate->items()[static_cast<size_t>(aRow)];

	// Edit the filter:
	DlgEditFilter dlg(mComponents, *item);
	dlg.exec();
	mComponents.get<Database>()->saveFilter(*item);

	// Update the UI that may have been affected by a filter edit:
	updateTemplateItemRow(aRow, *item);
	updateFilterMenu();
}





void DlgTemplatesList::itemChanged(QTableWidgetItem * aItem)
{
	if (mIsInternalChange)
	{
		return;
	}
	auto tmpl = currentTemplate();
	if (tmpl == nullptr)
	{
		assert(!"Invalid template");
		return;
	}
	assert(aItem != nullptr);

	// Get the affected filter object :
	auto row = aItem->row();
	const auto & items = tmpl->items();
	if ((row < 0) || (static_cast<size_t>(row) >= items.size()))
	{
		assert(!"Bad item index");
		return;
	}
	auto filter = tmpl->items()[static_cast<size_t>(row)];

	// Update the object:
	switch (aItem->column())
	{
		case colItemDisplayName: filter->setDisplayName(aItem->text()); break;
		case colItemNotes: filter->setNotes(aItem->text()); break;
		case colItemDurationLimit:
		{
			bool isOK;
			auto limit = Utils::parseTime(aItem->text(), isOK);
			if (isOK)
			{
				filter->setDurationLimit(limit);
			}
			else
			{
				filter->resetDurationLimit();
			}
			break;
		}
		case colItemBgColor:
		{
			QColor c(aItem->text());
			if (c.isValid())
			{
				filter->setBgColor(c);
				updateTemplateItemRow(row, *filter);
			}
			break;
		}
		default:
		{
			assert(!"Uneditable item");
			return;
		}
	}

	// Save to DB:
	mComponents.get<Database>()->saveFilter(*filter);

	// Update all rows containing the filter:
	mIsInternalChange = true;
	auto num = static_cast<int>(items.size());
	for (int i = 0; i < num; ++i)
	{
		auto item = items[static_cast<size_t>(i)];
		if (item.get() == filter.get())
		{
			updateTemplateItemRow(i, *item);
		}
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




void DlgTemplatesList::manageFilters()
{
	DlgManageFilters dlg(mComponents, this);
	dlg.exec();

	// Update the filters in the "Add" popup menu:
	updateFilterMenu();

	// Update the item counts in the template list (in case a filter was deleted):
	const auto & templates = mComponents.get<Database>()->templates();
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
	const auto & selection = mUI->tblItems->selectionModel()->selectedRows();
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
	std::sort(rowsToRemove.begin(), rowsToRemove.end(), [](const QModelIndex & aRow1, const QModelIndex & aRow2)
		{
			return (aRow1.row() > aRow2.row());
		}
	);
	auto tmpl = currentTemplate();
	for (const auto & row: rowsToRemove)
	{
		tmpl->delItem(row.row());
		mUI->tblItems->removeRow(row.row());
	}
	mComponents.get<Database>()->saveTemplate(*tmpl);
}




void DlgTemplatesList::moveItemUp()
{
	const auto & selection = mUI->tblItems->selectionModel()->selectedRows();
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
	const auto & selection = mUI->tblItems->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= mUI->tblItems->rowCount())
	{
		return;
	}
	swapItemsAndSelectSecond(row, row + 1);
}







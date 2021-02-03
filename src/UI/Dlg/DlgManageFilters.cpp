#include "DlgManageFilters.hpp"
#include "ui_DlgManageFilters.h"
#include <cassert>
#include <QMessageBox>
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "../../ComponentCollection.hpp"
#include "../../Utils.hpp"
#include "../ColorDelegate.hpp"
#include "DlgEditFilter.hpp"





/** Columns in tblFilters. */
enum
{
	colDisplayName = 0,
	colNotes,
	colIsFavorite,
	colBgColor,
	colDurationLimit,
	colNumSongs,
	colNumTemplates,
	colDescription,
};





DlgManageFilters::DlgManageFilters(
	ComponentCollection & aComponents,
	QWidget * aParent
):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::DlgManageFilters),
	mIsInternalChange(false)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgManageFilters", *this);
	Settings::loadHeaderView("DlgManageFilters", "tblFilters", *mUI->tblFilters->horizontalHeader());
	auto delegate = new ColorDelegate(tr("SkauTan: Choose filter color"));
	mUI->tblFilters->setItemDelegateForColumn(3, delegate);
	mUI->btnReplace->setMenu(&mReplaceMenu);

	// Connect the signals:
	connect(mUI->btnAdd,       &QPushButton::pressed,               this, &DlgManageFilters::addFilter);
	connect(mUI->btnEdit,      &QPushButton::pressed,               this, &DlgManageFilters::editFilter);
	connect(mUI->btnRemove,    &QPushButton::pressed,               this, &DlgManageFilters::removeFilter);
	connect(mUI->btnMoveUp,    &QPushButton::pressed,               this, &DlgManageFilters::moveFilterUp);
	connect(mUI->btnMoveDown,  &QPushButton::pressed,               this, &DlgManageFilters::moveFilterDown);
	connect(mUI->btnDuplicate, &QPushButton::pressed,               this, &DlgManageFilters::duplicateFilter);
	connect(mUI->btnClose,     &QPushButton::pressed,               this, &QDialog::close);
	connect(mUI->tblFilters,   &QTableWidget::itemSelectionChanged, this, &DlgManageFilters::filterSelectionChanged);
	connect(mUI->tblFilters,   &QTableWidget::itemChanged,          this, &DlgManageFilters::filterChanged);
	connect(mUI->tblFilters,   &QTableWidget::cellDoubleClicked,    this, &DlgManageFilters::filterDoubleClicked);

	// Fill in the existing filters:
	auto tbl = mUI->tblFilters;
	const auto & filters = mComponents.get<Database>()->filters();
	auto numFilters = static_cast<int>(filters.size());
	tbl->setRowCount(numFilters);
	for (int i = 0; i < numFilters; ++i)
	{
		updateFilterRow(i, *filters[static_cast<size_t>(i)]);
	}

	filterSelectionChanged();
}





DlgManageFilters::~DlgManageFilters()
{
	Settings::saveWindowPos("DlgManageFilters", *this);
	Settings::saveHeaderView("DlgManageFilters", "tblFilters", *mUI->tblFilters->horizontalHeader());
}





void DlgManageFilters::updateFilterRow(int aRow, const Filter & aFilter)
{
	assert(aRow >= 0);
	assert(aRow < static_cast<int>(mComponents.get<Database>()->filters().size()));

	mIsInternalChange = true;

	auto tbl = mUI->tblFilters;
	auto item = new QTableWidgetItem(aFilter.displayName());
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(aRow, colDisplayName, item);

	item = new QTableWidgetItem(aFilter.notes());
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(aRow, colNotes, item);

	item = new QTableWidgetItem();
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
	item->setCheckState(aFilter.isFavorite() ? Qt::Checked : Qt::Unchecked);
	tbl->setItem(aRow, colIsFavorite, item);

	item = new QTableWidgetItem(aFilter.bgColor().name());
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(aRow, colBgColor, item);

	auto durationLimit = aFilter.durationLimit();
	item = new QTableWidgetItem(durationLimit.isPresent() ? Utils::formatTime(durationLimit.value()) : "");
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(aRow, colDurationLimit, item);

	auto db = mComponents.get<Database>();
	item = new QTableWidgetItem(QString("%1").arg(db->numSongsMatchingFilter(aFilter)));
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	tbl->setItem(aRow, colNumSongs, item);

	item = new QTableWidgetItem(QString("%1").arg(db->numTemplatesContaining(aFilter)));
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	tbl->setItem(aRow, colNumTemplates, item);

	item = new QTableWidgetItem(aFilter.getFilterDescription());
	item->setBackground(aFilter.bgColor());
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	tbl->setItem(aRow, colDescription, item);

	mIsInternalChange = false;
}





void DlgManageFilters::editFilterOnRow(int aRow)
{
	const auto & filters = mComponents.get<Database>()->filters();
	if ((aRow < 0) || (aRow >= static_cast<int>(filters.size())))
	{
		return;
	}
	auto filter = filters[static_cast<size_t>(aRow)];
	DlgEditFilter dlg(mComponents, *filter, this);
	dlg.exec();
	mComponents.get<Database>()->saveFilter(*filter);
	updateFilterRow(aRow, *filter);
}




void DlgManageFilters::swapFiltersAndSelectSecond(int aRow1, int aRow2)
{
	auto db = mComponents.get<Database>();
	db->swapFiltersByIdx(static_cast<size_t>(aRow1), static_cast<size_t>(aRow2));
	const auto & filters = db->filters();
	updateFilterRow(aRow1, *filters[static_cast<size_t>(aRow1)]);
	updateFilterRow(aRow2, *filters[static_cast<size_t>(aRow2)]);
	mUI->tblFilters->selectRow(aRow2);
}





void DlgManageFilters::updateReplaceMenu()
{
	// Clear the current contents of the menu:
	mReplaceMenu.clear();

	// Add all unselected filters into the menu, together with the handlers:
	const auto & selModel = mUI->tblFilters->selectionModel();
	const auto & filters = mComponents.get<Database>()->filters();
	auto numFilters = static_cast<int>(filters.size());
	QModelIndex rootIndex;
	auto size = mReplaceMenu.style()->pixelMetric(QStyle::PM_SmallIconSize);
	for (int row = 0; row < numFilters; ++row)
	{
		if (selModel->isRowSelected(row, rootIndex))
		{
			continue;
		}
		const auto filter = filters[static_cast<size_t>(row)];
		auto action = new QAction(filter->displayName(), this);
		QPixmap pixmap(size, size);
		pixmap.fill(filter->bgColor());
		action->setIcon(QIcon(pixmap));
		mReplaceMenu.addAction(action);
		connect(action, &QAction::triggered, [this, filter]() { this->replaceWithFilter(filter); });
	}
}





void DlgManageFilters::replaceWithFilter(FilterPtr aFilter)
{
	// Replace the filters:
	const auto & selModel = mUI->tblFilters->selectionModel();
	auto db = mComponents.get<Database>();
	const auto & filters = db->filters();
	auto & templates = db->templates();
	auto numFilters = static_cast<int>(filters.size());
	QModelIndex rootIndex;
	for (int row = 0; row < numFilters; ++row)
	{
		if (!selModel->isRowSelected(row, rootIndex))
		{
			continue;
		}
		const auto & filter = filters[static_cast<size_t>(row)];
		assert(aFilter != filter);
		for (auto & tmpl: templates)
		{
			tmpl->replaceFilter(*filter, *aFilter);
		}
	}
	db->saveAllTemplates();

	// Update the UI, esp. template counts:
	for (int row = 0; row < numFilters; ++row)
	{
		const auto & filter = filters[static_cast<size_t>(row)];
		updateFilterRow(row, *filter);
	}
}





void DlgManageFilters::addFilter()
{
	// Add the filter and edit it:
	auto db = mComponents.get<Database>();
	auto filter = db->createFilter();
	DlgEditFilter dlg(mComponents, *filter, this);
	dlg.exec();
	mComponents.get<Database>()->saveFilter(*filter);

	// Update the UI:
	auto numFilters = static_cast<int>(db->filters().size());
	mUI->tblFilters->setRowCount(numFilters);
	updateFilterRow(numFilters - 1, *filter);
}





void DlgManageFilters::editFilter()
{
	const auto & selection = mUI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	editFilterOnRow(selection[0].row());
}





void DlgManageFilters::removeFilter()
{
	const auto & selection = mUI->tblFilters->selectionModel()->selectedRows();
	if (selection.isEmpty())
	{
		return;
	}
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove filter"),
		tr("Are you sure you want to remove the selected filters? The filters will also be removed from any template that uses them."),
		QMessageBox::Yes, QMessageBox::No
	) != QMessageBox::Yes)
	{
		return;
	}

	// Dlete from the DB and UI, from the last selected to the first:
	std::vector<int> rows;
	for (const auto & row: selection)
	{
		rows.push_back(row.row());
	}
	std::sort(rows.begin(), rows.end(), std::greater<int>());
	auto db = mComponents.get<Database>();
	for (const auto row: rows)
	{
		db->delFilter(static_cast<size_t>(row));
		mUI->tblFilters->removeRow(row);
	}
}




void DlgManageFilters::moveFilterUp()
{
	const auto & selection = mUI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row < 1)
	{
		return;
	}
	swapFiltersAndSelectSecond(row, row - 1);
}





void DlgManageFilters::moveFilterDown()
{
	const auto & selection = mUI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= mUI->tblFilters->rowCount())
	{
		return;
	}
	swapFiltersAndSelectSecond(row, row + 1);
}





void DlgManageFilters::duplicateFilter()
{
	const auto & selection = mUI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= mUI->tblFilters->rowCount())
	{
		return;
	}
	auto db = mComponents.get<Database>();
	auto filter = db->filters()[static_cast<size_t>(row)];
	auto clone = std::make_shared<Filter>(*filter);  // Clone the entire filter
	db->addFilter(clone);
	auto newRow = mUI->tblFilters->rowCount();
	mUI->tblFilters->setRowCount(newRow + 1);
	updateFilterRow(newRow, *clone);
	mUI->tblFilters->selectRow(newRow);
}





void DlgManageFilters::filterSelectionChanged()
{
	const auto & selection = mUI->tblFilters->selectionModel()->selectedRows();
	auto selSize = selection.size();
	mUI->btnEdit->setEnabled(selSize == 1);
	mUI->btnRemove->setEnabled(selSize > 0);
	bool canReplace = (selSize > 0) && (selSize < mUI->tblFilters->rowCount());
	mUI->btnReplace->setEnabled(canReplace);
	mUI->btnMoveUp->setEnabled((selSize == 1) && (selection[0].row() > 0));
	mUI->btnMoveDown->setEnabled((selSize == 1) && (selection[0].row() + 1 < mUI->tblFilters->rowCount()));
	if (canReplace)
	{
		updateReplaceMenu();
	}
}





void DlgManageFilters::filterChanged(QTableWidgetItem * aItem)
{
	assert(aItem != nullptr);
	if (mIsInternalChange)
	{
		return;
	}
	auto db = mComponents.get<Database>();
	auto row = aItem->row();
	auto filter = db->filters()[static_cast<size_t>(row)];
	switch (aItem->column())
	{
		case colDisplayName: filter->setDisplayName(aItem->text()); break;
		case colNotes: filter->setNotes(aItem->text()); break;
		case colIsFavorite: filter->setIsFavorite(aItem->checkState() == Qt::Checked); break;
		case colDurationLimit:
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
		case colBgColor:
		{
			QColor c(aItem->text());
			if (c.isValid())
			{
				filter->setBgColor(c);
				updateFilterRow(row, *filter);
			}
			break;
		}
		default:
		{
			assert(!"Editing a non-editable item");
			break;
		}
	}
	db->saveFilter(*filter);
}





void DlgManageFilters::filterDoubleClicked(int aRow, int aColumn)
{
	Q_UNUSED(aColumn);
	editFilterOnRow(aRow);
}

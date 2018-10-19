#include "DlgManageFilters.h"
#include "ui_DlgManageFilters.h"
#include <cassert>
#include <QMessageBox>
#include "Settings.h"
#include "ComponentCollection.h"
#include "Database.h"
#include "DlgEditFilter.h"
#include "ColorDelegate.h"
#include "Utils.h"





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
	ComponentCollection & a_Components,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_Components(a_Components),
	m_UI(new Ui::DlgManageFilters),
	m_IsInternalChange(false)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgManageFilters", *this);
	Settings::loadHeaderView("DlgManageFilters", "tblFilters", *m_UI->tblFilters->horizontalHeader());
	auto delegate = new ColorDelegate(tr("SkauTan: Choose filter color"));
	m_UI->tblFilters->setItemDelegateForColumn(3, delegate);

	// Connect the signals:
	connect(m_UI->btnAdd,       &QPushButton::pressed,               this, &DlgManageFilters::addFilter);
	connect(m_UI->btnEdit,      &QPushButton::pressed,               this, &DlgManageFilters::editFilter);
	connect(m_UI->btnRemove,    &QPushButton::pressed,               this, &DlgManageFilters::removeFilter);
	connect(m_UI->btnMoveUp,    &QPushButton::pressed,               this, &DlgManageFilters::moveFilterUp);
	connect(m_UI->btnMoveDown,  &QPushButton::pressed,               this, &DlgManageFilters::moveFilterDown);
	connect(m_UI->btnDuplicate, &QPushButton::pressed,               this, &DlgManageFilters::duplicateFilter);
	connect(m_UI->btnClose,     &QPushButton::pressed,               this, &QDialog::close);
	connect(m_UI->tblFilters,   &QTableWidget::itemSelectionChanged, this, &DlgManageFilters::filterSelectionChanged);
	connect(m_UI->tblFilters,   &QTableWidget::itemChanged,          this, &DlgManageFilters::filterChanged);

	// Fill in the existing filters:
	auto tbl = m_UI->tblFilters;
	const auto & filters = m_Components.get<Database>()->filters();
	auto numFilters = static_cast<int>(filters.size());
	tbl->setRowCount(numFilters);
	for (int i = 0; i < numFilters; ++i)
	{
		updateFilterRow(i, *filters[static_cast<size_t>(i)]);
	}
}





DlgManageFilters::~DlgManageFilters()
{
	Settings::saveWindowPos("DlgManageFilters", *this);
	Settings::saveHeaderView("DlgManageFilters", "tblFilters", *m_UI->tblFilters->horizontalHeader());
}





void DlgManageFilters::updateFilterRow(int a_Row, const Filter & a_Filter)
{
	assert(a_Row >= 0);
	assert(a_Row < m_UI->tblFilters->rowCount());

	m_IsInternalChange = true;

	auto tbl = m_UI->tblFilters;
	auto item = new QTableWidgetItem(a_Filter.displayName());
	item->setBackgroundColor(a_Filter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(a_Row, colDisplayName, item);

	item = new QTableWidgetItem(a_Filter.notes());
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(a_Row, colNotes, item);

	item = new QTableWidgetItem();
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
	item->setCheckState(a_Filter.isFavorite() ? Qt::Checked : Qt::Unchecked);
	tbl->setItem(a_Row, colIsFavorite, item);

	item = new QTableWidgetItem(a_Filter.bgColor().name());
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(a_Row, colBgColor, item);

	auto durationLimit = a_Filter.durationLimit();
	item = new QTableWidgetItem(durationLimit.isPresent() ? Utils::formatTime(durationLimit.value()) : "");
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	tbl->setItem(a_Row, colDurationLimit, item);

	auto db = m_Components.get<Database>();
	item = new QTableWidgetItem(QString("%1").arg(db->numSongsMatchingFilter(a_Filter)));
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	tbl->setItem(a_Row, colNumSongs, item);

	item = new QTableWidgetItem(QString("%1").arg(db->numTemplatesContaining(a_Filter)));
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	tbl->setItem(a_Row, colNumTemplates, item);

	item = new QTableWidgetItem(a_Filter.getFilterDescription());
	item->setBackground(a_Filter.bgColor());
	item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	tbl->setItem(a_Row, colDescription, item);

	m_IsInternalChange = false;
}





void DlgManageFilters::editFilterOnRow(int a_Row)
{
	const auto & filters = m_Components.get<Database>()->filters();
	if ((a_Row < 0) || (a_Row >= static_cast<int>(filters.size())))
	{
		return;
	}
	auto filter = filters[static_cast<size_t>(a_Row)];
	DlgEditFilter dlg(m_Components, *filter, this);
	dlg.exec();
	updateFilterRow(a_Row, *filter);
}




void DlgManageFilters::swapFiltersAndSelectSecond(int a_Row1, int a_Row2)
{
	auto db = m_Components.get<Database>();
	db->swapFiltersByIdx(static_cast<size_t>(a_Row1), static_cast<size_t>(a_Row2));
	const auto & filters = db->filters();
	updateFilterRow(a_Row1, *filters[static_cast<size_t>(a_Row1)]);
	updateFilterRow(a_Row2, *filters[static_cast<size_t>(a_Row2)]);
	m_UI->tblFilters->selectRow(a_Row2);
}





void DlgManageFilters::addFilter()
{
	auto db = m_Components.get<Database>();
	auto filter = db->createFilter();
	DlgEditFilter dlg(m_Components, *filter, this);
	dlg.exec();
	auto numFilters = static_cast<int>(db->filters().size());
	m_UI->tblFilters->setRowCount(numFilters);
	updateFilterRow(numFilters, *filter);
}





void DlgManageFilters::editFilter()
{
	const auto & selection = m_UI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	editFilterOnRow(selection[0].row());
}





void DlgManageFilters::removeFilter()
{
	const auto & selection = m_UI->tblFilters->selectionModel()->selectedRows();
	if (selection.isEmpty())
	{
		return;
	}
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove filter?"),
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
	auto db = m_Components.get<Database>();
	for (const auto row: rows)
	{
		db->delFilter(static_cast<size_t>(row));
		m_UI->tblFilters->removeRow(row);
	}
}




void DlgManageFilters::moveFilterUp()
{
	const auto & selection = m_UI->tblFilters->selectionModel()->selectedRows();
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
	const auto & selection = m_UI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= m_UI->tblFilters->rowCount())
	{
		return;
	}
	swapFiltersAndSelectSecond(row, row + 1);
}





void DlgManageFilters::duplicateFilter()
{
	const auto & selection = m_UI->tblFilters->selectionModel()->selectedRows();
	if (selection.size() != 1)
	{
		return;
	}
	auto row = selection[0].row();
	if (row + 1 >= m_UI->tblFilters->rowCount())
	{
		return;
	}
	auto db = m_Components.get<Database>();
	auto filter = db->filters()[static_cast<size_t>(row)];
	auto clone = std::make_shared<Filter>(*filter);  // Clone the entire filter
	db->addFilter(clone);
	auto newRow = m_UI->tblFilters->rowCount();
	m_UI->tblFilters->setRowCount(newRow + 1);
	updateFilterRow(newRow, *clone);
	m_UI->tblFilters->selectRow(newRow);
}





void DlgManageFilters::filterSelectionChanged()
{
	const auto & selection = m_UI->tblFilters->selectionModel()->selectedRows();
	auto selSize = selection.size();
	m_UI->btnEdit->setEnabled(selSize == 1);
	m_UI->btnRemove->setEnabled(selSize > 0);
	m_UI->btnMoveUp->setEnabled((selSize == 1) && (selection[0].row() > 0));
	m_UI->btnMoveDown->setEnabled((selSize == 1) && (selection[0].row() + 1 < m_UI->tblFilters->rowCount()));
}





void DlgManageFilters::filterChanged(QTableWidgetItem * a_Item)
{
	assert(a_Item != nullptr);
	if (m_IsInternalChange)
	{
		return;
	}
	auto db = m_Components.get<Database>();
	auto row = a_Item->row();
	auto filter = db->filters()[static_cast<size_t>(row)];
	switch (a_Item->column())
	{
		case colDisplayName: filter->setDisplayName(a_Item->text()); break;
		case colNotes: filter->setNotes(a_Item->text()); break;
		case colIsFavorite: filter->setIsFavorite(a_Item->checkState() == Qt::Checked); break;
		case colDurationLimit:
		{
			bool isOK;
			auto limit = Utils::parseTime(a_Item->text(), isOK);
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
			QColor c(a_Item->text());
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

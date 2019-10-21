#include "DlgChooseImportTemplates.hpp"
#include "ui_DlgChooseImportTemplates.h"
#include "../../Settings.hpp"





DlgChooseImportTemplates::DlgChooseImportTemplates(std::vector<TemplatePtr> && aTemplates, QWidget * aParent):
	Super(aParent),
	mTemplates(std::move(aTemplates)),
	mUI(new Ui::DlgChooseImportTemplates)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgChooseImportTemplates", *this);

	// Connect the signals:
	connect(mUI->btnCancel,    &QPushButton::clicked,               this, &QDialog::reject);
	connect(mUI->btnImport,    &QPushButton::clicked,               this, &DlgChooseImportTemplates::import);
	connect(mUI->tblTemplates, &QTableWidget::itemSelectionChanged, this, &DlgChooseImportTemplates::templateSelectionChanged);
	connect(mUI->tblTemplates, &QTableWidget::cellChanged,          this, &DlgChooseImportTemplates::templateCellChanged);

	// Update the UI state:
	mUI->tblTemplates->setRowCount(static_cast<int>(mTemplates.size()));
	mUI->tblTemplates->setColumnCount(3);
	mUI->tblTemplates->setHorizontalHeaderLabels({tr("Name"), tr("#"), tr("Notes")});
	mUI->tblItems->setRowCount(0);
	mUI->tblItems->setColumnCount(4);
	mUI->tblItems->setHorizontalHeaderLabels({tr("Name"), tr("Notes"), tr("Fav"), tr("Filter")});
	templateSelectionChanged();

	// Insert all the templates into the table view:
	for (size_t i = 0; i < mTemplates.size(); ++i)
	{
		const auto & tmpl = mTemplates[i];
		updateTemplateRow(static_cast<int>(i), *tmpl);
	}

	Settings::loadHeaderView("DlgChooseImportTemplates", "tblTemplates", *mUI->tblTemplates->horizontalHeader());
	Settings::loadHeaderView("DlgChooseImportTemplates", "tblItems",     *mUI->tblItems->horizontalHeader());
}





DlgChooseImportTemplates::~DlgChooseImportTemplates()
{
	Settings::saveHeaderView("DlgChooseImportTemplates", "tblTemplates", *mUI->tblTemplates->horizontalHeader());
	Settings::saveHeaderView("DlgChooseImportTemplates", "tblItems",     *mUI->tblItems->horizontalHeader());
	Settings::saveWindowPos("DlgChooseImportTemplates", *this);
}





void DlgChooseImportTemplates::updateTemplateRow(int aRow, const Template & aTemplate)
{
	auto item = new QTableWidgetItem(aTemplate.displayName());
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
	item->setCheckState(Qt::Checked);
	mUI->tblTemplates->setItem(aRow, 0, item);
	mUI->tblTemplates->setItem(aRow, 1, new QTableWidgetItem(QString::number(aTemplate.items().size())));
	mUI->tblTemplates->setItem(aRow, 2, new QTableWidgetItem(aTemplate.notes()));
	auto colCount = mUI->tblTemplates->columnCount();
	for (int col = 0; col < colCount; ++col)
	{
		mUI->tblTemplates->item(aRow, col)->setBackgroundColor(aTemplate.bgColor());
	}
}





void DlgChooseImportTemplates::import()
{
	// Process the check state on templates:
	mChosenTemplates.clear();
	int numRows = mUI->tblTemplates->rowCount();
	for (int row = 0; row < numRows; ++row)
	{
		if (mUI->tblTemplates->item(row, 0)->checkState() == Qt::Checked)
		{
			mChosenTemplates.push_back(mTemplates[static_cast<size_t>(row)]);
		}
	}

	accept();
}





void DlgChooseImportTemplates::templateSelectionChanged()
{
	const auto & selection = mUI->tblTemplates->selectionModel()->selectedRows();

	// Update the list of items in a template:
	if (selection.size() == 1)
	{
		const auto & tmpl = mTemplates[static_cast<size_t>(selection[0].row())];
		mUI->tblItems->setRowCount(static_cast<int>(tmpl->items().size()));
		int idx = 0;
		auto colCount = mUI->tblItems->columnCount();
		for (const auto & item: tmpl->items())
		{
			auto favDesc = item->isFavorite() ? tr("Y", "IsFavorite") : QString();
			auto filterDesc = item->rootNode()->getDescription();
			mUI->tblItems->setItem(idx, 0, new QTableWidgetItem(item->displayName()));
			mUI->tblItems->setItem(idx, 1, new QTableWidgetItem(item->notes()));
			mUI->tblItems->setItem(idx, 2, new QTableWidgetItem(favDesc));
			mUI->tblItems->setItem(idx, 3, new QTableWidgetItem(filterDesc));
			for (int col = 0; col < colCount; ++col)
			{
				mUI->tblItems->item(idx, col)->setBackgroundColor(item->bgColor());
			}
			idx += 1;
		}
	}
	else
	{
		mUI->tblItems->setRowCount(0);
	}
}





void DlgChooseImportTemplates::templateCellChanged(int aRow, int aColumn)
{
	Q_UNUSED(aRow);
	Q_UNUSED(aColumn);

	int numRows = mUI->tblTemplates->rowCount();
	for (int i = 0; i < numRows; ++i)
	{
		if (mUI->tblTemplates->item(i, 0)->checkState() == Qt::Checked)
		{
			mUI->btnImport->setEnabled(true);
			return;
		}
	}
	mUI->btnImport->setEnabled(false);
}

#include "DlgEditTemplate.h"
#include <QMessageBox>
#include <QColorDialog>
#include <QDebug>
#include "ui_DlgEditTemplate.h"
#include "DlgEditTemplateItem.h"
#include "DlgPickTemplateFavoriteItem.h"
#include "Database.h"
#include "Settings.h"





DlgEditTemplate::DlgEditTemplate(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	HashCalculator & a_Hasher,
	Template & a_Template,
	QWidget * a_Parent
):
	QDialog(a_Parent),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_HashCalculator(a_Hasher),
	m_Template(a_Template),
	m_UI(new Ui::DlgEditTemplate)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgEditTemplate", *this);

	// Connect the slots:
	connect(m_UI->btnClose,           &QPushButton::clicked,      this, &DlgEditTemplate::saveAndClose);
	connect(m_UI->btnAddItem,         &QPushButton::clicked,      this, &DlgEditTemplate::addItem);
	connect(m_UI->btnAddFavoriteItem, &QPushButton::clicked,      this, &DlgEditTemplate::addFavoriteItem);
	connect(m_UI->btnEditItem,        &QPushButton::clicked,      this, &DlgEditTemplate::editSelectedItem);
	connect(m_UI->btnRemoveItem,      &QPushButton::clicked,      this, &DlgEditTemplate::removeSelectedItems);
	connect(m_UI->leBgColor,          &QLineEdit::textChanged,    this, &DlgEditTemplate::bgColorTextChanged);
	connect(m_UI->btnBgColor,         &QPushButton::clicked,      this, &DlgEditTemplate::chooseBgColor);
	connect(m_UI->tblItems,           &QTableWidget::itemChanged, this, &DlgEditTemplate::itemChanged);
	connect(
		m_UI->tblItems, &QTableWidget::cellDoubleClicked,
		this, &DlgEditTemplate::cellDoubleClicked
	);
	connect(
		m_UI->tblItems->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &DlgEditTemplate::itemSelectionChanged
	);

	// Set the data:
	m_UI->leDisplayName->setText(a_Template.displayName());
	m_UI->pteNotes->setPlainText(a_Template.notes());
	m_UI->tblItems->setRowCount(static_cast<int>(a_Template.items().size()));
	m_UI->tblItems->setColumnCount(5);
	m_UI->tblItems->setHorizontalHeaderLabels({tr("Name"), tr("Notes"), tr("Fav"), tr("# Songs"), tr("Filter")});
	m_UI->leBgColor->setText(a_Template.bgColor().name());
	int row = 0;
	for (const auto & itm: a_Template.items())
	{
		updateTemplateItemRow(row, *itm);
		row += 1;
	}

	Settings::loadHeaderView("DlgEditTemplate", "tblItems", *m_UI->tblItems->horizontalHeader());

	itemSelectionChanged();
}





DlgEditTemplate::~DlgEditTemplate()
{
	Settings::saveHeaderView("DlgEditTemplate", "tblItems", *m_UI->tblItems->horizontalHeader());
	Settings::saveWindowPos("DlgEditTemplate", *this);
}





void DlgEditTemplate::reject()
{
	// Despite being called "reject", we do save the data
	// Mainly because we don't have a way of undoing all the edits.
	save();
	Super::reject();
}





void DlgEditTemplate::save()
{
	m_Template.setDisplayName(m_UI->leDisplayName->text());
	m_Template.setNotes(m_UI->pteNotes->toPlainText());
	QColor c(m_UI->leBgColor->text());
	if (c.isValid())
	{
		m_Template.setBgColor(c);
	}
}





void DlgEditTemplate::updateTemplateItemRow(int a_Row, const Template::Item & a_Item)
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

	auto numMatching = m_DB.numSongsMatchingFilter(*a_Item.filter());
	wi = new QTableWidgetItem(QString::number(numMatching));
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor((numMatching > 0) ? a_Item.bgColor() : QColor(255, 192, 192));
	wi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_UI->tblItems->setItem(a_Row, 3, wi);

	wi = new QTableWidgetItem(a_Item.filter()->getDescription());
	wi->setFlags(wi->flags() & ~Qt::ItemIsEditable);
	wi->setBackgroundColor(a_Item.bgColor());
	m_UI->tblItems->setItem(a_Row, 4, wi);
	m_IsInternalChange = false;
}





void DlgEditTemplate::saveAndClose()
{
	save();
	close();
}





void DlgEditTemplate::addItem()
{
	auto item = m_Template.addItem(tr("New item"), QString(), false);
	DlgEditTemplateItem dlg(m_DB, m_MetadataScanner, m_HashCalculator, *item, this);
	dlg.exec();
	m_DB.saveTemplate(m_Template);

	// Add the item in the UI:
	auto idx = m_UI->tblItems->rowCount();
	m_UI->tblItems->setRowCount(idx + 1);
	updateTemplateItemRow(idx, *item);
}





void DlgEditTemplate::editSelectedItem()
{
	const auto & sel = m_UI->tblItems->selectionModel()->selectedRows();
	if (sel.size() != 1)
	{
		return;
	}
	auto row = sel[0].row();
	auto item = m_Template.items()[static_cast<size_t>(row)];
	DlgEditTemplateItem dlg(m_DB, m_MetadataScanner, m_HashCalculator, *item, this);
	dlg.exec();
	m_DB.saveTemplate(m_Template);
	updateTemplateItemRow(row, *item);
}





void DlgEditTemplate::addFavoriteItem()
{
	auto favorites = m_DB.getFavoriteTemplateItems();
	if (favorites.empty())
	{
		return;
	}
	DlgPickTemplateFavoriteItem dlg(std::move(favorites));
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}
	auto item = dlg.itemSelected();
	if (item == nullptr)
	{
		return;
	}
	item = item->clone();
	if (item == nullptr)
	{
		return;
	}
	m_Template.appendExistingItem(item);
	m_DB.saveTemplate(m_Template);

	// Add the item in the UI:
	auto idx = m_UI->tblItems->rowCount();
	m_UI->tblItems->setRowCount(idx + 1);
	updateTemplateItemRow(idx, *item);
}





void DlgEditTemplate::removeSelectedItems()
{
	auto selRows = m_UI->tblItems->selectionModel()->selectedRows();
	if (selRows.isEmpty())
	{
		return;
	}

	// Ask the user:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove items"),
		tr("Are you sure you want to remove the selected items?"),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) != QMessageBox::Yes)
	{
		return;
	}

	// Sort the rows from the last to the first:
	std::vector<int> rows;
	rows.reserve(static_cast<size_t>(selRows.size()));
	for (const auto & row: selRows)
	{
		rows.push_back(row.row());
	}
	std::sort(rows.begin(), rows.end(), std::greater<int>());

	// Remove the rows:
	for (const auto row: rows)
	{
		m_Template.delItem(row);
		m_UI->tblItems->removeRow(row);
	}
	m_DB.saveTemplate(m_Template);
}





void DlgEditTemplate::cellDoubleClicked(int a_Row, int a_Column)
{
	Q_UNUSED(a_Column);

	auto item = m_Template.items()[static_cast<size_t>(a_Row)];
	DlgEditTemplateItem dlg(m_DB, m_MetadataScanner, m_HashCalculator, *item, this);
	dlg.exec();
	m_DB.saveTemplate(m_Template);
	updateTemplateItemRow(a_Row, *item);
}





void DlgEditTemplate::itemSelectionChanged()
{
	const auto & sel = m_UI->tblItems->selectionModel()->selectedRows();
	m_UI->btnEditItem->setEnabled(sel.size() == 1);
	m_UI->btnRemoveItem->setEnabled(!sel.isEmpty());
}





void DlgEditTemplate::bgColorTextChanged(const QString & a_NewText)
{
	QColor c(a_NewText);
	if (!c.isValid())
	{
		m_UI->leBgColor->setStyleSheet({});
		return;
	}
	m_UI->leBgColor->setStyleSheet(QString("background-color: %1").arg(a_NewText));
}





void DlgEditTemplate::chooseBgColor()
{
	auto c = QColorDialog::getColor(
		QColor(m_UI->leBgColor->text()),
		this,
		tr("SkauTan: Choose template color")
	);
	if (c.isValid())
	{
		m_UI->leBgColor->setText(c.name());
	}
}





void DlgEditTemplate::itemChanged(QTableWidgetItem * a_Item)
{
	if (m_IsInternalChange)
	{
		return;
	}
	auto item = m_Template.items()[static_cast<size_t>(a_Item->row())];
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
	}
	m_DB.saveTemplate(m_Template);
}

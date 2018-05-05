#include "DlgEditTemplate.h"
#include <QMessageBox>
#include <QColorDialog>
#include "ui_DlgEditTemplate.h"
#include "DlgEditTemplateItem.h"
#include "DlgPickTemplateFavoriteItem.h"
#include "Database.h"





DlgEditTemplate::DlgEditTemplate(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	Template & a_Template,
	QWidget * a_Parent
):
	QDialog(a_Parent),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_Template(a_Template),
	m_UI(new Ui::DlgEditTemplate)
{
	m_UI->setupUi(this);

	// Connect the slots:
	connect(m_UI->btnClose,           &QPushButton::clicked,   this, &DlgEditTemplate::saveAndClose);
	connect(m_UI->btnAddItem,         &QPushButton::clicked,   this, &DlgEditTemplate::addItem);
	connect(m_UI->btnAddFavoriteItem, &QPushButton::clicked,   this, &DlgEditTemplate::addFavoriteItem);
	connect(m_UI->btnEditItem,        &QPushButton::clicked,   this, &DlgEditTemplate::editSelectedItem);
	connect(m_UI->btnRemoveItem,      &QPushButton::clicked,   this, &DlgEditTemplate::removeSelectedItems);
	connect(m_UI->leBgColor,          &QLineEdit::textChanged, this, &DlgEditTemplate::bgColorTextChanged);
	connect(m_UI->btnBgColor,         &QPushButton::clicked,   this, &DlgEditTemplate::chooseBgColor);
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
	m_UI->tblItems->setColumnCount(4);
	m_UI->tblItems->setHorizontalHeaderLabels({tr("Name"), tr("Notes"), tr("Fav"), tr("Filter")});
	m_UI->leBgColor->setText(a_Template.bgColor().name());
	int idx = 0;
	for (const auto & itm: a_Template.items())
	{
		setItem(idx, *itm);
		idx += 1;
	}
	m_UI->tblItems->resizeColumnsToContents();

	itemSelectionChanged();
}





DlgEditTemplate::~DlgEditTemplate()
{
	// Nothing explicit needed
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





void DlgEditTemplate::setItem(int a_Idx, const Template::Item & a_Item)
{
	auto favDesc = a_Item.isFavorite() ? tr("Y", "IsFavorite") : QString();
	auto filterDesc = a_Item.filter()->getDescription();
	m_UI->tblItems->setItem(a_Idx, 0, new QTableWidgetItem(a_Item.displayName()));
	m_UI->tblItems->setItem(a_Idx, 1, new QTableWidgetItem(a_Item.notes()));
	m_UI->tblItems->setItem(a_Idx, 2, new QTableWidgetItem(favDesc));
	m_UI->tblItems->setItem(a_Idx, 3, new QTableWidgetItem(filterDesc));
	for (int i = 0; i < 4; ++i)
	{
		m_UI->tblItems->item(a_Idx, i)->setBackgroundColor(a_Item.bgColor());
	}
}





void DlgEditTemplate::saveAndClose()
{
	save();
	close();
}





void DlgEditTemplate::addItem()
{
	auto item = m_Template.addItem(tr("New item"), QString(), false);
	DlgEditTemplateItem dlg(m_DB, m_MetadataScanner, *item, this);
	dlg.exec();
	m_DB.saveTemplate(m_Template);

	// Add the item in the UI:
	auto idx = m_UI->tblItems->rowCount();
	m_UI->tblItems->setRowCount(idx + 1);
	setItem(idx, *item);
	m_UI->tblItems->resizeColumnsToContents();
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
	DlgEditTemplateItem dlg(m_DB, m_MetadataScanner, *item, this);
	dlg.exec();
	m_DB.saveTemplate(m_Template);
	setItem(row, *item);
	m_UI->tblItems->resizeColumnsToContents();
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
	setItem(idx, *item);
	m_UI->tblItems->resizeColumnsToContents();
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
		tr("SkauTan - Remove items"),
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
	DlgEditTemplateItem dlg(m_DB, m_MetadataScanner, *item, this);
	dlg.exec();
	m_DB.saveTemplate(m_Template);
	setItem(a_Row, *item);
	m_UI->tblItems->resizeColumnsToContents();
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

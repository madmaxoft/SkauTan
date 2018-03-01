#include "DlgEditTemplate.h"
#include "ui_DlgEditTemplate.h"
#include "DlgEditTemplateItem.h"
#include "DlgPickTemplateFavoriteItem.h"
#include "Database.h"





DlgEditTemplate::DlgEditTemplate(Database & a_DB, Template & a_Template, QWidget * a_Parent):
	QDialog(a_Parent),
	m_DB(a_DB),
	m_Template(a_Template),
	m_UI(new Ui::DlgEditTemplate)
{
	m_UI->setupUi(this);

	// Connect the slots:
	connect(m_UI->btnClose,           &QPushButton::clicked, this, &DlgEditTemplate::saveAndClose);
	connect(m_UI->btnAddItem,         &QPushButton::clicked, this, &DlgEditTemplate::addItem);
	connect(m_UI->btnAddFavoriteItem, &QPushButton::clicked, this, &DlgEditTemplate::addFavoriteItem);
	connect(m_UI->btnEditItem,        &QPushButton::clicked, this, &DlgEditTemplate::editSelectedItem);
	connect(m_UI->btnRemoveItem,      &QPushButton::clicked, this, &DlgEditTemplate::removeSelectedItems);
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
}





void DlgEditTemplate::setItem(int a_Idx, const Template::Item & a_Item)
{
	auto favDesc = a_Item.isFavorite() ? tr("Y", "IsFavorite") : QString();
	auto filterDesc = a_Item.filter()->getDescription();
	m_UI->tblItems->setItem(a_Idx, 0, new QTableWidgetItem(a_Item.displayName()));
	m_UI->tblItems->setItem(a_Idx, 1, new QTableWidgetItem(a_Item.notes()));
	m_UI->tblItems->setItem(a_Idx, 2, new QTableWidgetItem(favDesc));
	m_UI->tblItems->setItem(a_Idx, 3, new QTableWidgetItem(filterDesc));
}





void DlgEditTemplate::saveAndClose()
{
	save();
	close();
}





void DlgEditTemplate::addItem()
{
	auto item = m_Template.addItem(tr("New item"), QString(), false);
	DlgEditTemplateItem dlg(*item, this);
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
	auto item = m_Template.items()[row];
	DlgEditTemplateItem dlg(*item, this);
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
	// TODO
}





void DlgEditTemplate::cellDoubleClicked(int a_Row, int a_Column)
{
	Q_UNUSED(a_Column);

	auto item = m_Template.items()[a_Row];
	DlgEditTemplateItem dlg(*item, this);
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

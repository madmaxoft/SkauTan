#include "DlgPickTemplateFavoriteItem.h"
#include "ui_DlgPickTemplateFavoriteItem.h"
#include <QKeyEvent>





DlgPickTemplateFavoriteItem::DlgPickTemplateFavoriteItem(std::vector<Template::ItemPtr> && a_Items, QWidget * a_Parent):
	Super(a_Parent),
	m_Items(std::move(a_Items)),
	m_UI(new Ui::DlgPickTemplateFavoriteItem)
{
	m_UI->setupUi(this);

	// Connect signals:
	connect(m_UI->tblItems, &QTableWidget::cellDoubleClicked, this, &DlgPickTemplateFavoriteItem::itemDblClicked);

	// Fill in items:
	m_UI->tblItems->setColumnCount(4);
	m_UI->tblItems->setHorizontalHeaderLabels({
		tr("Name",        "HeaderLabel"),
		tr("Notes",       "HeaderLabel"),
		tr("Fav",         "HeaderLabel"),
		tr("Description", "HeaderLabel")
	});
	m_UI->tblItems->setRowCount(static_cast<int>(m_Items.size()));
	int row = 0;
	auto colCount = m_UI->tblItems->columnCount();
	for (const auto & item: m_Items)
	{
		m_UI->tblItems->setItem(row, 0, new QTableWidgetItem(item->displayName()));
		m_UI->tblItems->setItem(row, 1, new QTableWidgetItem(item->notes()));
		m_UI->tblItems->setItem(row, 2, new QTableWidgetItem(item->isFavorite() ? tr("Y", "IsFavorite") : QString()));
		m_UI->tblItems->setItem(row, 3, new QTableWidgetItem(item->filter()->getDescription()));
		for (int col = 0; col < colCount; ++col)
		{
			m_UI->tblItems->item(row, col)->setBackgroundColor(item->bgColor());
		}
		row += 1;
	}
	m_UI->tblItems->resizeColumnsToContents();
	m_UI->tblItems->setCurrentItem(m_UI->tblItems->item(0, 0));
}





DlgPickTemplateFavoriteItem::~DlgPickTemplateFavoriteItem()
{
	// Nothing explicit needed yet
}





void DlgPickTemplateFavoriteItem::keyPressEvent(QKeyEvent * a_Event)
{
	switch (a_Event->key())
	{
		case Qt::Key_Enter:
		case Qt::Key_Return:
		{
			auto curIdx = m_UI->tblItems->currentRow();
			if (curIdx >= 0)
			{
				m_ItemSelected = m_Items[static_cast<size_t>(curIdx)];
				accept();
				return;
			}
			a_Event->ignore();
			return;
		}
	}
	Super::keyPressEvent(a_Event);
}





void DlgPickTemplateFavoriteItem::itemDblClicked(int a_Row, int a_Column)
{
	Q_UNUSED(a_Column);

	if (a_Row >= 0)
	{
		m_ItemSelected = m_Items[static_cast<size_t>(a_Row)];
		accept();
	}
}

#include "DlgPickTemplate.hpp"
#include "ui_DlgPickTemplate.h"
#include <cassert>
#include <QKeyEvent>
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"





DlgPickTemplate::DlgPickTemplate(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::DlgPickTemplate)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgPickTemplate", *this);

	// Connect the signals:
	connect(mUI->tblTemplates, &QTableWidget::cellDoubleClicked, this, &DlgPickTemplate::cellDblClicked);

	// Fill in the templates:
	mUI->tblTemplates->setColumnCount(3);
	mUI->tblTemplates->setRowCount(static_cast<int>(mComponents.get<Database>()->templates().size()));
	mUI->tblTemplates->setHorizontalHeaderLabels({tr("Template"), tr("#"), tr("Notes")});
	int idx = 0;
	auto colCount = mUI->tblTemplates->columnCount();
	for (const auto & tmpl: mComponents.get<Database>()->templates())
	{
		mUI->tblTemplates->setItem(idx, 0, new QTableWidgetItem(tmpl->displayName()));
		mUI->tblTemplates->setItem(idx, 1, new QTableWidgetItem(QString::number(tmpl->items().size())));
		mUI->tblTemplates->setItem(idx, 2, new QTableWidgetItem(tmpl->notes()));
		for (int col = 0; col < colCount; ++col)
		{
			mUI->tblTemplates->item(idx, col)->setBackgroundColor(tmpl->bgColor());
		}
		idx += 1;
	}
	Settings::loadHeaderView("DlgPickTemplate", "tblTemplates", *mUI->tblTemplates->horizontalHeader());
	mUI->tblTemplates->setCurrentItem(mUI->tblTemplates->item(0, 0));
}





DlgPickTemplate::~DlgPickTemplate()
{
	Settings::saveHeaderView("DlgPickTemplate", "tblTemplates", *mUI->tblTemplates->horizontalHeader());
	Settings::saveWindowPos("DlgPickTemplate", *this);
}





void DlgPickTemplate::keyPressEvent(QKeyEvent * aEvent)
{
	switch (aEvent->key())
	{
		case Qt::Key_Enter:
		case Qt::Key_Return:
		{
			auto curIdx = mUI->tblTemplates->currentRow();
			if (curIdx >= 0)
			{
				assert(curIdx < static_cast<int>(mComponents.get<Database>()->templates().size()));
				mSelectedTemplate = mComponents.get<Database>()->templates()[static_cast<size_t>(curIdx)];
				accept();
				return;
			}
			aEvent->ignore();
			return;
		}
	}
	Super::keyPressEvent(aEvent);
}





void DlgPickTemplate::cellDblClicked(int aRow, int aColumn)
{
	Q_UNUSED(aColumn);

	if (aRow >= 0)
	{
		assert(aRow < static_cast<int>(mComponents.get<Database>()->templates().size()));
		mSelectedTemplate = mComponents.get<Database>()->templates()[static_cast<size_t>(aRow)];
		accept();
	}
}

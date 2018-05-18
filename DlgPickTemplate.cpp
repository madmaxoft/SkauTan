#include "DlgPickTemplate.h"
#include "ui_DlgPickTemplate.h"
#include <assert.h>
#include <QKeyEvent>
#include "Database.h"
#include "Settings.h"





DlgPickTemplate::DlgPickTemplate(const Database & a_DB, QWidget * a_Parent):
	Super(a_Parent),
	m_DB(a_DB),
	m_UI(new Ui::DlgPickTemplate)
{
	m_UI->setupUi(this);

	// Connect the signals:
	connect(m_UI->tblTemplates, &QTableWidget::cellDoubleClicked, this, &DlgPickTemplate::cellDblClicked);

	// Fill in the templates:
	m_UI->tblTemplates->setColumnCount(3);
	m_UI->tblTemplates->setRowCount(static_cast<int>(a_DB.templates().size()));
	m_UI->tblTemplates->setHorizontalHeaderLabels({tr("Template"), tr("#"), tr("Notes")});
	int idx = 0;
	auto colCount = m_UI->tblTemplates->columnCount();
	for (const auto & tmpl: a_DB.templates())
	{
		m_UI->tblTemplates->setItem(idx, 0, new QTableWidgetItem(tmpl->displayName()));
		m_UI->tblTemplates->setItem(idx, 1, new QTableWidgetItem(QString::number(tmpl->items().size())));
		m_UI->tblTemplates->setItem(idx, 2, new QTableWidgetItem(tmpl->notes()));
		for (int col = 0; col < colCount; ++col)
		{
			m_UI->tblTemplates->item(idx, col)->setBackgroundColor(tmpl->bgColor());
		}
		idx += 1;
	}
	Settings::loadHeaderView("DlgPickTemplate", "tblTemplates", *m_UI->tblTemplates->horizontalHeader());
	m_UI->tblTemplates->setCurrentItem(m_UI->tblTemplates->item(0, 0));
}





DlgPickTemplate::~DlgPickTemplate()
{
	Settings::saveHeaderView("DlgPickTemplate", "tblTemplates", *m_UI->tblTemplates->horizontalHeader());
}





void DlgPickTemplate::keyPressEvent(QKeyEvent * a_Event)
{
	switch (a_Event->key())
	{
		case Qt::Key_Enter:
		case Qt::Key_Return:
		{
			auto curIdx = m_UI->tblTemplates->currentRow();
			if (curIdx >= 0)
			{
				assert(curIdx < static_cast<int>(m_DB.templates().size()));
				m_SelectedTemplate = m_DB.templates()[static_cast<size_t>(curIdx)];
				accept();
				return;
			}
			a_Event->ignore();
			return;
		}
	}
	Super::keyPressEvent(a_Event);
}





void DlgPickTemplate::cellDblClicked(int a_Row, int a_Column)
{
	Q_UNUSED(a_Column);

	if (a_Row >= 0)
	{
		assert(a_Row < static_cast<int>(m_DB.templates().size()));
		m_SelectedTemplate = m_DB.templates()[static_cast<size_t>(a_Row)];
		accept();
	}
}

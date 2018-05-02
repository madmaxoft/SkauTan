#include "DlgChooseImportTemplates.h"
#include "ui_DlgChooseImportTemplates.h"





DlgChooseImportTemplates::DlgChooseImportTemplates(std::vector<TemplatePtr> && a_Templates, QWidget * a_Parent):
	Super(a_Parent),
	m_Templates(std::move(a_Templates)),
	m_UI(new Ui::DlgChooseImportTemplates)
{
	m_UI->setupUi(this);

	// Connect the signals:
	connect(m_UI->btnCancel,    &QPushButton::clicked,               this, &QDialog::reject);
	connect(m_UI->btnImport,    &QPushButton::clicked,               this, &DlgChooseImportTemplates::import);
	connect(m_UI->tblTemplates, &QTableWidget::itemSelectionChanged, this, &DlgChooseImportTemplates::templateSelectionChanged);
	connect(m_UI->tblTemplates, &QTableWidget::cellChanged,          this, &DlgChooseImportTemplates::templateCellChanged);

	// Update the UI state:
	m_UI->tblTemplates->setRowCount(static_cast<int>(m_Templates.size()));
	m_UI->tblTemplates->setColumnCount(3);
	m_UI->tblTemplates->setHorizontalHeaderLabels({tr("Name"), tr("#"), tr("Notes")});
	m_UI->tblItems->setRowCount(0);
	m_UI->tblItems->setColumnCount(4);
	m_UI->tblItems->setHorizontalHeaderLabels({tr("Name"), tr("Notes"), tr("Fav"), tr("Filter")});
	templateSelectionChanged();

	// Insert all the templates into the table view:
	for (size_t i = 0; i < m_Templates.size(); ++i)
	{
		const auto & tmpl = m_Templates[i];
		updateTemplateRow(static_cast<int>(i), *tmpl);
	}
	m_UI->tblTemplates->resizeColumnsToContents();
}





DlgChooseImportTemplates::~DlgChooseImportTemplates()
{
	// Nothing explicit needed
}





void DlgChooseImportTemplates::updateTemplateRow(int a_Row, const Template & a_Template)
{
	auto item = new QTableWidgetItem(a_Template.displayName());
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
	item->setCheckState(Qt::Checked);
	m_UI->tblTemplates->setItem(a_Row, 0, item);
	m_UI->tblTemplates->setItem(a_Row, 1, new QTableWidgetItem(QString::number(a_Template.items().size())));
	m_UI->tblTemplates->setItem(a_Row, 2, new QTableWidgetItem(a_Template.notes()));
}





void DlgChooseImportTemplates::import()
{
	// Process the check state on templates:
	m_ChosenTemplates.clear();
	int numRows = m_UI->tblTemplates->rowCount();
	for (int row = 0; row < numRows; ++row)
	{
		if (m_UI->tblTemplates->item(row, 0)->checkState() == Qt::Checked)
		{
			m_ChosenTemplates.push_back(m_Templates[static_cast<size_t>(row)]);
		}
	}

	accept();
}





void DlgChooseImportTemplates::templateSelectionChanged()
{
	const auto & selection = m_UI->tblTemplates->selectionModel()->selectedRows();

	// Update the list of items in a template:
	if (selection.size() == 1)
	{
		const auto & tmpl = m_Templates[static_cast<size_t>(selection[0].row())];
		m_UI->tblItems->setRowCount(static_cast<int>(tmpl->items().size()));
		int idx = 0;
		for (const auto & item: tmpl->items())
		{
			auto favDesc = item->isFavorite() ? tr("Y", "IsFavorite") : QString();
			auto filterDesc = item->filter()->getDescription();
			m_UI->tblItems->setItem(idx, 0, new QTableWidgetItem(item->displayName()));
			m_UI->tblItems->setItem(idx, 1, new QTableWidgetItem(item->notes()));
			m_UI->tblItems->setItem(idx, 2, new QTableWidgetItem(favDesc));
			m_UI->tblItems->setItem(idx, 3, new QTableWidgetItem(filterDesc));
			idx += 1;
		}
		m_UI->tblItems->resizeColumnsToContents();
	}
	else
	{
		m_UI->tblItems->setRowCount(0);
	}
}





void DlgChooseImportTemplates::templateCellChanged(int a_Row, int a_Column)
{
	Q_UNUSED(a_Row);
	Q_UNUSED(a_Column);

	int numRows = m_UI->tblTemplates->rowCount();
	for (int i = 0; i < numRows; ++i)
	{
		if (m_UI->tblTemplates->item(i, 0)->checkState() == Qt::Checked)
		{
			m_UI->btnImport->setEnabled(true);
			return;
		}
	}
	m_UI->btnImport->setEnabled(false);
}

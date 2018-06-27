#include "DlgRemovedSongs.h"
#include "ui_DlgRemovedSongs.h"
#include <QFileDialog>
#include <QMessageBox>
#include "Database.h"
#include "Settings.h"





DlgRemovedSongs::DlgRemovedSongs(ComponentCollection & a_Components, QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::DlgRemovedSongs),
	m_Components(a_Components)
{
	m_UI->setupUi(this);

	// Connect the signals:
	connect(m_UI->btnClose,  &QPushButton::pressed, this, &QDialog::reject);
	connect(m_UI->btnClear,  &QPushButton::pressed, this, &DlgRemovedSongs::clearDB);
	connect(m_UI->btnExport, &QPushButton::pressed, this, &DlgRemovedSongs::exportList);

	// Fill in the data:
	auto db = m_Components.get<Database>();
	const auto & removed = db->removedSongs();
	m_UI->twRemoved->setRowCount(static_cast<int>(removed.size()));
	int row = 0;
	for (const auto & r: removed)
	{
		m_UI->twRemoved->setItem(row, 0, new QTableWidgetItem(r->m_DateRemoved.toString(Qt::ISODate)));
		m_UI->twRemoved->setItem(row, 1, new QTableWidgetItem(r->m_FileName));
		auto item = new QTableWidgetItem();
		item->setFlags(Qt::ItemIsUserCheckable);
		item->setCheckState(r->m_WasDeleted ? Qt::Checked : Qt::Unchecked);
		m_UI->twRemoved->setItem(row, 2, item);
		m_UI->twRemoved->setItem(row, 3, new QTableWidgetItem(QString::number(r->m_NumDuplicates)));
		auto sharedData = db->sharedDataFromHash(r->m_Hash);
		if (sharedData != nullptr)
		{
			m_UI->twRemoved->setItem(row, 4, new QTableWidgetItem(QString::number(sharedData->duplicatesCount())));
		}
		row += 1;
	}
	m_UI->twRemoved->resizeColumnsToContents();
	m_UI->twRemoved->resizeRowsToContents();

	Settings::loadHeaderView("DlgRemovedSongs", "twRemoved", *m_UI->twRemoved->horizontalHeader());
	Settings::loadWindowPos("DlgRemovedSongs", *this);
}





DlgRemovedSongs::~DlgRemovedSongs()
{
	Settings::saveWindowPos("DlgRemovedSongs", *this);
	Settings::saveHeaderView("DlgRemovedSongs", "twRemoved", *m_UI->twRemoved->horizontalHeader());
}





void DlgRemovedSongs::clearDB()
{
	if (QMessageBox::question(
		this,
		tr("SkauTan: Clear removed songs"),
		tr("Are you sure you want to clear the history of removed songs?"),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) != QMessageBox::Yes)
	{
		return;
	}
	m_Components.get<Database>()->clearRemovedSongs();
	m_UI->twRemoved->setRowCount(0);
}





void DlgRemovedSongs::exportList()
{
	// Ask for filename:
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Export list of removed songs"),
		QString(),
		tr("Excel sheet (*.xls)")
	);
	if (fileName.isEmpty())
	{
		return;
	}

	// Export the data:
	QFile f(fileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot write to file"),
			tr("Cannot write to file %1: %2").arg(fileName).arg(f.errorString())
		);
		return;
	}
	f.write("Date\tFilename\tWasDeleted\tDuplicatesThen\tDuplicatesNow\n");
	auto db = m_Components.get<Database>();
	const auto & removed = db->removedSongs();
	for (const auto & r: removed)
	{
		f.write(r->m_DateRemoved.toString(Qt::ISODate).toUtf8());
		f.write("\t");
		f.write(r->m_FileName.toUtf8());
		f.write("\t");
		f.write(r->m_WasDeleted ? "1" : "0");
		f.write("\t");
		f.write(QString::number(r->m_NumDuplicates).toUtf8());
		f.write("\t");
		auto sharedData = db->sharedDataFromHash(r->m_Hash);
		if (sharedData != nullptr)
		{
			f.write(QString::number(sharedData->duplicatesCount()).toUtf8());
		}
		f.write("\n");
	}
	QMessageBox::information(
		this,
		tr("SkauTan: Exported"),
		tr("The history of removed songs was exported to %1").arg(fileName)
	);
}

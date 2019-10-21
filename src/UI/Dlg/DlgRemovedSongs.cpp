#include "DlgRemovedSongs.hpp"
#include "ui_DlgRemovedSongs.h"
#include <QFileDialog>
#include <QMessageBox>
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"





DlgRemovedSongs::DlgRemovedSongs(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mUI(new Ui::DlgRemovedSongs),
	mComponents(aComponents)
{
	mUI->setupUi(this);

	// Connect the signals:
	connect(mUI->btnClose,  &QPushButton::pressed, this, &QDialog::reject);
	connect(mUI->btnClear,  &QPushButton::pressed, this, &DlgRemovedSongs::clearDB);
	connect(mUI->btnExport, &QPushButton::pressed, this, &DlgRemovedSongs::exportList);

	// Fill in the data:
	auto db = mComponents.get<Database>();
	const auto & removed = db->removedSongs();
	mUI->twRemoved->setRowCount(static_cast<int>(removed.size()));
	int row = 0;
	for (const auto & r: removed)
	{
		mUI->twRemoved->setItem(row, 0, new QTableWidgetItem(r->mDateRemoved.toString(Qt::ISODate)));
		mUI->twRemoved->setItem(row, 1, new QTableWidgetItem(r->mFileName));
		auto item = new QTableWidgetItem();
		item->setFlags(Qt::ItemIsUserCheckable);
		item->setCheckState(r->mWasDeleted ? Qt::Checked : Qt::Unchecked);
		mUI->twRemoved->setItem(row, 2, item);
		mUI->twRemoved->setItem(row, 3, new QTableWidgetItem(QString::number(r->mNumDuplicates)));
		auto sharedData = db->sharedDataFromHash(r->mHash);
		if (sharedData != nullptr)
		{
			mUI->twRemoved->setItem(row, 4, new QTableWidgetItem(QString::number(sharedData->duplicatesCount())));
		}
		row += 1;
	}
	mUI->twRemoved->resizeColumnsToContents();
	mUI->twRemoved->resizeRowsToContents();

	Settings::loadHeaderView("DlgRemovedSongs", "twRemoved", *mUI->twRemoved->horizontalHeader());
	Settings::loadWindowPos("DlgRemovedSongs", *this);
}





DlgRemovedSongs::~DlgRemovedSongs()
{
	Settings::saveWindowPos("DlgRemovedSongs", *this);
	Settings::saveHeaderView("DlgRemovedSongs", "twRemoved", *mUI->twRemoved->horizontalHeader());
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
	mComponents.get<Database>()->clearRemovedSongs();
	mUI->twRemoved->setRowCount(0);
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
	auto db = mComponents.get<Database>();
	const auto & removed = db->removedSongs();
	for (const auto & r: removed)
	{
		f.write(r->mDateRemoved.toString(Qt::ISODate).toUtf8());
		f.write("\t");
		f.write(r->mFileName.toUtf8());
		f.write("\t");
		f.write(r->mWasDeleted ? "1" : "0");
		f.write("\t");
		f.write(QString::number(r->mNumDuplicates).toUtf8());
		f.write("\t");
		auto sharedData = db->sharedDataFromHash(r->mHash);
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

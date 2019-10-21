#include "DlgLibraryMaintenance.hpp"
#include "ui_DlgLibraryMaintenance.h"
#include <QMessageBox>
#include <QFileDialog>
#include "../../Settings.hpp"
#include "../../BackgroundTasks.hpp"
#include "../../ComponentCollection.hpp"
#include "../../DB/Database.hpp"
#include "../../DB/TagImportExport.hpp"





DlgLibraryMaintenance::DlgLibraryMaintenance(ComponentCollection & aComponents, QWidget *aParent) :
	QDialog(aParent),
	mComponents(aComponents),
	mUI(new Ui::DlgLibraryMaintenance)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgLibraryMaintenance", *this);

	// Connect the signals:
	connect(mUI->btnClose,                   &QPushButton::pressed, this, &QDialog::close);
	connect(mUI->btnRemoveInaccessibleSongs, &QPushButton::pressed, this, &DlgLibraryMaintenance::removeInaccessibleSongs);
	connect(mUI->btnExportAllTags,           &QPushButton::pressed, this, &DlgLibraryMaintenance::exportAllTags);
	connect(mUI->btnImportTags,              &QPushButton::pressed, this, &DlgLibraryMaintenance::importTags);
}





DlgLibraryMaintenance::~DlgLibraryMaintenance()
{
	Settings::saveWindowPos("DlgLibraryMaintenance", *this);
}





void DlgLibraryMaintenance::inaccessibleSongsRemoved(quint32 aNumRemoved)
{
	QMessageBox::information(
		this,
		tr("SkauTan: Inaccessible songs removed"),
		tr("Number of songs removed: %1").arg(aNumRemoved)
	);
}





void DlgLibraryMaintenance::tagExportError(const QString & aFileName, const QString & aError)
{
	QMessageBox::warning(
		this,
		tr("SkauTan: Cannot export tags"),
		tr("SkauTan cannot export tags to file %1:\n\n%2").arg(aFileName).arg(aError)
	);
}





void DlgLibraryMaintenance::tagExportFinished(const QString & aFileName)
{
	QMessageBox::information(
		this,
		tr("SkauTan: Tags exported"),
		tr("SkauTan has successfully exported all tags into file %1.").arg(aFileName)
	);
}





void DlgLibraryMaintenance::tagImportError(const QString & aFileName, const QString & aError)
{
	QMessageBox::warning(
		this,
		tr("SkauTan: Cannot import tags"),
		tr("SkauTan cannot import tags from file %1:\n\n%2").arg(aFileName).arg(aError)
	);
}





void DlgLibraryMaintenance::tagImportFinished(const QString & aFileName)
{
	QMessageBox::information(
		this,
		tr("SkauTan: Tags imported"),
		tr("SkauTan has successfully imported tags from file %1.").arg(aFileName)
	);
}





void DlgLibraryMaintenance::removeInaccessibleSongs()
{
	auto db = mComponents.get<Database>();
	BackgroundTasks::enqueue(tr("Remove inaccessible songs"),
		[db, this]()
		{
			quint32 numRemoved = db->removeInaccessibleSongs();
			QMetaObject::invokeMethod(this, "inaccessibleSongsRemoved", Q_ARG(quint32, numRemoved));
		}
	);
}





void DlgLibraryMaintenance::exportAllTags()
{
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Export all tags"),
		QString(),
		tr("SkauTan tag file (*.SkauTanTags)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	auto db = mComponents.get<Database>();
	BackgroundTasks::enqueue(tr("Export all tags"),
		[this, db, fileName]()
		{
			try
			{
				TagImportExport::doExport(*db, fileName);
			}
			catch (const Exception & exc)
			{
				QMetaObject::invokeMethod(this, "tagExportError", Q_ARG(QString, fileName), Q_ARG(QString, exc.what()));
				return;
			}
			QMetaObject::invokeMethod(this, "tagExportFinished", Q_ARG(QString, fileName));
		}
	);
}





void DlgLibraryMaintenance::importTags()
{
	auto fileName = QFileDialog::getOpenFileName(
		this,
		tr("SkauTan: Import tags"),
		QString(),
		tr("SkauTan tag file (*.SkauTanTags)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	auto db = mComponents.get<Database>();
	BackgroundTasks::enqueue(tr("Import tags"),
		[this, db, fileName]()
		{
			try
			{
				TagImportExport::doImport(*db, fileName);
			}
			catch (const Exception & exc)
			{
				QMetaObject::invokeMethod(this, "tagImportError", Q_ARG(QString, fileName), Q_ARG(QString, exc.what()));
				return;
			}
			QMetaObject::invokeMethod(this, "tagImportFinished", Q_ARG(QString, fileName));
		}
	);
}

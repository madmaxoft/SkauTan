#include "DlgImportDB.hpp"
#include "ui_DlgImportDB.h"
#include <QFileDialog>
#include "../../Settings.hpp"





DlgImportDB::DlgImportDB(QWidget * aParent) :
	Super(aParent),
	mUI(new Ui::DlgImportDB)
{
	mUI->setupUi(this);
	mUI->chbManualTag->setChecked      (Settings::loadValue("DlgImportDB", "chbManualTag.isChecked",       true).toBool());
	mUI->chbDetectedTempo->setChecked  (Settings::loadValue("DlgImportDB", "chbDetectedTempo.isChecked",   true).toBool());
	mUI->chbLastPlayedDate->setChecked (Settings::loadValue("DlgImportDB", "chbLastPlayedDate.isChecked",  true).toBool());
	mUI->chbLocalRating->setChecked    (Settings::loadValue("DlgImportDB", "chbLocalRating.isChecked",     true).toBool());
	mUI->chbCommunityRating->setChecked(Settings::loadValue("DlgImportDB", "chbCommunityRating.isChecked", true).toBool());
	mUI->chbPlaybackHistory->setChecked(Settings::loadValue("DlgImportDB", "chbPlaybackHistory.isChecked", true).toBool());
	mUI->chbSkipStart->setChecked      (Settings::loadValue("DlgImportDB", "chbSkipStart.isChecked",       true).toBool());
	mUI->chbDeletionHistory->setChecked(Settings::loadValue("DlgImportDB", "chbDeletionHistory.isChecked", true).toBool());
	mUI->chbSongColors->setChecked     (Settings::loadValue("DlgImportDB", "chbSongColors.isChecked",      true).toBool());

	connect(mUI->btnBrowse, &QPushButton::clicked, this, &DlgImportDB::browseForDB);
	connect(mUI->btnCancel, &QPushButton::clicked, this, &DlgImportDB::reject);
	connect(mUI->btnImport, &QPushButton::clicked, this, &DlgImportDB::accept);
	connect(this,            &QDialog::finished,    this, &DlgImportDB::onFinished);

	// Ask for the initial file, close if cancelled:
	browseForDB();
	if (mFileName.isEmpty())
	{
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
	}
}





DlgImportDB::~DlgImportDB()
{
	Settings::saveValue("DlgImportDB", "chbManualTag.isChecked",       mUI->chbManualTag->isChecked());
	Settings::saveValue("DlgImportDB", "chbDetectedTempo.isChecked",   mUI->chbDetectedTempo->isChecked());
	Settings::saveValue("DlgImportDB", "chbLastPlayedDate.isChecked",  mUI->chbLastPlayedDate->isChecked());
	Settings::saveValue("DlgImportDB", "chbLocalRating.isChecked",     mUI->chbLocalRating->isChecked());
	Settings::saveValue("DlgImportDB", "chbCommunityRating.isChecked", mUI->chbCommunityRating->isChecked());
	Settings::saveValue("DlgImportDB", "chbPlaybackHistory.isChecked", mUI->chbPlaybackHistory->isChecked());
	Settings::saveValue("DlgImportDB", "chbSkipStart.isChecked",       mUI->chbSkipStart->isChecked());
	Settings::saveValue("DlgImportDB", "chbDeletionHistory.isChecked", mUI->chbDeletionHistory->isChecked());
	Settings::saveValue("DlgImportDB", "chbSongColors.isChecked",      mUI->chbSongColors->isChecked());
}





void DlgImportDB::onFinished(int aResult)
{
	Q_UNUSED(aResult);

	mOptions.mShouldImportManualTag       = mUI->chbManualTag->isChecked();
	mOptions.mShouldImportDetectedTempo   = mUI->chbDetectedTempo->isChecked();
	mOptions.mShouldImportLastPlayedDate  = mUI->chbLastPlayedDate->isChecked();
	mOptions.mShouldImportLocalRating     = mUI->chbLocalRating->isChecked();
	mOptions.mShouldImportCommunityRating = mUI->chbCommunityRating->isChecked();
	mOptions.mShouldImportPlaybackHistory = mUI->chbPlaybackHistory->isChecked();
	mOptions.mShouldImportSkipStart       = mUI->chbSkipStart->isChecked();
	mOptions.mShouldImportDeletionHistory = mUI->chbDeletionHistory->isChecked();
	mOptions.mShouldImportSongColors      = mUI->chbSongColors->isChecked();
}





void DlgImportDB::browseForDB()
{
	auto fileName = QFileDialog::getOpenFileName(
		this,
		tr("SkauTan: Import data from DB"),
		QString(),
		tr("SkauTan Database (*.sqlite)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	mFileName = fileName;
	mUI->leFileName->setText(fileName);
}

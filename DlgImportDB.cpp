#include "DlgImportDB.h"
#include "ui_DlgImportDB.h"
#include <QFileDialog>
#include "Settings.h"





DlgImportDB::DlgImportDB(QWidget * a_Parent) :
	Super(a_Parent),
	m_UI(new Ui::DlgImportDB)
{
	m_UI->setupUi(this);
	m_UI->chbManualTag->setChecked      (Settings::loadValue("DlgImportDB", "chbManualTag.isChecked", true).toBool());
	m_UI->chbLastPlayedDate->setChecked (Settings::loadValue("DlgImportDB", "chbLastPlayedDate.isChecked", true).toBool());
	m_UI->chbLocalRating->setChecked    (Settings::loadValue("DlgImportDB", "chbLocalRating.isChecked", true).toBool());
	m_UI->chbCommunityRating->setChecked(Settings::loadValue("DlgImportDB", "chbCommunityRating.isChecked", true).toBool());
	m_UI->chbPlaybackHistory->setChecked(Settings::loadValue("DlgImportDB", "chbPlaybackHistory.isChecked", true).toBool());
	m_UI->chbSkipStart->setChecked      (Settings::loadValue("DlgImportDB", "chbSkipStart.isChecked", true).toBool());
	m_UI->chbDeletionHistory->setChecked(Settings::loadValue("DlgImportDB", "chbDeletionHistory.isChecked", true).toBool());

	connect(m_UI->btnBrowse, &QPushButton::clicked, this, &DlgImportDB::browseForDB);
	connect(m_UI->btnCancel, &QPushButton::clicked, this, &DlgImportDB::reject);
	connect(m_UI->btnImport, &QPushButton::clicked, this, &DlgImportDB::accept);
	connect(this,            &QDialog::finished,    this, &DlgImportDB::onFinished);

	// Ask for the initial file, close if cancelled:
	browseForDB();
	if (m_FileName.isEmpty())
	{
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
	}
}





DlgImportDB::~DlgImportDB()
{
	Settings::saveValue("DlgImportDB", "chbManualTag.isChecked",       m_UI->chbManualTag->isChecked());
	Settings::saveValue("DlgImportDB", "chbLastPlayedDate.isChecked",  m_UI->chbLastPlayedDate->isChecked());
	Settings::saveValue("DlgImportDB", "chbLocalRating.isChecked",     m_UI->chbLocalRating->isChecked());
	Settings::saveValue("DlgImportDB", "chbCommunityRating.isChecked", m_UI->chbCommunityRating->isChecked());
	Settings::saveValue("DlgImportDB", "chbPlaybackHistory.isChecked", m_UI->chbPlaybackHistory->isChecked());
	Settings::saveValue("DlgImportDB", "chbSkipStart.isChecked",       m_UI->chbSkipStart->isChecked());
	Settings::saveValue("DlgImportDB", "chbDeletionHistory.isChecked", m_UI->chbDeletionHistory->isChecked());
}





void DlgImportDB::onFinished(int a_Result)
{
	Q_UNUSED(a_Result);

	m_Options.m_ShouldImportManualTag       = m_UI->chbManualTag->isChecked();
	m_Options.m_ShouldImportLastPlayedDate  = m_UI->chbLastPlayedDate->isChecked();
	m_Options.m_ShouldImportLocalRating     = m_UI->chbLocalRating->isChecked();
	m_Options.m_ShouldImportCommunityRating = m_UI->chbCommunityRating->isChecked();
	m_Options.m_ShouldImportPlaybackHistory = m_UI->chbPlaybackHistory->isChecked();
	m_Options.m_ShouldImportSkipStart       = m_UI->chbSkipStart->isChecked();
	m_Options.m_ShouldImportDeletionHistory = m_UI->chbDeletionHistory->isChecked();
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
	m_FileName = fileName;
	m_UI->leFileName->setText(fileName);
}

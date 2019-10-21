#include "DlgTapTempo.hpp"
#include "ui_DlgTapTempo.h"
#include "../../Audio/Player.hpp"
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "../../Playlist.hpp"
#include "../../PlaylistItemSong.hpp"





/** Returns the MPM formatted for user output. */
static QString formatMPM(double aMPM)
{
	return QString::number(aMPM, 'f', 1);
}





DlgTapTempo::DlgTapTempo(
	ComponentCollection & aComponents,
	SongPtr aSong,
	QWidget * aParent
):
	Super(aParent),
	mUI(new Ui::DlgTapTempo),
	mComponents(aComponents),
	mSong(aSong)
{
	mUI->setupUi(this);
	Settings::loadHeaderView("DlgTapTempo", "twMeasures", *mUI->twMeasures->horizontalHeader());

	// Connect the signals:
	connect(mUI->btnNow,    &QPushButton::pressed, this, &DlgTapTempo::buttonPressed);
	connect(mUI->btnCancel, &QPushButton::pressed, this, &DlgTapTempo::reject);
	connect(mUI->btnSave,   &QPushButton::pressed, this, &DlgTapTempo::saveAndClose);

	// Start the playback:
	mPlayer.reset(new Player);
	mPlayer->playlist().addItem(std::make_shared<PlaylistItemSong>(mSong, nullptr));
	mPlayer->startPlayback();
}





DlgTapTempo::~DlgTapTempo()
{
	Settings::saveHeaderView("DlgTapTempo", "twMeasures", *mUI->twMeasures->horizontalHeader());
}





void DlgTapTempo::clearTimePoints()
{
	mTimePoints.clear();
	mUI->twMeasures->setRowCount(0);
	mUI->lblDetectionResults->setText(tr("Detected MPM: --"));
	mUI->btnSave->setEnabled(false);
}





void DlgTapTempo::addTimePoint(qint64 aMSecElapsed)
{
	mTimePoints.push_back(aMSecElapsed);
	auto row = mUI->twMeasures->rowCount();
	auto tempo = 60000.0 / aMSecElapsed;
	mUI->twMeasures->setRowCount(row + 1);
	mUI->twMeasures->setItem(row, 0, new QTableWidgetItem(QString::number(aMSecElapsed)));
	mUI->twMeasures->setItem(row, 1, new QTableWidgetItem(formatMPM(tempo)));
	mUI->twMeasures->setItem(row, 2, new QTableWidgetItem(formatMPM(Song::adjustMpm(tempo, mSong->primaryGenre().valueOrDefault()))));
	mUI->twMeasures->resizeRowToContents(row);
	auto overallMPM = detectMPM();
	mUI->lblDetectionResults->setText(tr("Detected average MPM: %1").arg(formatMPM(overallMPM)));
	mUI->btnSave->setEnabled(true);
}





double DlgTapTempo::detectMPM()
{
	double sum = 0;
	for (const auto & tp: mTimePoints)
	{
		sum += tp;
	}
	auto tempo = (static_cast<double>(mTimePoints.size()) * 60000) / sum;
	return Song::adjustMpm(tempo, mSong->primaryGenre().valueOrDefault());
}





void DlgTapTempo::buttonPressed()
{
	if (!mTimer.isValid())
	{
		// This is the first press of the button, start the timer
		mTimer.start();
		return;
	}
	auto elapsed = mTimer.restart();
	if (elapsed > 4000)
	{
		// Longer than 4 seconds - we assume the user has abandoned the previous attempt and is re-starting
		clearTimePoints();
		return;
	}
	addTimePoint(elapsed);
}





void DlgTapTempo::saveAndClose()
{
	if (!mTimePoints.empty())
	{
		mSong->setManualMeasuresPerMinute(static_cast<int>(detectMPM() * 10) / 10.0);
		mComponents.get<Database>()->saveSong(mSong);
	}
	accept();
}

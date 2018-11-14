#include "DlgTapTempo.hpp"
#include "ui_DlgTapTempo.h"
#include "../../Audio/Player.hpp"
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "../../Playlist.hpp"
#include "../../PlaylistItemSong.hpp"





/** Returns the MPM formatted for user output. */
static QString formatMPM(double a_MPM)
{
	return QString::number(a_MPM, 'f', 1);
}





DlgTapTempo::DlgTapTempo(
	ComponentCollection & a_Components,
	SongPtr a_Song,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_UI(new Ui::DlgTapTempo),
	m_Components(a_Components),
	m_Song(a_Song)
{
	m_UI->setupUi(this);
	Settings::loadHeaderView("DlgTapTempo", "twMeasures", *m_UI->twMeasures->horizontalHeader());

	// Connect the signals:
	connect(m_UI->btnNow,    &QPushButton::pressed, this, &DlgTapTempo::buttonPressed);
	connect(m_UI->btnCancel, &QPushButton::pressed, this, &DlgTapTempo::reject);
	connect(m_UI->btnSave,   &QPushButton::pressed, this, &DlgTapTempo::saveAndClose);

	// Start the playback:
	m_Player.reset(new Player);
	m_Player->playlist().addItem(std::make_shared<PlaylistItemSong>(m_Song, nullptr));
	m_Player->startPlayback();
}





DlgTapTempo::~DlgTapTempo()
{
	Settings::saveHeaderView("DlgTapTempo", "twMeasures", *m_UI->twMeasures->horizontalHeader());
}





void DlgTapTempo::clearTimePoints()
{
	m_TimePoints.clear();
	m_UI->twMeasures->setRowCount(0);
	m_UI->lblDetectionResults->setText(tr("Detected MPM: --"));
	m_UI->btnSave->setEnabled(false);
}





void DlgTapTempo::addTimePoint(qint64 a_MSecElapsed)
{
	m_TimePoints.push_back(a_MSecElapsed);
	auto row = m_UI->twMeasures->rowCount();
	auto tempo = 60000.0 / a_MSecElapsed;
	m_UI->twMeasures->setRowCount(row + 1);
	m_UI->twMeasures->setItem(row, 0, new QTableWidgetItem(QString::number(a_MSecElapsed)));
	m_UI->twMeasures->setItem(row, 1, new QTableWidgetItem(formatMPM(tempo)));
	m_UI->twMeasures->setItem(row, 2, new QTableWidgetItem(formatMPM(Song::adjustMpm(tempo, m_Song->primaryGenre().valueOrDefault()))));
	m_UI->twMeasures->resizeRowToContents(row);
	auto overallMPM = detectMPM();
	m_UI->lblDetectionResults->setText(tr("Detected average MPM: %1").arg(formatMPM(overallMPM)));
	m_UI->btnSave->setEnabled(true);
}





double DlgTapTempo::detectMPM()
{
	double sum = 0;
	for (const auto & tp: m_TimePoints)
	{
		sum += tp;
	}
	auto tempo = (static_cast<double>(m_TimePoints.size()) * 60000) / sum;
	return Song::adjustMpm(tempo, m_Song->primaryGenre().valueOrDefault());
}





void DlgTapTempo::buttonPressed()
{
	if (!m_Timer.isValid())
	{
		// This is the first press of the button, start the timer
		m_Timer.start();
		return;
	}
	auto elapsed = m_Timer.restart();
	if (elapsed > 3000)
	{
		// Longer than 3 seconds - we assume the user has abandoned the previous attempt and is re-starting
		clearTimePoints();
		return;
	}
	addTimePoint(elapsed);
}





void DlgTapTempo::saveAndClose()
{
	if (!m_TimePoints.empty())
	{
		m_Song->setManualMeasuresPerMinute(static_cast<int>(detectMPM() * 10) / 10.0);
		m_Components.get<Database>()->saveSong(m_Song);
	}
	reject();
}

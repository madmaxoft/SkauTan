#include "DlgTempoDetect.hpp"
#include <cassert>
#include <QTimer>
#include <QFileDialog>
#include "ui_DlgTempoDetect.h"
#include "../../DB/Database.hpp"
#include "../../Utils.hpp"
#include "../../Settings.hpp"
#include "../../ComponentCollection.hpp"





static QString formatMPM(const DatedOptional<double> & a_SongMPM)
{
	if (!a_SongMPM.isPresent())
	{
		return QString();
	}
	return QLocale().toString(a_SongMPM.value(), 'f', 1);
}





static QString levelAlgorithmToStr(TempoDetector::ELevelAlgorithm a_Alg)
{
	switch (a_Alg)
	{
		case TempoDetector::laSumDist:               return "SumDist";
		case TempoDetector::laMinMax:                return "MinMax";
		case TempoDetector::laDiscreetSineTransform: return "DiscreetSineTransform";
		case TempoDetector::laSumDistMinMax:         return "SumDistMinMax";
	}
	assert(!"Unknown level algorithm");
	return "<unknown>";
}





////////////////////////////////////////////////////////////////////////////////
// DlgTempoDetect:

DlgTempoDetect::DlgTempoDetect(ComponentCollection & a_Components, SongPtr a_Song, QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::DlgTempoDetect),
	m_Components(a_Components),
	m_Song(a_Song),
	m_TempoDetectDelay(0),
	m_Detector(new TempoDetector)
{
	// Init the UI:
	m_UI->setupUi(this);
	initOptionsUi();
	m_UI->twDetectionHistory->resizeColumnsToContents();  // Default column widths
	Settings::loadWindowPos("DlgTempoDetect", *this);
	Settings::loadHeaderView("DlgTempoDetect", "twDetectionHistory", *m_UI->twDetectionHistory->horizontalHeader());

	// Connect the signals:
	connect(m_UI->btnClose,                    &QPushButton::pressed,           this, &QDialog::reject);
	connect(m_UI->leSampleRate,                &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leLocalMaxDistance,          &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leMaxTempo,                  &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leMinTempo,                  &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leNormalizeLevelsWindowSize, &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->lwLevelAlgorithm,            &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(m_UI->lwWindowSize,                &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(m_UI->lwStride,                    &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(m_UI->btnSaveDebugBeats,           &QPushButton::pressed,           this, &DlgTempoDetect::saveDebugBeats);
	connect(m_UI->btnSaveDebugLevels,          &QPushButton::pressed,           this, &DlgTempoDetect::saveDebugLevels);
	connect(m_Detector.get(),                  &TempoDetector::songScanned,     this, &DlgTempoDetect::songScanned);

	// Start the timer for delayed tempo detection:
	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, [this]()
		{
			if (m_TempoDetectDelay > 0)
			{
				m_TempoDetectDelay -= 1;
				if (m_TempoDetectDelay == 1)
				{
					detectTempo();
				}
			}
		}
	);
	timer->start(50);

	// Fill in the song details:
	auto info = m_Song->tagId3().m_Author.valueOrDefault();
	if (!m_Song->tagId3().m_Title.isEmpty())
	{
		if (!info.isEmpty())
		{
			info += " - ";
		}
		info += m_Song->tagId3().m_Title.value();
	}
	if (info.isEmpty())
	{
		m_UI->lblSongInfo->setText(m_Song->fileName());
	}
	else
	{
		m_UI->lblSongInfo->setText(info + " (" + m_Song->fileName() + ")");
	}
	m_UI->leMpmManual->setText(formatMPM(m_Song->tagManual().m_MeasuresPerMinute));
	m_UI->leMpmId3->setText(formatMPM(m_Song->tagId3().m_MeasuresPerMinute));
	m_UI->leMpmFilename->setText(formatMPM(m_Song->tagFileName().m_MeasuresPerMinute));

	// Detect using the initial options:
	detectTempo();
}





DlgTempoDetect::~DlgTempoDetect()
{
	Settings::saveWindowPos("DlgTempoDetect", *this);
}





void DlgTempoDetect::initOptionsUi()
{
	// LevelAlgorithms:
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laSumDist),               m_UI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laSumDist);
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laMinMax),                m_UI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laMinMax);
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laDiscreetSineTransform), m_UI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laDiscreetSineTransform);
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laSumDistMinMax),         m_UI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laSumDistMinMax);

	// Window size:
	(new QListWidgetItem(   "9", m_UI->lwWindowSize))->setData(Qt::UserRole,    9);
	(new QListWidgetItem(  "10", m_UI->lwWindowSize))->setData(Qt::UserRole,   10);
	(new QListWidgetItem(  "11", m_UI->lwWindowSize))->setData(Qt::UserRole,   11);
	(new QListWidgetItem(  "12", m_UI->lwWindowSize))->setData(Qt::UserRole,   12);
	(new QListWidgetItem(  "13", m_UI->lwWindowSize))->setData(Qt::UserRole,   13);
	(new QListWidgetItem(  "14", m_UI->lwWindowSize))->setData(Qt::UserRole,   14);
	(new QListWidgetItem(  "15", m_UI->lwWindowSize))->setData(Qt::UserRole,   15);
	(new QListWidgetItem(  "16", m_UI->lwWindowSize))->setData(Qt::UserRole,   16);
	(new QListWidgetItem(  "17", m_UI->lwWindowSize))->setData(Qt::UserRole,   17);
	(new QListWidgetItem(  "18", m_UI->lwWindowSize))->setData(Qt::UserRole,   18);
	(new QListWidgetItem(  "32", m_UI->lwWindowSize))->setData(Qt::UserRole,   32);
	(new QListWidgetItem(  "64", m_UI->lwWindowSize))->setData(Qt::UserRole,   64);
	(new QListWidgetItem( "128", m_UI->lwWindowSize))->setData(Qt::UserRole,  128);
	(new QListWidgetItem( "256", m_UI->lwWindowSize))->setData(Qt::UserRole,  256);
	(new QListWidgetItem( "512", m_UI->lwWindowSize))->setData(Qt::UserRole,  512);
	(new QListWidgetItem("1024", m_UI->lwWindowSize))->setData(Qt::UserRole, 1024);
	(new QListWidgetItem("2048", m_UI->lwWindowSize))->setData(Qt::UserRole, 2048);
	(new QListWidgetItem("4096", m_UI->lwWindowSize))->setData(Qt::UserRole, 4096);
	(new QListWidgetItem("8192", m_UI->lwWindowSize))->setData(Qt::UserRole, 8192);

	// Stride:
	(new QListWidgetItem( "4", m_UI->lwStride))->setData(Qt::UserRole,  4);
	(new QListWidgetItem( "5", m_UI->lwStride))->setData(Qt::UserRole,  5);
	(new QListWidgetItem( "6", m_UI->lwStride))->setData(Qt::UserRole,  6);
	(new QListWidgetItem( "7", m_UI->lwStride))->setData(Qt::UserRole,  7);
	(new QListWidgetItem( "8", m_UI->lwStride))->setData(Qt::UserRole,  8);
	(new QListWidgetItem( "9", m_UI->lwStride))->setData(Qt::UserRole,  9);
	(new QListWidgetItem("10", m_UI->lwStride))->setData(Qt::UserRole, 10);
	(new QListWidgetItem("11", m_UI->lwStride))->setData(Qt::UserRole, 11);
	(new QListWidgetItem("12", m_UI->lwStride))->setData(Qt::UserRole, 12);
	(new QListWidgetItem("13", m_UI->lwStride))->setData(Qt::UserRole, 13);
	(new QListWidgetItem("14", m_UI->lwStride))->setData(Qt::UserRole, 14);
	(new QListWidgetItem("15", m_UI->lwStride))->setData(Qt::UserRole, 15);
	(new QListWidgetItem("1024", m_UI->lwStride))->setData(Qt::UserRole, 1024);
	(new QListWidgetItem("2048", m_UI->lwStride))->setData(Qt::UserRole, 2048);
	(new QListWidgetItem("4096", m_UI->lwStride))->setData(Qt::UserRole, 4096);
	(new QListWidgetItem("8192", m_UI->lwStride))->setData(Qt::UserRole, 8192);

	// Select the defaults:
	TempoDetector::Options opt;
	auto genre = m_Song->primaryGenre().valueOrDefault();
	std::tie(opt.m_MinTempo, opt.m_MaxTempo) = Song::detectionTempoRangeForGenre(genre);
	selectOptions(opt);
}





void DlgTempoDetect::selectOptions(const TempoDetector::Options & a_Options)
{
	m_IsInternalChange = true;
	Utils::selectItemWithData(m_UI->lwLevelAlgorithm, a_Options.m_LevelAlgorithm);
	Utils::selectItemWithData(m_UI->lwWindowSize,     static_cast<qulonglong>(a_Options.m_WindowSize));
	Utils::selectItemWithData(m_UI->lwStride,         static_cast<qulonglong>(a_Options.m_Stride));
	m_UI->leSampleRate->setText(QString::number(a_Options.m_SampleRate));
	m_UI->leLocalMaxDistance->setText(QString::number(a_Options.m_LocalMaxDistance));
	m_UI->leMinTempo->setText(QString::number(a_Options.m_MinTempo));
	m_UI->leMaxTempo->setText(QString::number(a_Options.m_MaxTempo));
	m_UI->leNormalizeLevelsWindowSize->setText(QString::number(a_Options.m_NormalizeLevelsWindowSize));
	m_IsInternalChange = false;
}





TempoDetector::Options DlgTempoDetect::readOptionsFromUi()
{
	TempoDetector::Options options;
	options.m_LevelAlgorithm = static_cast<TempoDetector::ELevelAlgorithm>(m_UI->lwLevelAlgorithm->currentItem()->data(Qt::UserRole).toInt());
	options.m_WindowSize = static_cast<size_t>(m_UI->lwWindowSize->currentItem()->data(Qt::UserRole).toLongLong());
	options.m_Stride = static_cast<size_t>(m_UI->lwStride->currentItem()->data(Qt::UserRole).toLongLong());
	options.m_SampleRate = m_UI->leSampleRate->text().toInt();
	options.m_LocalMaxDistance = static_cast<size_t>(m_UI->leLocalMaxDistance->text().toLongLong());
	options.m_MinTempo = m_UI->leMinTempo->text().toInt();
	options.m_MaxTempo = m_UI->leMaxTempo->text().toInt();
	options.m_NormalizeLevelsWindowSize = static_cast<size_t>(m_UI->leNormalizeLevelsWindowSize->text().toLongLong());
	options.m_ShouldNormalizeLevels = (options.m_NormalizeLevelsWindowSize > 0);
	return options;
}





void DlgTempoDetect::updateHistoryRow(int a_Row)
{
	const auto & res = m_History[static_cast<size_t>(a_Row)];
	const auto & opt = res->m_Options[0];
	m_UI->twDetectionHistory->setItem(a_Row, 0,  new QTableWidgetItem(levelAlgorithmToStr(opt.m_LevelAlgorithm)));
	m_UI->twDetectionHistory->setItem(a_Row, 1,  new QTableWidgetItem(QString::number(opt.m_WindowSize)));
	m_UI->twDetectionHistory->setItem(a_Row, 2,  new QTableWidgetItem(QString::number(opt.m_Stride)));
	m_UI->twDetectionHistory->setItem(a_Row, 3,  new QTableWidgetItem(QString::number(opt.m_SampleRate)));
	m_UI->twDetectionHistory->setItem(a_Row, 4,  new QTableWidgetItem(QString::number(opt.m_LocalMaxDistance)));
	m_UI->twDetectionHistory->setItem(a_Row, 5,  new QTableWidgetItem(QString::number(opt.m_MinTempo)));
	m_UI->twDetectionHistory->setItem(a_Row, 6,  new QTableWidgetItem(QString::number(opt.m_MaxTempo)));
	m_UI->twDetectionHistory->setItem(a_Row, 7,  new QTableWidgetItem(QString::number(opt.m_NormalizeLevelsWindowSize)));
	m_UI->twDetectionHistory->setItem(a_Row, 8,  new QTableWidgetItem(QString::number(res->m_Tempo)));
	double mpm;
	if (m_Song->primaryGenre().isPresent())
	{
		mpm = Song::adjustMpm(res->m_Tempo, m_Song->primaryGenre().valueOrDefault());
	}
	else
	{
		mpm = res->m_Tempo;
	}
	m_UI->twDetectionHistory->setItem(a_Row, 9,  new QTableWidgetItem(QString::number(mpm, 'f', 1)));
	m_UI->twDetectionHistory->setItem(a_Row, 10, new QTableWidgetItem(QString::number(res->m_Confidence)));
	auto btn = new QPushButton(tr("Use"));
	m_UI->twDetectionHistory->setCellWidget(a_Row, 11, btn);
	connect(btn, &QPushButton::pressed, [this, mpm]()
		{
			m_Song->setManualMeasuresPerMinute(mpm);
			m_UI->leMpmManual->setText(formatMPM(m_Song->tagManual().m_MeasuresPerMinute));
			m_Components.get<Database>()->saveSong(m_Song);
		}
	);
	m_UI->twDetectionHistory->resizeRowToContents(a_Row);
}





void DlgTempoDetect::delayedDetectTempo()
{
	m_TempoDetectDelay = 10;  // 0.5 sec
}





void DlgTempoDetect::detectTempo()
{
	auto options = readOptionsFromUi();
	for (const auto & h: m_History)
	{
		if (h->m_Options[0] == options)
		{
			// This set of options has already been calculated, bail out:
			return;
		}
	}

	// Start the detection:
	m_Detector->queueScanSong(m_Song, {options});
}





void DlgTempoDetect::songScanned(SongPtr a_Song, TempoDetector::ResultPtr a_Result)
{
	Q_UNUSED(a_Song);

	// Add to history:
	m_History.push_back(a_Result);
	m_UI->twDetectionHistory->setRowCount(static_cast<int>(m_History.size()));
	updateHistoryRow(static_cast<int>(m_History.size()) - 1);
	m_UI->twDetectionHistory->resizeColumnsToContents();
}





void DlgTempoDetect::saveDebugBeats()
{
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Save debug audio with Beats"),
		QString(),
		tr("RAW audio file (*.raw)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	auto options = readOptionsFromUi();
	options.m_DebugAudioBeatsFileName = fileName;
	m_Detector->queueScanSong(m_Song, {options});
}





void DlgTempoDetect::saveDebugLevels()
{
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Save debug audio with Levels"),
		QString(),
		tr("RAW audio file (*.raw)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	auto options = readOptionsFromUi();
	options.m_DebugAudioLevelsFileName = fileName;
	m_Detector->queueScanSong(m_Song, {options});
}






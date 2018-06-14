#include "DlgTempoDetect.h"
#include <assert.h>
#include <QTimer>
#include <QFileDialog>
#include "ui_DlgTempoDetect.h"
#include "Utils.h"
#include "Settings.h"
#include "Database.h"





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
		case TempoDetector::laSumDist: return "SumDist";
		case TempoDetector::laMinMax:  return "MinMax";
		case TempoDetector::laDiscreetSineTransform: return "DiscreetSineTransform";
	}
	assert(!"Unknown level algorithm");
	return "<unknown>";
}





/** Converts the detected tempo to MPM, using the specified genre as range guide. */
static int mpmFromGenreTempo(int a_Tempo, const QString & a_Genre)
{
	auto range = Song::competitionTempoRangeForGenre(a_Genre);
	if (range.first <= 0.1)
	{
		qDebug() << "No valid range for genre " << a_Genre << ", bailing out of MPM adjustment.";
		return a_Tempo;
	}
	range.first = range.first * 0.8;  // Allow 20 % slower songs
	range.second = range.second * 1.1;  // Allow 10 % faster songs
	if (a_Tempo < range.first)
	{
		while (a_Tempo < range.first)
		{
			a_Tempo = a_Tempo * 2;
		}
		return a_Tempo;
	}

	// The tempo is faster than the regular range
	// For SW, VW and BL, try first if it makes sense to divide by 3:
	if ((a_Genre == "SW") || (a_Genre == "VW") || (a_Genre == "BL"))
	{
		for (int d: {1, 2, 4, 8, 16})
		{
			if (Utils::isInRange<double>((a_Tempo / d) / 3, range.first, range.second))
			{
				return (a_Tempo / d) / 3;
			}
		}
	}

	// All other genres only half the tempo until it is lower than the max:
	while (a_Tempo > range.second)
	{
		a_Tempo = a_Tempo / 2;
	}
	return a_Tempo;
}





////////////////////////////////////////////////////////////////////////////////
// DlgTempoDetect:

DlgTempoDetect::DlgTempoDetect(Database & a_DB, SongPtr a_Song, QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::DlgTempoDetect),
	m_DB(a_DB),
	m_Song(a_Song),
	m_TempoDetectDelay(0),
	m_Detector(new TempoDetector)
{
	// Init the UI:
	m_UI->setupUi(this);
	initOptionsUi();
	m_ProgressLabel.reset(new QLabel(m_UI->twDetectedResults->viewport()));
	m_ProgressLabel->hide();
	m_ProgressLabel->setText(tr("Detection in progress..."));
	m_ProgressLabel->setAlignment(Qt::AlignCenter);
	m_UI->twDetectionHistory->resizeColumnsToContents();  // Default column widths
	Settings::loadWindowPos("DlgTempoDetect", *this);
	Settings::loadHeaderView("DlgTempoDetect", "twDetectedResults",  *m_UI->twDetectedResults->horizontalHeader());
	Settings::loadHeaderView("DlgTempoDetect", "twDetectionHistory", *m_UI->twDetectionHistory->horizontalHeader());

	// Connect the signals:
	connect(m_UI->btnClose,           &QPushButton::pressed,           this, &QDialog::reject);
	connect(m_UI->leFoldHistogramMax, &QLineEdit::textChanged,         this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leFoldHistogramMin, &QLineEdit::textChanged,         this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leHistogramCutoff,  &QLineEdit::textChanged,         this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leLevelAvg,         &QLineEdit::textChanged,         this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->leLevelPeak,        &QLineEdit::textChanged,         this, &DlgTempoDetect::delayedDetectTempo);
	connect(m_UI->lwLevelAlgorithm,   &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(m_UI->lwWindowSize,       &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(m_UI->lwStride,           &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(m_UI->btnSaveDebugBeats,  &QPushButton::pressed,           this, &DlgTempoDetect::saveDebugBeats);
	connect(m_UI->btnSaveDebugLevels, &QPushButton::pressed,           this, &DlgTempoDetect::saveDebugLevels);
	connect(m_Detector.get(),         &TempoDetector::songScanned,     this, &DlgTempoDetect::songScanned);

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

	// Fill in the default options:
	selectOptions(TempoDetector::Options());

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

	// Window size:
	(new QListWidgetItem( "256", m_UI->lwWindowSize))->setData(Qt::UserRole,  256);
	(new QListWidgetItem( "512", m_UI->lwWindowSize))->setData(Qt::UserRole,  512);
	(new QListWidgetItem("1024", m_UI->lwWindowSize))->setData(Qt::UserRole, 1024);
	(new QListWidgetItem("2048", m_UI->lwWindowSize))->setData(Qt::UserRole, 2048);
	(new QListWidgetItem("4096", m_UI->lwWindowSize))->setData(Qt::UserRole, 4096);
	(new QListWidgetItem("8192", m_UI->lwWindowSize))->setData(Qt::UserRole, 8192);

	// Stride:
	(new QListWidgetItem( "256", m_UI->lwStride))->setData(Qt::UserRole,  256);
	(new QListWidgetItem( "512", m_UI->lwStride))->setData(Qt::UserRole,  512);
	(new QListWidgetItem("1024", m_UI->lwStride))->setData(Qt::UserRole, 1024);
	(new QListWidgetItem("2048", m_UI->lwStride))->setData(Qt::UserRole, 2048);
	(new QListWidgetItem("4096", m_UI->lwStride))->setData(Qt::UserRole, 4096);
	(new QListWidgetItem("8192", m_UI->lwStride))->setData(Qt::UserRole, 8192);

	// Select the defaults:
	selectOptions(TempoDetector::Options());
}





void DlgTempoDetect::selectOptions(const TempoDetector::Options & a_Options)
{
	m_IsInternalChange = true;
	Utils::selectItemWithData(m_UI->lwLevelAlgorithm, a_Options.m_LevelAlgorithm);
	Utils::selectItemWithData(m_UI->lwWindowSize,     static_cast<qulonglong>(a_Options.m_WindowSize));
	Utils::selectItemWithData(m_UI->lwStride,         static_cast<qulonglong>(a_Options.m_Stride));
	m_UI->leLevelAvg->setText(        QString::number(a_Options.m_LevelAvg));
	m_UI->leLevelPeak->setText(       QString::number(a_Options.m_LevelPeak));
	m_UI->leHistogramCutoff->setText( QString::number(a_Options.m_HistogramCutoff));
	m_UI->leFoldHistogramMin->setText(QString::number(a_Options.m_HistogramFoldMin));
	m_UI->leFoldHistogramMax->setText(QString::number(a_Options.m_HistogramFoldMax));
	m_UI->chbFoldHistogram->setChecked(a_Options.m_ShouldFoldHistogram);
	m_IsInternalChange = false;
}





TempoDetector::Options DlgTempoDetect::readOptionsFromUi()
{
	TempoDetector::Options options;
	options.m_LevelAlgorithm = static_cast<TempoDetector::ELevelAlgorithm>(m_UI->lwLevelAlgorithm->currentItem()->data(Qt::UserRole).toInt());
	options.m_WindowSize = static_cast<size_t>(m_UI->lwWindowSize->currentItem()->data(Qt::UserRole).toLongLong());
	options.m_Stride = static_cast<size_t>(m_UI->lwStride->currentItem()->data(Qt::UserRole).toLongLong());
	options.m_LevelAvg = static_cast<size_t>(m_UI->leLevelAvg->text().toLongLong());
	options.m_LevelPeak = static_cast<size_t>(m_UI->leLevelPeak->text().toLongLong());
	options.m_HistogramCutoff = static_cast<size_t>(m_UI->leHistogramCutoff->text().toLongLong());
	options.m_ShouldFoldHistogram = m_UI->chbFoldHistogram->isChecked();
	if (options.m_ShouldFoldHistogram)
	{
		options.m_HistogramFoldMin = m_UI->leFoldHistogramMin->text().toInt();
		options.m_HistogramFoldMax = m_UI->leFoldHistogramMax->text().toInt();
	}
	return options;
}





void DlgTempoDetect::fillInResults(const TempoDetector::Result & a_Results)
{
	m_UI->twDetectedResults->clearContents();
	int row = 0;
	for (const auto & c: a_Results.m_Confidences)
	{
		m_UI->twDetectedResults->setItem(row, 0, new QTableWidgetItem(QString::number(c.first)));
		m_UI->twDetectedResults->setItem(row, 1, new QTableWidgetItem(QString::number(c.second)));
		row += 1;
	}
}





void DlgTempoDetect::updateHistoryRow(int a_Row)
{
	const auto & res = m_History[static_cast<size_t>(a_Row)];
	const auto & opt = res->m_Options;
	m_UI->twDetectionHistory->setItem(a_Row, 0,  new QTableWidgetItem(levelAlgorithmToStr(opt.m_LevelAlgorithm)));
	m_UI->twDetectionHistory->setItem(a_Row, 1,  new QTableWidgetItem(QString::number(opt.m_WindowSize)));
	m_UI->twDetectionHistory->setItem(a_Row, 2,  new QTableWidgetItem(QString::number(opt.m_Stride)));
	m_UI->twDetectionHistory->setItem(a_Row, 3,  new QTableWidgetItem(QString::number(opt.m_LevelAvg)));
	m_UI->twDetectionHistory->setItem(a_Row, 4,  new QTableWidgetItem(QString::number(opt.m_LevelPeak)));
	m_UI->twDetectionHistory->setItem(a_Row, 5,  new QTableWidgetItem(QString::number(opt.m_HistogramCutoff)));
	m_UI->twDetectionHistory->setItem(a_Row, 6,  new QTableWidgetItem(opt.m_ShouldFoldHistogram ? QString::number(opt.m_HistogramFoldMin) : QString()));
	m_UI->twDetectionHistory->setItem(a_Row, 7,  new QTableWidgetItem(opt.m_ShouldFoldHistogram ? QString::number(opt.m_HistogramFoldMax) : QString()));
	m_UI->twDetectionHistory->setItem(a_Row, 8,  new QTableWidgetItem(QString::number(res->m_Tempo)));
	if (m_Song->primaryGenre().isPresent())
	{
		auto mpm = mpmFromGenreTempo(res->m_Tempo, m_Song->primaryGenre().value());
		m_UI->twDetectionHistory->setItem(a_Row, 9,  new QTableWidgetItem(QString::number(mpm)));
		auto btn = new QPushButton(tr("Use"));
		m_UI->twDetectionHistory->setCellWidget(a_Row, 10, btn);
		connect(btn, &QPushButton::pressed, [this, mpm]()
			{
				m_Song->setManualMeasuresPerMinute(mpm);
				m_UI->leMpmManual->setText(formatMPM(m_Song->tagManual().m_MeasuresPerMinute));
				m_DB.saveSong(m_Song);
			}
		);
	}
	m_UI->twDetectionHistory->setItem(a_Row, 11, new QTableWidgetItem(QString::number(res->m_Confidence)));
	if ((res->m_Confidences.size() > 1) && (res->m_Confidences[0].second > 0))
	{
		auto confRel = static_cast<int>(100 * res->m_Confidences[1].second / res->m_Confidences[0].second);
		m_UI->twDetectionHistory->setItem(a_Row, 12, new QTableWidgetItem(QString::number(confRel)));
	}
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
		if (h->m_Options == options)
		{
			// This set of options has already been calculated, use cached results:
			fillInResults(*h);
			return;
		}
	}

	// Start the detection:
	m_UI->twDetectedResults->setRowCount(0);
	m_ProgressLabel->resize(m_UI->twDetectedResults->viewport()->size());
	m_ProgressLabel->show();
	m_Detector->queueScanSong(m_Song, options);
}





void DlgTempoDetect::songScanned(SongPtr a_Song, TempoDetector::ResultPtr a_Result)
{
	Q_UNUSED(a_Song);

	// Display the results:
	m_UI->twDetectedResults->clearContents();
	m_UI->twDetectedResults->setRowCount(static_cast<int>(a_Result->m_Confidences.size()));
	int row = 0;
	for (const auto & c: a_Result->m_Confidences)
	{
		m_UI->twDetectedResults->setItem(row, 0, new QTableWidgetItem(QString::number(c.first)));
		m_UI->twDetectedResults->setItem(row, 1, new QTableWidgetItem(QString::number(c.second)));
		row += 1;
	}
	m_UI->twDetectedResults->resizeRowsToContents();

	// Add to history:
	m_History.push_back(a_Result);
	m_UI->twDetectionHistory->setRowCount(static_cast<int>(m_History.size()));
	updateHistoryRow(static_cast<int>(m_History.size()) - 1);

	m_ProgressLabel->hide();
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
	m_Detector->queueScanSong(m_Song, options);
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
	m_Detector->queueScanSong(m_Song, options);
}






#include "DlgTempoDetect.hpp"
#include <cassert>
#include <QTimer>
#include <QFileDialog>
#include "ui_DlgTempoDetect.h"
#include "../../DB/Database.hpp"
#include "../../Utils.hpp"
#include "../../Settings.hpp"
#include "../../ComponentCollection.hpp"





static QString formatMPM(const DatedOptional<double> & aSongMPM)
{
	if (!aSongMPM.isPresent())
	{
		return QString();
	}
	return QLocale().toString(aSongMPM.value(), 'f', 1);
}





static QString levelAlgorithmToStr(TempoDetector::ELevelAlgorithm aAlg)
{
	switch (aAlg)
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

DlgTempoDetect::DlgTempoDetect(ComponentCollection & aComponents, SongPtr aSong, QWidget * aParent):
	Super(aParent),
	mUI(new Ui::DlgTempoDetect),
	mComponents(aComponents),
	mSong(aSong),
	mTempoDetectDelay(0),
	mDetector(new SongTempoDetector)
{
	// Init the UI:
	mUI->setupUi(this);
	initOptionsUi();
	mUI->twDetectionHistory->resizeColumnsToContents();  // Default column widths
	Settings::loadWindowPos("DlgTempoDetect", *this);
	Settings::loadHeaderView("DlgTempoDetect", "twDetectionHistory", *mUI->twDetectionHistory->horizontalHeader());

	// Connect the signals:
	connect(mUI->btnClose,                    &QPushButton::pressed,           this, &QDialog::reject);
	connect(mUI->leSampleRate,                &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(mUI->leLocalMaxDistance,          &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(mUI->leMaxTempo,                  &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(mUI->leMinTempo,                  &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(mUI->leNormalizeLevelsWindowSize, &QLineEdit::textEdited,          this, &DlgTempoDetect::delayedDetectTempo);
	connect(mUI->lwLevelAlgorithm,            &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(mUI->lwWindowSize,                &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(mUI->lwStride,                    &QListWidget::currentRowChanged, this, &DlgTempoDetect::detectTempo);
	connect(mUI->btnSaveDebugBeats,           &QPushButton::pressed,           this, &DlgTempoDetect::saveDebugBeats);
	connect(mUI->btnSaveDebugLevels,          &QPushButton::pressed,           this, &DlgTempoDetect::saveDebugLevels);
	connect(mDetector.get(),                  &SongTempoDetector::songScanned, this, &DlgTempoDetect::songScanned);

	// Start the timer for delayed tempo detection:
	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, [this]()
		{
			if (mTempoDetectDelay > 0)
			{
				mTempoDetectDelay -= 1;
				if (mTempoDetectDelay == 1)
				{
					detectTempo();
				}
			}
		}
	);
	timer->start(50);

	// Fill in the song details:
	auto info = mSong->tagId3().mAuthor.valueOrDefault();
	if (!mSong->tagId3().mTitle.isEmpty())
	{
		if (!info.isEmpty())
		{
			info += " - ";
		}
		info += mSong->tagId3().mTitle.value();
	}
	if (info.isEmpty())
	{
		mUI->lblSongInfo->setText(mSong->fileName());
	}
	else
	{
		mUI->lblSongInfo->setText(info + " (" + mSong->fileName() + ")");
	}
	mUI->leMpmManual->setText(formatMPM(mSong->tagManual().mMeasuresPerMinute));
	mUI->leMpmId3->setText(formatMPM(mSong->tagId3().mMeasuresPerMinute));
	mUI->leMpmFilename->setText(formatMPM(mSong->tagFileName().mMeasuresPerMinute));

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
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laSumDist),               mUI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laSumDist);
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laMinMax),                mUI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laMinMax);
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laDiscreetSineTransform), mUI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laDiscreetSineTransform);
	(new QListWidgetItem(levelAlgorithmToStr(TempoDetector::laSumDistMinMax),         mUI->lwLevelAlgorithm))->setData(Qt::UserRole, TempoDetector::laSumDistMinMax);

	// Window size:
	(new QListWidgetItem(   "9", mUI->lwWindowSize))->setData(Qt::UserRole,    9);
	(new QListWidgetItem(  "10", mUI->lwWindowSize))->setData(Qt::UserRole,   10);
	(new QListWidgetItem(  "11", mUI->lwWindowSize))->setData(Qt::UserRole,   11);
	(new QListWidgetItem(  "12", mUI->lwWindowSize))->setData(Qt::UserRole,   12);
	(new QListWidgetItem(  "13", mUI->lwWindowSize))->setData(Qt::UserRole,   13);
	(new QListWidgetItem(  "14", mUI->lwWindowSize))->setData(Qt::UserRole,   14);
	(new QListWidgetItem(  "15", mUI->lwWindowSize))->setData(Qt::UserRole,   15);
	(new QListWidgetItem(  "16", mUI->lwWindowSize))->setData(Qt::UserRole,   16);
	(new QListWidgetItem(  "17", mUI->lwWindowSize))->setData(Qt::UserRole,   17);
	(new QListWidgetItem(  "18", mUI->lwWindowSize))->setData(Qt::UserRole,   18);
	(new QListWidgetItem(  "32", mUI->lwWindowSize))->setData(Qt::UserRole,   32);
	(new QListWidgetItem(  "64", mUI->lwWindowSize))->setData(Qt::UserRole,   64);
	(new QListWidgetItem( "128", mUI->lwWindowSize))->setData(Qt::UserRole,  128);
	(new QListWidgetItem( "256", mUI->lwWindowSize))->setData(Qt::UserRole,  256);
	(new QListWidgetItem( "512", mUI->lwWindowSize))->setData(Qt::UserRole,  512);
	(new QListWidgetItem("1024", mUI->lwWindowSize))->setData(Qt::UserRole, 1024);
	(new QListWidgetItem("2048", mUI->lwWindowSize))->setData(Qt::UserRole, 2048);
	(new QListWidgetItem("4096", mUI->lwWindowSize))->setData(Qt::UserRole, 4096);
	(new QListWidgetItem("8192", mUI->lwWindowSize))->setData(Qt::UserRole, 8192);

	// Stride:
	(new QListWidgetItem( "4", mUI->lwStride))->setData(Qt::UserRole,  4);
	(new QListWidgetItem( "5", mUI->lwStride))->setData(Qt::UserRole,  5);
	(new QListWidgetItem( "6", mUI->lwStride))->setData(Qt::UserRole,  6);
	(new QListWidgetItem( "7", mUI->lwStride))->setData(Qt::UserRole,  7);
	(new QListWidgetItem( "8", mUI->lwStride))->setData(Qt::UserRole,  8);
	(new QListWidgetItem( "9", mUI->lwStride))->setData(Qt::UserRole,  9);
	(new QListWidgetItem("10", mUI->lwStride))->setData(Qt::UserRole, 10);
	(new QListWidgetItem("11", mUI->lwStride))->setData(Qt::UserRole, 11);
	(new QListWidgetItem("12", mUI->lwStride))->setData(Qt::UserRole, 12);
	(new QListWidgetItem("13", mUI->lwStride))->setData(Qt::UserRole, 13);
	(new QListWidgetItem("14", mUI->lwStride))->setData(Qt::UserRole, 14);
	(new QListWidgetItem("15", mUI->lwStride))->setData(Qt::UserRole, 15);
	(new QListWidgetItem("1024", mUI->lwStride))->setData(Qt::UserRole, 1024);
	(new QListWidgetItem("2048", mUI->lwStride))->setData(Qt::UserRole, 2048);
	(new QListWidgetItem("4096", mUI->lwStride))->setData(Qt::UserRole, 4096);
	(new QListWidgetItem("8192", mUI->lwStride))->setData(Qt::UserRole, 8192);

	// Select the defaults:
	SongTempoDetector::Options opt;
	auto genre = mSong->primaryGenre().valueOrDefault();
	std::tie(opt.mMinTempo, opt.mMaxTempo) = Song::detectionTempoRangeForGenre(genre);
	selectOptions(opt);
}





void DlgTempoDetect::selectOptions(const SongTempoDetector::Options & aOptions)
{
	mIsInternalChange = true;
	Utils::selectItemWithData(mUI->lwLevelAlgorithm, aOptions.mLevelAlgorithm);
	Utils::selectItemWithData(mUI->lwWindowSize,     static_cast<qulonglong>(aOptions.mWindowSize));
	Utils::selectItemWithData(mUI->lwStride,         static_cast<qulonglong>(aOptions.mStride));
	mUI->leSampleRate->setText(QString::number(aOptions.mSampleRate));
	mUI->leLocalMaxDistance->setText(QString::number(aOptions.mLocalMaxDistance));
	mUI->leMinTempo->setText(QString::number(aOptions.mMinTempo));
	mUI->leMaxTempo->setText(QString::number(aOptions.mMaxTempo));
	mUI->leNormalizeLevelsWindowSize->setText(QString::number(aOptions.mNormalizeLevelsWindowSize));
	mIsInternalChange = false;
}





SongTempoDetector::Options DlgTempoDetect::readOptionsFromUi()
{
	SongTempoDetector::Options options;
	options.mLevelAlgorithm = static_cast<TempoDetector::ELevelAlgorithm>(mUI->lwLevelAlgorithm->currentItem()->data(Qt::UserRole).toInt());
	options.mWindowSize = static_cast<size_t>(mUI->lwWindowSize->currentItem()->data(Qt::UserRole).toLongLong());
	options.mStride = static_cast<size_t>(mUI->lwStride->currentItem()->data(Qt::UserRole).toLongLong());
	options.mSampleRate = mUI->leSampleRate->text().toInt();
	options.mLocalMaxDistance = static_cast<size_t>(mUI->leLocalMaxDistance->text().toLongLong());
	options.mMinTempo = mUI->leMinTempo->text().toInt();
	options.mMaxTempo = mUI->leMaxTempo->text().toInt();
	options.mNormalizeLevelsWindowSize = static_cast<size_t>(mUI->leNormalizeLevelsWindowSize->text().toLongLong());
	options.mShouldNormalizeLevels = (options.mNormalizeLevelsWindowSize > 0);
	return options;
}





void DlgTempoDetect::updateHistoryRow(int aRow)
{
	const auto & res = mHistory[static_cast<size_t>(aRow)];
	const auto & opt = res->mOptions;
	mUI->twDetectionHistory->setItem(aRow, 0,  new QTableWidgetItem(levelAlgorithmToStr(opt.mLevelAlgorithm)));
	mUI->twDetectionHistory->setItem(aRow, 1,  new QTableWidgetItem(QString::number(opt.mWindowSize)));
	mUI->twDetectionHistory->setItem(aRow, 2,  new QTableWidgetItem(QString::number(opt.mStride)));
	mUI->twDetectionHistory->setItem(aRow, 3,  new QTableWidgetItem(QString::number(opt.mSampleRate)));
	mUI->twDetectionHistory->setItem(aRow, 4,  new QTableWidgetItem(QString::number(opt.mLocalMaxDistance)));
	mUI->twDetectionHistory->setItem(aRow, 5,  new QTableWidgetItem(QString::number(opt.mMinTempo)));
	mUI->twDetectionHistory->setItem(aRow, 6,  new QTableWidgetItem(QString::number(opt.mMaxTempo)));
	mUI->twDetectionHistory->setItem(aRow, 7,  new QTableWidgetItem(QString::number(opt.mNormalizeLevelsWindowSize)));
	mUI->twDetectionHistory->setItem(aRow, 8,  new QTableWidgetItem(QString::number(res->mTempo)));
	double mpm;
	if (mSong->primaryGenre().isPresent())
	{
		mpm = Song::adjustMpm(res->mTempo, mSong->primaryGenre().valueOrDefault());
	}
	else
	{
		mpm = res->mTempo;
	}
	mUI->twDetectionHistory->setItem(aRow, 9,  new QTableWidgetItem(QString::number(mpm, 'f', 1)));
	mUI->twDetectionHistory->setItem(aRow, 10, new QTableWidgetItem(QString::number(res->mConfidence)));
	auto btn = new QPushButton(tr("Use"));
	mUI->twDetectionHistory->setCellWidget(aRow, 11, btn);
	connect(btn, &QPushButton::pressed, [this, mpm]()
		{
			mSong->setManualMeasuresPerMinute(mpm);
			mUI->leMpmManual->setText(formatMPM(mSong->tagManual().mMeasuresPerMinute));
			mComponents.get<Database>()->saveSong(mSong);
		}
	);
	mUI->twDetectionHistory->resizeRowToContents(aRow);
}





void DlgTempoDetect::delayedDetectTempo()
{
	mTempoDetectDelay = 10;  // 0.5 sec
}





void DlgTempoDetect::detectTempo()
{
	auto options = readOptionsFromUi();
	for (const auto & h: mHistory)
	{
		if (h->mOptions == options)
		{
			// This set of options has already been calculated, bail out:
			return;
		}
	}

	// Start the detection:
	mDetector->queueScanSong(mSong, {options});
}





void DlgTempoDetect::songScanned(SongPtr aSong, TempoDetector::ResultPtr aResult)
{
	Q_UNUSED(aSong);

	// Add to history:
	mHistory.push_back(aResult);
	mUI->twDetectionHistory->setRowCount(static_cast<int>(mHistory.size()));
	updateHistoryRow(static_cast<int>(mHistory.size()) - 1);
	mUI->twDetectionHistory->resizeColumnsToContents();
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
	options.mDebugAudioBeatsFileName = fileName;
	mDetector->queueScanSong(mSong, {options});
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
	options.mDebugAudioLevelsFileName = fileName;
	mDetector->queueScanSong(mSong, {options});
}






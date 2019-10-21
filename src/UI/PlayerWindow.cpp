#include "PlayerWindow.hpp"
#include <cassert>
#include <QDebug>
#include <QShortcut>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
#include <QInputDialog>
#include <QFileDialog>
#include "ui_PlayerWindow.h"
#include "../Audio/PlaybackBuffer.hpp"
#include "../Audio/Player.hpp"
#include "../DB/Database.hpp"
#include "../DB/DatabaseImport.hpp"
#include "../PlaylistItemSong.hpp"
#include "../MetadataScanner.hpp"
#include "../LengthHashCalculator.hpp"
#include "../Settings.hpp"
#include "../Utils.hpp"
#include "../LocalVoteServer.hpp"
#include "../PlaylistImportExport.hpp"
#include "Dlg/DlgSongs.hpp"
#include "Dlg/DlgTemplatesList.hpp"
#include "Dlg/DlgHistory.hpp"
#include "Dlg/DlgPickTemplate.hpp"
#include "Dlg/DlgBackgroundTaskList.hpp"
#include "Dlg/DlgSongProperties.hpp"
#include "Dlg/DlgRemovedSongs.hpp"
#include "Dlg/DlgImportDB.hpp"
#include "Dlg/DlgLvsStatus.hpp"
#include "Dlg/DlgLibraryMaintenance.hpp"
#include "Dlg/DlgDebugLog.hpp"





PlayerWindow::PlayerWindow(ComponentCollection & aComponents):
	Super(nullptr),
	mUI(new Ui::PlayerWindow),
	mComponents(aComponents),
	mPlaylistDelegate(new PlaylistItemDelegate),
	mIsLibraryRescanShown(true),
	mLastLibraryRescanTotal(0),
	mLastLibraryRescanQueue(-1),
	mClassroomWindow(nullptr)
{
	mPlaylistModel.reset(new PlaylistItemModel(mComponents.get<Player>()->playlist()));

	mUI->setupUi(this);
	mUI->splitter->setCollapsible(1, false);
	Settings::loadSplitterSizes("PlayerWindow", "splitter", *mUI->splitter);
	mUI->tblPlaylist->setModel(mPlaylistModel.get());
	mUI->tblPlaylist->setItemDelegate(mPlaylistDelegate.get());
	mUI->tblPlaylist->setDropIndicatorShown(true);
	mUI->waveform->setPlayer(*mComponents.get<Player>());
	setProperty(DJControllers::CONTEXT_PROPERTY_NAME, "PlayerWindow");

	// Set labels' minimum width to avoid layout changes in runtime:
	mUI->lblTotalTime->setMinimumWidth(mUI->lblTotalTime->fontMetrics().width("00:00"));
	mUI->lblPosition->setMinimumWidth(mUI->lblPosition->fontMetrics().width("00:00"));
	mUI->lblRemaining->setMinimumWidth(mUI->lblRemaining->fontMetrics().width("-00:00"));

	// Set button widths:
	auto w = mUI->lblTotalTime->fontMetrics().width("000");
	QSize btnSize(w, w);
	mUI->btnPrev->setFixedSize(btnSize);
	mUI->btnPlayPause->setFixedSize(btnSize);
	mUI->btnStop->setFixedSize(btnSize);
	mUI->btnNext->setFixedSize(btnSize);

	// Decorate the splitter handle with 3 sunken lines:
	auto lay = new QVBoxLayout(mUI->splitter->handle(1));
	lay->setSpacing(0);
	lay->setMargin(0);
	for (int i = 0; i < 3; ++i)
	{
		auto frame = new QFrame(mUI->splitter->handle(1));
		frame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
		frame->setLineWidth(1);
		lay->addWidget(frame);
	}

	// Connect the signals:
	auto db     = mComponents.get<Database>();
	auto player = mComponents.get<Player>();
	auto djc    = mComponents.get<DJControllers>();
	connect(mUI->btnSongs,               &QPushButton::clicked,                 this,         &PlayerWindow::showSongs);
	connect(mUI->btnTemplates,           &QPushButton::clicked,                 this,         &PlayerWindow::showTemplates);
	connect(mUI->btnHistory,             &QPushButton::clicked,                 this,         &PlayerWindow::showHistory);
	connect(mUI->btnAddFromTemplate,     &QPushButton::clicked,                 this,         &PlayerWindow::addFromTemplate);
	connect(mUI->btnClassroomMode,       &QPushButton::clicked,                 this,         &PlayerWindow::switchToClassroomMode);
	connect(mUI->btnPrev,                &QPushButton::clicked,                 player.get(), &Player::prevTrack);
	connect(mUI->btnPlayPause,           &QPushButton::clicked,                 player.get(), &Player::startPausePlayback);
	connect(mUI->btnStop,                &QPushButton::clicked,                 player.get(), &Player::stopPlayback);
	connect(mUI->btnNext,                &QPushButton::clicked,                 player.get(), &Player::nextTrack);
	connect(mUI->tblPlaylist,            &QTableView::doubleClicked,            this,         &PlayerWindow::trackDoubleClicked);
	connect(mUI->vsVolume,               &QSlider::sliderMoved,                 this,         &PlayerWindow::volumeSliderMoved);
	connect(mUI->vsTempo,                &QSlider::valueChanged,                this,         &PlayerWindow::tempoValueChanged);
	connect(mUI->btnTempoReset,          &QToolButton::clicked,                 this,         &PlayerWindow::resetTempo);
	connect(mUI->actBackgroundTasks,     &QAction::triggered,                   this,         &PlayerWindow::showBackgroundTasks);
	connect(mUI->actSongProperties,      &QAction::triggered,                   this,         &PlayerWindow::showSongProperties);
	connect(&mUpdateTimer,               &QTimer::timeout,                      this,         &PlayerWindow::periodicUIUpdate);
	connect(mUI->actDeleteFromDisk,      &QAction::triggered,                   this,         &PlayerWindow::deleteSongsFromDisk);
	connect(mUI->actRemoveFromLibrary,   &QAction::triggered,                   this,         &PlayerWindow::removeSongsFromLibrary);
	connect(mUI->actRemoveFromPlaylist,  &QAction::triggered,                   this,         &PlayerWindow::deleteSelectedPlaylistItems);
	connect(mUI->actPlay,                &QAction::triggered,                   this,         &PlayerWindow::jumpToAndPlay);
	connect(mUI->actSetDurationLimit,    &QAction::triggered,                   this,         &PlayerWindow::setDurationLimit);
	connect(mUI->actRemoveDurationLimit, &QAction::triggered,                   this,         &PlayerWindow::removeDurationLimit);
	connect(mUI->actRemovedSongs,        &QAction::triggered,                   this,         &PlayerWindow::showRemovedSongs);
	connect(mUI->actImportDB,            &QAction::triggered,                   this,         &PlayerWindow::importDB);
	connect(mUI->actLibraryMaintenance,  &QAction::triggered,                   this,         &PlayerWindow::showLibraryMaintenance);
	connect(mUI->actSavePlaylist,        &QAction::triggered,                   this,         &PlayerWindow::savePlaylist);
	connect(mUI->actLoadPlaylist,        &QAction::triggered,                   this,         &PlayerWindow::loadPlaylist);
	connect(mUI->actToggleLvs,           &QAction::triggered,                   this,         &PlayerWindow::toggleLvs);
	connect(mUI->actLvsStatus,           &QAction::triggered,                   this,         &PlayerWindow::showLvsStatus);
	connect(mUI->actShowDebugLog,        &QAction::triggered,                   this,         &PlayerWindow::showDebugLog);
	connect(mUI->lwQuickPlay,            &QListWidget::itemClicked,             this,         &PlayerWindow::quickPlayItemClicked);
	connect(mPlaylistDelegate.get(),     &PlaylistItemDelegate::replaceSong,    this,         &PlayerWindow::replaceSong);
	connect(mUI->tblPlaylist,            &QWidget::customContextMenuRequested,  this,         &PlayerWindow::showPlaylistContextMenu);
	connect(mUI->waveform,               &WaveformDisplay::songChanged,         db.get(),     &Database::saveSong);
	connect(player.get(),                 &Player::startedPlayback,              this,         &PlayerWindow::playerStartedPlayback);
	connect(mUI->chbKeepTempo,           &QCheckBox::toggled,                   player.get(), &Player::setKeepTempo);
	connect(mUI->chbKeepVolume,          &QCheckBox::toggled,                   player.get(), &Player::setKeepVolume);
	connect(player.get(),                 &Player::tempoCoeffChanged,            this,         &PlayerWindow::tempoCoeffChanged);
	connect(player.get(),                 &Player::volumeChanged,                this,         &PlayerWindow::playerVolumeChanged);
	connect(player.get(),                 &Player::invalidTrack,                 this,         &PlayerWindow::invalidTrack);
	connect(djc.get(),                    &DJControllers::controllerConnected,   this,         &PlayerWindow::djControllerConnected);
	connect(djc.get(),                    &DJControllers::controllerRemoved,     this,         &PlayerWindow::djControllerRemoved);

	// Set up the header sections (defaults, then load from previous session):
	QFontMetrics fm(mUI->tblPlaylist->horizontalHeader()->font());
	mUI->tblPlaylist->setColumnWidth(PlaylistItemModel::colLength,            fm.width("W000:00:00W"));
	mUI->tblPlaylist->setColumnWidth(PlaylistItemModel::colGenre,             fm.width("WGenreW"));
	mUI->tblPlaylist->setColumnWidth(PlaylistItemModel::colMeasuresPerMinute, fm.width("WMPMW"));
	auto defaultWid = mUI->tblPlaylist->columnWidth(PlaylistItemModel::colDisplayName);
	mUI->tblPlaylist->setColumnWidth(PlaylistItemModel::colAuthor,      defaultWid * 2);
	mUI->tblPlaylist->setColumnWidth(PlaylistItemModel::colTitle,       defaultWid * 2);
	mUI->tblPlaylist->setColumnWidth(PlaylistItemModel::colDisplayName, defaultWid * 3);
	Settings::loadHeaderView("PlayerWindow", "tblPlaylist", *mUI->tblPlaylist->horizontalHeader());

	// Load checkboxes from the last session:
	mUI->chbImmediatePlayback->setChecked(Settings::loadValue("PlayerWindow", "chbImmediatePlayback.isChecked", true).toBool());
	mUI->chbAppendUponCompletion->setChecked(Settings::loadValue("PlayerWindow", "chbAppendUponCompletion.isChecked", true).toBool());
	auto keepTempo = Settings::loadValue("PlayerWindow", "chbKeepTempo.isChecked", false).toBool();
	auto keepVolume = Settings::loadValue("PlayerWindow", "chbKeepVolume.isChecked", false).toBool();
	mUI->chbKeepTempo->setChecked(keepTempo);
	mUI->chbKeepVolume->setChecked(keepVolume);
	player->setKeepTempo(keepTempo);
	player->setKeepVolume(keepVolume);

	// Set the TempoReset button's size to avoid layout changes while dragging the tempo slider:
	fm = mUI->btnTempoReset->fontMetrics();
	mUI->btnTempoReset->setMinimumWidth(
		mUI->btnTempoReset->sizeHint().width() - fm.width(mUI->btnTempoReset->text()) + fm.width("+99.9 %")
	);

	// Set up the Tools button:
	auto lvs = mComponents.get<LocalVoteServer>();
	mUI->actToggleLvs->setChecked(lvs->isStarted());
	mUI->actLvsStatus->setEnabled(lvs->isStarted());
	auto menu = new QMenu(this);
	menu->addAction(mUI->actBackgroundTasks);
	menu->addAction(mUI->actRemovedSongs);
	menu->addSeparator();
	menu->addAction(mUI->actImportDB);
	menu->addSeparator();
	menu->addAction(mUI->actLibraryMaintenance);
	menu->addSeparator();
	menu->addAction(mUI->actSavePlaylist);
	menu->addAction(mUI->actLoadPlaylist);
	menu->addSeparator();
	menu->addAction(mUI->actToggleLvs);
	menu->addAction(mUI->actLvsStatus);
	menu->addSeparator();
	menu->addAction(mUI->actShowDebugLog);
	mUI->btnTools->setMenu(menu);

	// Add the context-menu actions to their respective controls, so that their shortcuts work:
	mUI->tblPlaylist->addActions({
		mUI->actDeleteFromDisk,
		mUI->actPlay,
		mUI->actRate,
		mUI->actRemoveFromLibrary,
		mUI->actRemoveFromPlaylist,
		mUI->actSongProperties,
	});
	mUI->btnTools->addActions({
		mUI->actBackgroundTasks,
		mUI->actRemovedSongs,
		mUI->actImportDB,
		mUI->actLibraryMaintenance,
		mUI->actSavePlaylist,
		mUI->actLoadPlaylist,
		mUI->actToggleLvs,
		mUI->actLvsStatus,
		mUI->actShowDebugLog,
	});

	refreshQuickPlay();
	refreshAppendUponCompletion();
	auto lastCompletionTemplateName = Settings::loadValue("PlayerWindow", "cbCompletionAppendTemplate.templateName", "").toString();
	auto tmplIndex = mUI->cbCompletionAppendTemplate->findText(lastCompletionTemplateName);
	mUI->cbCompletionAppendTemplate->setCurrentIndex(tmplIndex);

	setUpDjControllers();

	mUpdateTimer.start(200);
}





PlayerWindow::~PlayerWindow()
{
	Settings::saveValue("PlayerWindow", "cbCompletionAppendTemplate.templateName", mUI->cbCompletionAppendTemplate->currentText());
	Settings::saveValue("PlayerWindow", "chbKeepVolume.isChecked", mUI->chbKeepVolume->isChecked());
	Settings::saveValue("PlayerWindow", "chbKeepTempo.isChecked", mUI->chbKeepTempo->isChecked());
	Settings::saveValue("PlayerWindow", "chbImmediatePlayback.isChecked", mUI->chbImmediatePlayback->isChecked());
	Settings::saveValue("PlayerWindow", "chbAppendUponCompletion.isChecked", mUI->chbAppendUponCompletion->isChecked());
	Settings::saveHeaderView("PlayerWindow", "tblPlaylist", *mUI->tblPlaylist->horizontalHeader());
	Settings::saveSplitterSizes("PlayerWindow", "splitter", *mUI->splitter);
}





void PlayerWindow::setClassroomWindow(QWidget & aClassroomWindow)
{
	mClassroomWindow = &aClassroomWindow;
}





void PlayerWindow::rateSelectedSongs(double aLocalRating)
{
	for (auto & song: selectedPlaylistSongs())
	{
		song->setLocalRating(aLocalRating);
		mComponents.get<Database>()->saveSong(song);
	}
}





std::vector<SongPtr> PlayerWindow::selectedPlaylistSongs() const
{
	std::vector<SongPtr> res;
	res.reserve(static_cast<size_t>(mUI->tblPlaylist->selectionModel()->selectedRows().count()));
	auto player = mComponents.get<Player>();
	for (const auto & idx: mUI->tblPlaylist->selectionModel()->selectedRows())
	{
		auto pli = player->playlist().items()[static_cast<size_t>(idx.row())];
		if (pli == nullptr)
		{
			qWarning() << "Got a nullptr playlist item";
			continue;
		}
		auto plis = std::dynamic_pointer_cast<PlaylistItemSong>(pli);
		if (plis == nullptr)
		{
			continue;
		}
		res.push_back(plis->song());
	}
	return res;
}





void PlayerWindow::refreshQuickPlay()
{
	mUI->lwQuickPlay->clear();

	// Insert the templates:
	auto db = mComponents.get<Database>();
	auto templates = db->templates();
	for (const auto & tmpl: templates)
	{
		auto item = new QListWidgetItem(tmpl->displayName(), mUI->lwQuickPlay);
		item->setData(Qt::UserRole, QVariant::fromValue(tmpl));
		item->setBackgroundColor(tmpl->bgColor());
	}

	// Insert the favorite filters:
	auto favorites = db->getFavoriteFilters();
	for (const auto & fav: favorites)
	{
		auto item = new QListWidgetItem(fav->displayName(), mUI->lwQuickPlay);
		item->setData(Qt::UserRole, QVariant::fromValue(fav));
		item->setBackgroundColor(fav->bgColor());
		auto numMatches = db->numSongsMatchingFilter(*fav);
		item->setToolTip(tr("Songs: %1").arg(numMatches));
		if (numMatches == 0)
		{
			auto fBrush = item->foreground();
			fBrush.setColor(QColor(191, 0, 0));
			item->setForeground(fBrush);
			auto font = item->font();
			font.setItalic(true);
			item->setFont(font);
		}
	}
}





void PlayerWindow::refreshAppendUponCompletion()
{
	// Store the current selection:
	auto selObj = objectToAppendUponCompletion();
	auto selTemplate = selObj.value<TemplatePtr>();
	auto selFilter = selObj.value<FilterPtr>();

	// Insert templates:
	auto cb = mUI->cbCompletionAppendTemplate;
	cb->clear();
	auto tmpls = mComponents.get<Database>()->templates();
	auto size = cb->fontMetrics().ascent();
	for (const auto & tmpl: tmpls)
	{
		QPixmap pixmap(size, size);
		pixmap.fill(tmpl->bgColor());
		cb->addItem(QIcon(pixmap), tmpl->displayName(), QVariant::fromValue(tmpl));
		if (tmpl == selTemplate)
		{
			cb->setCurrentIndex(mUI->cbCompletionAppendTemplate->count() - 1);
		}
	}

	// Insert favorite filters:
	auto filters = mComponents.get<Database>()->getFavoriteFilters();
	for (const auto & filter: filters)
	{
		QPixmap pixmap(size, size);
		pixmap.fill(filter->bgColor());
		cb->addItem(QIcon(pixmap), filter->displayName(), QVariant::fromValue(filter));
		if (filter == selFilter)
		{
			cb->setCurrentIndex(mUI->cbCompletionAppendTemplate->count() - 1);
		}
	}
}





void PlayerWindow::setSelectedItemsDurationLimit(double aNewDurationLimit)
{
	auto player = mComponents.get<Player>();
	const auto & items = player->playlist().items();
	for (const auto & row: mUI->tblPlaylist->selectionModel()->selectedRows())
	{
		items[static_cast<size_t>(row.row())]->setDurationLimit(aNewDurationLimit);
	}
	// Update the items, starting with the current one:
	// (No changes to historic data)
	player->updateTrackTimesFromCurrent();
}





QVariant PlayerWindow::objectToAppendUponCompletion() const
{
	return mUI->cbCompletionAppendTemplate->currentData();
}





void PlayerWindow::setUpDjControllers()
{
	const QString CONTEXT = "PlayerWindow";
	setProperty(DJControllers::CONTEXT_PROPERTY_NAME, CONTEXT);
	auto djc = mComponents.get<DJControllers>();
	mDjKeyHandler    = djc->registerContextKeyHandler(   CONTEXT, this, "handleDjControllerKey");
	mDjSliderHandler = djc->registerContextSliderHandler(CONTEXT, this, "handleDjControllerSlider");
	mDjWheelHandler  = djc->registerContextWheelHandler( CONTEXT, this, "handleDjControllerWheel");
}





void PlayerWindow::handleDjControllerKey(int aKey)
{
	switch (aKey)
	{
		case AbstractController::skPlayPause1:
		case AbstractController::skPlayPause2:
		{
			mComponents.get<Player>()->startPausePlayback();
			return;
		}
	}
}





void PlayerWindow::handleDjControllerSlider(int aSlider, qreal aValue)
{
	switch (aSlider)
	{
		case AbstractController::ssPitch1:
		case AbstractController::ssPitch2:
		{
			mUI->vsTempo->setValue(static_cast<int>(100 * aValue) - 50);
			return;
		}

		case AbstractController::ssVolume1:
		case AbstractController::ssVolume2:
		case AbstractController::ssMasterVolume:
		{
			auto volume = static_cast<int>(100 * aValue);
			mUI->vsVolume->setValue(volume);
			volumeSliderMoved(volume);
			return;
		}
	}
}





void PlayerWindow::handleDjControllerWheel(int aWheel, int aNumSteps)
{
	switch (aWheel)
	{
		case AbstractController::swBrowse:
		{
			if (mUI->tblPlaylist->currentIndex().isValid())
			{
				auto row = mUI->tblPlaylist->currentIndex().row() - aNumSteps;
				auto numRows = static_cast<int>(mComponents.get<Player>()->playlist().items().size());
				mUI->tblPlaylist->selectRow(Utils::clamp(row, 0, numRows - 1));
			}
			else
			{
				mUI->tblPlaylist->selectRow(0);
			}
			return;
		}
	}
}





void PlayerWindow::showSongs()
{
	DlgSongs dlg(mComponents, nullptr, true, this);
	connect(&dlg, &DlgSongs::addSongToPlaylist,    this, &PlayerWindow::addSongToPlaylist);
	connect(&dlg, &DlgSongs::insertSongToPlaylist, this, &PlayerWindow::insertSongToPlaylist);
	dlg.showMaximized();
	dlg.exec();
}





void PlayerWindow::showTemplates()
{
	DlgTemplatesList dlg(mComponents, this);
	dlg.exec();

	refreshQuickPlay();
	refreshAppendUponCompletion();
}





void PlayerWindow::showHistory()
{
	DlgHistory dlg(mComponents, this);
	connect(&dlg, &DlgHistory::appendSongToPlaylist, this, &PlayerWindow::addSongToPlaylist);
	connect(&dlg, &DlgHistory::insertSongToPlaylist, this, &PlayerWindow::insertSongToPlaylist);
	dlg.exec();
}





void PlayerWindow::addSongToPlaylist(SongPtr aSong)
{
	addPlaylistItem(std::make_shared<PlaylistItemSong>(aSong, nullptr));
}





void PlayerWindow::insertSongToPlaylist(SongPtr aSong)
{
	const auto & sel = mUI->tblPlaylist->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		// No selection -> append to end
		return addSongToPlaylist(aSong);
	}
	auto row = sel[0].row() + 1;
	mComponents.get<Player>()->playlist().insertItem(row, std::make_shared<PlaylistItemSong>(aSong, nullptr));
	mUI->tblPlaylist->selectionModel()->clearSelection();
	mUI->tblPlaylist->selectRow(row);
}





void PlayerWindow::addPlaylistItem(std::shared_ptr<IPlaylistItem> aItem)
{
	mComponents.get<Player>()->playlist().addItem(aItem);
}





void PlayerWindow::deleteSelectedPlaylistItems()
{
	const auto & sel = mUI->tblPlaylist->selectionModel()->selectedRows();
	std::vector<int> rows;
	for (const auto & row: sel)
	{
		rows.push_back(row.row());
	}
	std::sort(rows.begin(), rows.end());
	int numErased = 0;
	auto player = mComponents.get<Player>();
	for (auto row: rows)
	{
		player->playlist().deleteItem(row - numErased);
		numErased += 1;  // Each erased row shifts indices upwards by one
	}
	player->updateTrackTimesFromCurrent();
}




void PlayerWindow::trackDoubleClicked(const QModelIndex & aTrack)
{
	if (aTrack.isValid())
	{
		mComponents.get<Player>()->jumpTo(aTrack.row());
	}
}





void PlayerWindow::volumeSliderMoved(int aNewValue)
{
	mComponents.get<Player>()->setVolume(static_cast<double>(aNewValue) / 100);
}





void PlayerWindow::addFromTemplate()
{
	DlgPickTemplate dlg(mComponents, this);
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}
	auto tmpl = dlg.selectedTemplate();
	if (tmpl == nullptr)
	{
		return;
	}
	mComponents.get<Player>()->playlist().addFromTemplate(*mComponents.get<Database>(), *tmpl);
}





void PlayerWindow::switchToClassroomMode()
{
	if (mClassroomWindow == nullptr)
	{
		qWarning() << "ClassroomWindow not available!";
		assert(!"ClassroomWindow not available");
		return;
	}
	Settings::saveValue("UI", "LastMode", 1);
	mClassroomWindow->showMaximized();
	this->hide();
}





void PlayerWindow::tempoValueChanged(int aNewValue)
{
	auto percent = static_cast<double>(aNewValue) / 3;
	if (aNewValue >= 0)
	{
		mUI->btnTempoReset->setText(QString("+%1 %").arg(QString::number(percent, 'f', 1)));
	}
	else
	{
		mUI->btnTempoReset->setText(QString("-%1 %").arg(QString::number(-percent, 'f', 1)));
	}
	if (!mIsInternalChange)
	{
		mComponents.get<Player>()->setTempo(static_cast<double>(percent + 100) / 100);
	}
}





void PlayerWindow::resetTempo()
{
	mUI->vsTempo->setValue(0);
}





void PlayerWindow::showBackgroundTasks()
{
	DlgBackgroundTaskList dlg(this);
	dlg.exec();
}





void PlayerWindow::periodicUIUpdate()
{
	// Update the player UI:
	auto player = mComponents.get<Player>();
	auto position  = static_cast<int>(player->currentPosition() + 0.5);
	auto remaining = static_cast<int>(player->remainingTime() + 0.5);
	auto total     = static_cast<int>(player->totalTime() + 0.5);
	mUI->lblPosition->setText( QString( "%1:%2").arg(position  / 60).arg(QString::number(position  % 60), 2, '0'));
	mUI->lblRemaining->setText(QString("-%1:%2").arg(remaining / 60).arg(QString::number(remaining % 60), 2, '0'));
	mUI->lblTotalTime->setText(QString( "%1:%2").arg(total     / 60).arg(QString::number(total     % 60), 2, '0'));
	mUI->lblWallClockTime->setText(QTime::currentTime().toString());

	// Update the SongScan UI:
	auto queueLength = mComponents.get<LengthHashCalculator>()->queueLength() * 2 + mComponents.get<MetadataScanner>()->queueLength();
	if (mLastLibraryRescanQueue != queueLength)
	{
		mLastLibraryRescanQueue = queueLength;
		if (queueLength == 0)
		{
			if (mIsLibraryRescanShown)
			{
				mUI->wLibraryRescan->hide();
				mIsLibraryRescanShown = false;
			}
		}
		else
		{
			auto numSongs = static_cast<int>(mComponents.get<Database>()->songs().size() * 2);
			if (numSongs != mLastLibraryRescanTotal)
			{
				mUI->pbLibraryRescan->setMaximum(numSongs);
				mLastLibraryRescanTotal = numSongs;
			}
			mUI->pbLibraryRescan->setValue(std::max(numSongs - queueLength, 0));
			mUI->pbLibraryRescan->update();  // For some reason setting the value is not enough to redraw
			if (!mIsLibraryRescanShown)
			{
				mUI->wLibraryRescan->show();
				mIsLibraryRescanShown = true;
			}
		}
	}
}





void PlayerWindow::showPlaylistContextMenu(const QPoint & aPos)
{
	// Build the context menu:
	const auto & sel = mUI->tblPlaylist->selectionModel()->selectedRows();
	QMenu contextMenu;
	contextMenu.addAction(mUI->actPlay);
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actRate);
	connect(contextMenu.addAction(QString("    * * * * *")), &QAction::triggered, [this](){ rateSelectedSongs(5); });
	connect(contextMenu.addAction(QString("    * * * *")),   &QAction::triggered, [this](){ rateSelectedSongs(4); });
	connect(contextMenu.addAction(QString("    * * *")),     &QAction::triggered, [this](){ rateSelectedSongs(3); });
	connect(contextMenu.addAction(QString("    * *")),       &QAction::triggered, [this](){ rateSelectedSongs(2); });
	connect(contextMenu.addAction(QString("    *")),         &QAction::triggered, [this](){ rateSelectedSongs(1); });
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actSetDurationLimit);
	for (const auto dl: {30, 45, 60, 75, 90, 120, 150})
	{
		auto actQuickOffer = contextMenu.addAction(QString("    %1").arg(Utils::formatTime(dl)));
		connect(actQuickOffer, &QAction::triggered, [this, dl]() { setSelectedItemsDurationLimit(dl); });
		actQuickOffer->setEnabled(!sel.isEmpty());
	}
	contextMenu.addAction(mUI->actRemoveDurationLimit);
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actRemoveFromPlaylist);
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actRemoveFromLibrary);
	contextMenu.addAction(mUI->actDeleteFromDisk);
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actSongProperties);

	// Update actions based on selection:
	mUI->actSongProperties->setEnabled(sel.count() == 1);
	mUI->actRemoveFromLibrary->setEnabled(!sel.isEmpty());
	mUI->actRemoveFromPlaylist->setEnabled(!sel.isEmpty());
	mUI->actDeleteFromDisk->setEnabled(!sel.isEmpty());
	mUI->actPlay->setEnabled(sel.count() == 1);
	mUI->actRate->setEnabled(!sel.isEmpty());
	mUI->actSetDurationLimit->setEnabled(!sel.isEmpty());
	mUI->actRemoveDurationLimit->setEnabled(!sel.isEmpty());

	// Display the menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? aPos : widget->mapToGlobal(aPos);
	contextMenu.exec(pos, nullptr);
}





void PlayerWindow::showSongProperties()
{
	assert(mUI->tblPlaylist->selectionModel()->selectedRows().count() == 1);

	auto row = mUI->tblPlaylist->selectionModel()->selectedRows()[0].row();
	auto item = mComponents.get<Player>()->playlist().items()[static_cast<size_t>(row)];
	auto songItem = std::dynamic_pointer_cast<PlaylistItemSong>(item);
	if (songItem == nullptr)
	{
		return;
	}
	auto song = songItem->song();
	DlgSongProperties dlg(mComponents, song, this);
	dlg.exec();
}





void PlayerWindow::deleteSongsFromDisk()
{
	// Collect the songs to remove:
	auto songs = selectedPlaylistSongs();
	if (songs.empty())
	{
		return;
	}

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Delete songs?"),
		tr(
			"Are you sure you want to delete the selected songs from the disk?"
			"The files will be deleted and all properties set in the library will be lost.\n\n"
			"This operation cannot be undone!"
		),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Remove from the DB, delete from disk:
	for (const auto & song: songs)
	{
		mComponents.get<Database>()->removeSong(*song, true);
	}
}





void PlayerWindow::removeSongsFromLibrary()
{
	// Collect the songs to remove:
	auto songs = selectedPlaylistSongs();
	if (songs.empty())
	{
		return;
	}

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove songs?"),
		tr(
			"Are you sure you want to remove the selected songs from the library? The song files will stay "
			"on the disk, but all properties set in the library will be lost.\n\n"
			"This operation cannot be undone!"
		),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Remove from the DB:
	for (const auto & song: songs)
	{
		mComponents.get<Database>()->removeSong(*song, false);
	}
}





void PlayerWindow::jumpToAndPlay()
{
	const auto & sel = mUI->tblPlaylist->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		return;
	}
	auto player = mComponents.get<Player>();
	if (player->playlist().currentItemIndex() == sel[0].row())
	{
		// Already playing this track, bail out:
		return;
	}
	player->jumpTo(sel[0].row());
}





void PlayerWindow::quickPlayItemClicked(QListWidgetItem * aItem)
{
	// Find the position where to insert in the playlist:
	auto player = mComponents.get<Player>();
	auto & playlist = player->playlist();
	auto idx = mUI->tblPlaylist->currentIndex().row();
	if (idx < 0)
	{
		idx = static_cast<int>(playlist.items().size());
	}
	else
	{
		idx += 1;
	}

	auto data = aItem->data(Qt::UserRole);
	auto tmpl = data.value<TemplatePtr>();
	int numAdded = 0;
	if (tmpl != nullptr)
	{
		// Insert songs by a template:
		auto chosen = mComponents.get<Database>()->pickSongsForTemplate(*tmpl);
		if (chosen.empty())
		{
			qDebug() << "Failed to add songs by template " << tmpl->displayName() << " to playlist.";
			return;
		}
		for (auto itr = chosen.crbegin(), end = chosen.crend(); itr != end; ++itr)
		{
			playlist.insertItem(idx, std::make_shared<PlaylistItemSong>(itr->first, itr->second));
		}
		numAdded = static_cast<int>(chosen.size());
	}
	else
	{
		// Insert a song by a filter:
		auto filter = data.value<FilterPtr>();
		if (filter == nullptr)
		{
			qDebug() << "Unknown object clicked in QuickPlayer";
			assert(!"Unknown object clicked");
			return;
		}
		auto chosen = mComponents.get<Database>()->pickSongForFilter(*filter);
		if (chosen == nullptr)
		{
			qDebug() << "Failed to add a song by filter " << filter->displayName() << " to playlist.";
			return;
		}
		playlist.insertItem(idx, std::make_shared<PlaylistItemSong>(chosen, filter));
		numAdded = 1;
	}
	if (mUI->chbImmediatePlayback->checkState() == Qt::Checked)
	{
		player->jumpTo(idx);
		if (!player->isPlaying())
		{
			player->startPlayback();
		}
	}
	mUI->tblPlaylist->selectRow(idx + numAdded - 1);
}





void PlayerWindow::setDurationLimit()
{
	bool isOK;
	QString durationLimitStr = QInputDialog::getText(
		this,
		tr("SkauTan: Set duration limit"),
		tr("New duration:"),
		QLineEdit::Normal,
		QString(),
		&isOK
	);
	if (!isOK)
	{
		return;
	}
	auto durationLimit = Utils::parseTime(durationLimitStr, isOK);
	if (durationLimitStr.isEmpty() || !isOK)
	{
		durationLimit = -1;
	}

	setSelectedItemsDurationLimit(durationLimit);
}





void PlayerWindow::removeDurationLimit()
{
	setSelectedItemsDurationLimit(-1);
}





void PlayerWindow::replaceSong(const QModelIndex & aIndex)
{
	// Is it a replace-able song:
	auto idx = aIndex.row();
	auto player = mComponents.get<Player>();
	if (!player->playlist().isValidIndex(idx))
	{
		return;
	}
	auto pli = player->playlist().items()[static_cast<size_t>(idx)];
	auto pls = std::dynamic_pointer_cast<PlaylistItemSong>(pli);
	if (pls == nullptr)
	{
		return;
	}
	auto filter = pls->filter();
	if (filter == nullptr)
	{
		return;
	}

	// Get a replacement:
	auto song = mComponents.get<Database>()->pickSongForFilter(*filter, pls->song());
	if (song == pls->song())
	{
		// No replacement available
		return;
	}
	player->playlist().replaceItem(idx, std::make_shared<PlaylistItemSong>(song, filter));

	// Special handling for replacing currently-played song:
	if (player->isTrackLoaded() && aIndex.row() == player->playlist().currentItemIndex())
	{
		// Replacing the currently played song, restart the playback of the new item:
		player->jumpTo(aIndex.row());
	}
}





void PlayerWindow::showRemovedSongs()
{
	DlgRemovedSongs dlg(mComponents, this);
	dlg.exec();
}





void PlayerWindow::playerStartedPlayback()
{
	// Check if starting to play the last item:
	if (!mComponents.get<Player>()->playlist().isAtEnd())
	{
		return;
	}

	// Check if the user agreed to appending:
	if (!mUI->chbAppendUponCompletion->isChecked())
	{
		return;
	}

	// Append:
	auto selObj = objectToAppendUponCompletion();
	auto player = mComponents.get<Player>();
	auto db = mComponents.get<Database>();
	if (selObj.value<TemplatePtr>() != nullptr)
	{
		player->playlist().addFromTemplate(*db, *selObj.value<TemplatePtr>());
	}
	else if (selObj.value<FilterPtr>() != nullptr)
	{
		player->playlist().addFromFilter(*db, *selObj.value<FilterPtr>());
	}
}





void PlayerWindow::tempoCoeffChanged(qreal aTempoCoeff)
{
	auto value = static_cast<int>((aTempoCoeff * 100 - 100) * 3);
	mIsInternalChange = true;
	mUI->vsTempo->setValue(value);
	mIsInternalChange = false;
}





void PlayerWindow::playerVolumeChanged(qreal aVolume)
{
	auto value = static_cast<int>(aVolume * 100);
	mIsInternalChange = true;
	mUI->vsVolume->setValue(value);
	mIsInternalChange = false;
}





void PlayerWindow::invalidTrack(IPlaylistItemPtr aItem)
{
	mPlaylistModel->trackWasModified(*aItem);
}





void PlayerWindow::importDB()
{
	DlgImportDB dlg(this);
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}

	Database fromDB(mComponents);
	fromDB.open(dlg.mFileName);
	DatabaseImport import(fromDB, *mComponents.get<Database>(), dlg.mOptions);
}





void PlayerWindow::showLibraryMaintenance()
{
	DlgLibraryMaintenance dlg(mComponents, this);
	dlg.exec();
}





void PlayerWindow::savePlaylist()
{
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Save playlist"),
		QString(),
		tr("M3U playlist (*.m3u)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	try
	{
		PlaylistImportExport::doExport(mComponents.get<Player>()->playlist(), fileName);
	}
	catch (const std::exception & exc)
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot save playlist"),
			tr("Cannot save playlist to file %1: %2").arg(fileName).arg(exc.what())
		);
	}
}





void PlayerWindow::loadPlaylist()
{
	auto fileName = QFileDialog::getOpenFileName(
		this,
		tr("SkauTan: Load playlist"),
		QString(),
		tr("M3U playlist (*.m3u)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	try
	{
		auto & playlist = mComponents.get<Player>()->playlist();
		const auto & selRows = mUI->tblPlaylist->selectionModel()->selectedRows();
		auto pos = selRows.empty() ? static_cast<int>(playlist.items().size()) - 1 : selRows[0].row();
		auto numInserted = PlaylistImportExport::doImport(playlist, *(mComponents.get<Database>()), pos, fileName);
		mUI->tblPlaylist->selectRow(pos + numInserted);
	}
	catch (const std::exception & exc)
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot load playlist"),
			tr("Cannot save playlist to file %1: %2").arg(fileName).arg(exc.what())
		);
	}
}





void PlayerWindow::toggleLvs()
{
	auto lvs = mComponents.get<LocalVoteServer>();
	if (lvs->isStarted())
	{
		lvs->stopServer();
		mUI->actLvsStatus->setEnabled(false);
	}
	else
	{
		lvs->startServer();
		mUI->actLvsStatus->setEnabled(true);
	}
}





void PlayerWindow::djControllerConnected(const QString & aName)
{
	Q_UNUSED(aName);
	auto mc = mComponents.get<DJControllers>();
	// TODO: Nicer LED control - only light up if there's something to play
	mc->setLedPlay(true);
}





void PlayerWindow::djControllerRemoved()
{
	// Nothing needed yet
}





void PlayerWindow::showLvsStatus()
{
	DlgLvsStatus dlg(mComponents, this);
	dlg.exec();
}





void PlayerWindow::showDebugLog()
{
	DlgDebugLog dlg(this);
	dlg.exec();
}

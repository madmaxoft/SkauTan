#include "PlayerWindow.h"
#include "assert.h"
#include <QDebug>
#include <QShortcut>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
#include <QInputDialog>
#include <QFileDialog>
#include "ui_PlayerWindow.h"
#include "Database.h"
#include "DlgSongs.h"
#include "DlgTemplatesList.h"
#include "DlgHistory.h"
#include "PlaylistItemSong.h"
#include "Player.h"
#include "DlgPickTemplate.h"
#include "DlgBackgroundTaskList.h"
#include "PlaybackBuffer.h"
#include "MetadataScanner.h"
#include "LengthHashCalculator.h"
#include "Settings.h"
#include "DlgSongProperties.h"
#include "Utils.h"
#include "DlgRemovedSongs.h"
#include "DJControllers.h"
#include "DlgImportDB.h"
#include "DatabaseImport.h"
#include "LocalVoteServer.h"
#include "DlgLvsStatus.h"






PlayerWindow::PlayerWindow(ComponentCollection & a_Components):
	Super(nullptr),
	m_UI(new Ui::PlayerWindow),
	m_Components(a_Components),
	m_PlaylistDelegate(new PlaylistItemDelegate),
	m_IsLibraryRescanShown(true),
	m_LastLibraryRescanTotal(0),
	m_LastLibraryRescanQueue(-1)
{
	m_PlaylistModel.reset(new PlaylistItemModel(m_Components.get<Player>()->playlist()));

	m_UI->setupUi(this);
	m_UI->splitter->setCollapsible(1, false);
	Settings::loadSplitterSizes("PlayerWindow", "splitter", *m_UI->splitter);
	m_UI->tblPlaylist->setModel(m_PlaylistModel.get());
	m_UI->tblPlaylist->setItemDelegate(m_PlaylistDelegate.get());
	m_UI->tblPlaylist->setDropIndicatorShown(true);
	m_UI->waveform->setPlayer(*m_Components.get<Player>());

	// Set labels' minimum width to avoid layout changes in runtime:
	m_UI->lblTotalTime->setMinimumWidth(m_UI->lblTotalTime->fontMetrics().width("00:00"));
	m_UI->lblPosition->setMinimumWidth(m_UI->lblPosition->fontMetrics().width("00:00"));
	m_UI->lblRemaining->setMinimumWidth(m_UI->lblRemaining->fontMetrics().width("-00:00"));

	// Decorate the splitter handle with 3 sunken lines:
	auto lay = new QVBoxLayout(m_UI->splitter->handle(1));
	lay->setSpacing(0);
	lay->setMargin(0);
	for (int i = 0; i < 3; ++i)
	{
		auto frame = new QFrame(m_UI->splitter->handle(1));
		frame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
		frame->setLineWidth(1);
		lay->addWidget(frame);
	}

	// Connect the signals:
	auto db     = m_Components.get<Database>();
	auto player = m_Components.get<Player>();
	auto mc     = m_Components.get<DJControllers>();
	connect(m_UI->btnSongs,               &QPushButton::clicked,                 this,         &PlayerWindow::showSongs);
	connect(m_UI->btnTemplates,           &QPushButton::clicked,                 this,         &PlayerWindow::showTemplates);
	connect(m_UI->btnHistory,             &QPushButton::clicked,                 this,         &PlayerWindow::showHistory);
	connect(m_UI->btnAddFromTemplate,     &QPushButton::clicked,                 this,         &PlayerWindow::addFromTemplate);
	connect(m_UI->btnPrev,                &QPushButton::clicked,                 player.get(), &Player::prevTrack);
	connect(m_UI->btnPlayPause,           &QPushButton::clicked,                 player.get(), &Player::startPausePlayback);
	connect(m_UI->btnStop,                &QPushButton::clicked,                 player.get(), &Player::stopPlayback);
	connect(m_UI->btnNext,                &QPushButton::clicked,                 player.get(), &Player::nextTrack);
	connect(m_UI->tblPlaylist,            &QTableView::doubleClicked,            this,         &PlayerWindow::trackDoubleClicked);
	connect(m_UI->vsVolume,               &QSlider::sliderMoved,                 this,         &PlayerWindow::volumeSliderMoved);
	connect(m_UI->vsTempo,                &QSlider::valueChanged,                this,         &PlayerWindow::tempoValueChanged);
	connect(m_UI->btnTempoReset,          &QToolButton::clicked,                 this,         &PlayerWindow::resetTempo);
	connect(m_UI->actBackgroundTasks,     &QAction::triggered,                   this,         &PlayerWindow::showBackgroundTasks);
	connect(m_UI->actSongProperties,      &QAction::triggered,                   this,         &PlayerWindow::showSongProperties);
	connect(&m_UpdateTimer,               &QTimer::timeout,                      this,         &PlayerWindow::periodicUIUpdate);
	connect(m_UI->actDeleteFromDisk,      &QAction::triggered,                   this,         &PlayerWindow::deleteSongsFromDisk);
	connect(m_UI->actRemoveFromLibrary,   &QAction::triggered,                   this,         &PlayerWindow::removeSongsFromLibrary);
	connect(m_UI->actRemoveFromPlaylist,  &QAction::triggered,                   this,         &PlayerWindow::deleteSelectedPlaylistItems);
	connect(m_UI->actPlay,                &QAction::triggered,                   this,         &PlayerWindow::jumpToAndPlay);
	connect(m_UI->actSetDurationLimit,    &QAction::triggered,                   this,         &PlayerWindow::setDurationLimit);
	connect(m_UI->actRemoveDurationLimit, &QAction::triggered,                   this,         &PlayerWindow::removeDurationLimit);
	connect(m_UI->actRemovedSongs,        &QAction::triggered,                   this,         &PlayerWindow::showRemovedSongs);
	connect(m_UI->actImportDB,            &QAction::triggered,                   this,         &PlayerWindow::importDB);
	connect(m_UI->actSavePlaylist,        &QAction::triggered,                   this,         &PlayerWindow::savePlaylist);
	connect(m_UI->actToggleLvs,           &QAction::triggered,                   this,         &PlayerWindow::toggleLvs);
	connect(m_UI->actLvsStatus,           &QAction::triggered,                   this,         &PlayerWindow::showLvsStatus);
	connect(m_UI->lwQuickPlay,            &QListWidget::itemClicked,             this,         &PlayerWindow::quickPlayItemClicked);
	connect(m_PlaylistDelegate.get(),     &PlaylistItemDelegate::replaceSong,    this,         &PlayerWindow::replaceSong);
	connect(m_UI->tblPlaylist,            &QWidget::customContextMenuRequested,  this,         &PlayerWindow::showPlaylistContextMenu);
	connect(m_UI->waveform,               &WaveformDisplay::songChanged,         db.get(),     &Database::saveSong);
	connect(player.get(),                 &Player::startedPlayback,              this,         &PlayerWindow::playerStartedPlayback);
	connect(m_UI->chbKeepTempo,           &QCheckBox::toggled,                   player.get(), &Player::setKeepTempo);
	connect(m_UI->chbKeepVolume,          &QCheckBox::toggled,                   player.get(), &Player::setKeepVolume);
	connect(player.get(),                 &Player::tempoCoeffChanged,            this,         &PlayerWindow::tempoCoeffChanged);
	connect(mc.get(),                     &DJControllers::controllerConnected,   this,         &PlayerWindow::djControllerConnected);
	connect(mc.get(),                     &DJControllers::controllerRemoved,     this,         &PlayerWindow::djControllerRemoved);
	connect(mc.get(),                     &DJControllers::setTempoCoeff,         this,         &PlayerWindow::djControllerSetTempoCoeff);
	connect(mc.get(),                     &DJControllers::setVolume,             this,         &PlayerWindow::djControllerSetVolume);
	connect(mc.get(),                     &DJControllers::playPause,             player.get(), &Player::startPausePlayback);
	connect(mc.get(),                     &DJControllers::navigateUp,            this,         &PlayerWindow::djControllerNavigateUp);
	connect(mc.get(),                     &DJControllers::navigateDown,          this,         &PlayerWindow::djControllerNavigateDown);

	// Set up the header sections (defaults, then load from previous session):
	QFontMetrics fm(m_UI->tblPlaylist->horizontalHeader()->font());
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colLength,            fm.width("W000:00:00W"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colGenre,             fm.width("WGenreW"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colMeasuresPerMinute, fm.width("WMPMW"));
	auto defaultWid = m_UI->tblPlaylist->columnWidth(PlaylistItemModel::colDisplayName);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colAuthor,      defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colTitle,       defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colDisplayName, defaultWid * 3);
	Settings::loadHeaderView("PlayerWindow", "tblPlaylist", *m_UI->tblPlaylist->horizontalHeader());

	// Load checkboxes from the last session:
	m_UI->chbImmediatePlayback->setChecked(Settings::loadValue("PlayerWindow", "chbImmediatePlayback.isChecked", true).toBool());
	m_UI->chbAppendUponCompletion->setChecked(Settings::loadValue("PlayerWindow", "chbAppendUponCompletion.isChecked", true).toBool());
	auto keepTempo = Settings::loadValue("PlayerWindow", "chbKeepTempo.isChecked", false).toBool();
	auto keepVolume = Settings::loadValue("PlayerWindow", "chbKeepVolume.isChecked", false).toBool();
	m_UI->chbKeepTempo->setChecked(keepTempo);
	m_UI->chbKeepVolume->setChecked(keepVolume);
	player->setKeepTempo(keepTempo);
	player->setKeepVolume(keepVolume);

	// Set the TempoReset button's size to avoid layout changes while dragging the tempo slider:
	fm = m_UI->btnTempoReset->fontMetrics();
	m_UI->btnTempoReset->setMinimumWidth(
		m_UI->btnTempoReset->sizeHint().width() - fm.width(m_UI->btnTempoReset->text()) + fm.width("+99.9 %")
	);

	// Set up the Tools button:
	auto lvs = m_Components.get<LocalVoteServer>();
	m_UI->actToggleLvs->setChecked(lvs->isStarted());
	m_UI->actLvsStatus->setEnabled(lvs->isStarted());
	auto menu = new QMenu(this);
	menu->addAction(m_UI->actBackgroundTasks);
	menu->addAction(m_UI->actRemovedSongs);
	menu->addSeparator();
	menu->addAction(m_UI->actImportDB);
	menu->addSeparator();
	menu->addAction(m_UI->actSavePlaylist);
	menu->addSeparator();
	menu->addAction(m_UI->actToggleLvs);
	menu->addAction(m_UI->actLvsStatus);
	m_UI->btnTools->setMenu(menu);

	// Add the context-menu actions to their respective controls, so that their shortcuts work:
	m_UI->tblPlaylist->addActions({
		m_UI->actDeleteFromDisk,
		m_UI->actPlay,
		m_UI->actRate,
		m_UI->actRemoveFromLibrary,
		m_UI->actRemoveFromPlaylist,
		m_UI->actSongProperties,
	});
	m_UI->btnTools->addActions({
		m_UI->actBackgroundTasks,
		m_UI->actRemovedSongs,
		m_UI->actImportDB,
		m_UI->actSavePlaylist,
		m_UI->actToggleLvs,
		m_UI->actLvsStatus,
	});

	refreshQuickPlayer();
	refreshAppendUponCompletion();
	auto lastCompletionTemplateName = Settings::loadValue("PlayerWindow", "cbCompletionAppendTemplate.templateName", "").toString();
	auto tmplIndex = m_UI->cbCompletionAppendTemplate->findText(lastCompletionTemplateName);
	m_UI->cbCompletionAppendTemplate->setCurrentIndex(tmplIndex);

	m_UpdateTimer.start(200);
}





PlayerWindow::~PlayerWindow()
{
	Settings::saveValue("PlayerWindow", "cbCompletionAppendTemplate.templateName", m_UI->cbCompletionAppendTemplate->currentText());
	Settings::saveValue("PlayerWindow", "chbKeepVolume.isChecked", m_UI->chbKeepVolume->isChecked());
	Settings::saveValue("PlayerWindow", "chbKeepTempo.isChecked", m_UI->chbKeepTempo->isChecked());
	Settings::saveValue("PlayerWindow", "chbImmediatePlayback.isChecked", m_UI->chbImmediatePlayback->isChecked());
	Settings::saveValue("PlayerWindow", "chbAppendUponCompletion.isChecked", m_UI->chbAppendUponCompletion->isChecked());
	Settings::saveHeaderView("PlayerWindow", "tblPlaylist", *m_UI->tblPlaylist->horizontalHeader());
	Settings::saveSplitterSizes("PlayerWindow", "splitter", *m_UI->splitter);
}





void PlayerWindow::rateSelectedSongs(double a_LocalRating)
{
	for (auto & song: selectedPlaylistSongs())
	{
		song->setLocalRating(a_LocalRating);
		m_Components.get<Database>()->saveSong(song);
	}
}





std::vector<SongPtr> PlayerWindow::selectedPlaylistSongs() const
{
	std::vector<SongPtr> res;
	res.reserve(static_cast<size_t>(m_UI->tblPlaylist->selectionModel()->selectedRows().count()));
	auto player = m_Components.get<Player>();
	for (const auto & idx: m_UI->tblPlaylist->selectionModel()->selectedRows())
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





void PlayerWindow::refreshQuickPlayer()
{
	m_UI->lwQuickPlay->clear();

	// Insert the templates:
	auto templates = m_Components.get<Database>()->templates();
	for (const auto & tmpl: templates)
	{
		auto item = new QListWidgetItem(tmpl->displayName(), m_UI->lwQuickPlay);
		item->setData(Qt::UserRole, QVariant::fromValue(tmpl));
		item->setBackgroundColor(tmpl->bgColor());
	}

	// Insert the favorite filters:
	auto favorites = m_Components.get<Database>()->getFavoriteFilters();
	for (const auto & fav: favorites)
	{
		auto item = new QListWidgetItem(fav->displayName(), m_UI->lwQuickPlay);
		item->setData(Qt::UserRole, QVariant::fromValue(fav));
		item->setBackgroundColor(fav->bgColor());
	}
}





void PlayerWindow::refreshAppendUponCompletion()
{
	// Store the current selection:
	auto selObj = objectToAppendUponCompletion();
	auto selTemplate = selObj.value<TemplatePtr>();
	auto selFilter = selObj.value<FilterPtr>();

	// Insert templates:
	m_UI->cbCompletionAppendTemplate->clear();
	auto tmpls = m_Components.get<Database>()->templates();
	for (const auto & tmpl: tmpls)
	{
		m_UI->cbCompletionAppendTemplate->addItem(tmpl->displayName(), QVariant::fromValue(tmpl));
		if (tmpl == selTemplate)
		{
			m_UI->cbCompletionAppendTemplate->setCurrentIndex(m_UI->cbCompletionAppendTemplate->count() - 1);
		}
	}

	// Insert favorite filters:
	auto filters = m_Components.get<Database>()->getFavoriteFilters();
	for (const auto & filter: filters)
	{
		m_UI->cbCompletionAppendTemplate->addItem(filter->displayName(), QVariant::fromValue(filter));
		if (filter == selFilter)
		{
			m_UI->cbCompletionAppendTemplate->setCurrentIndex(m_UI->cbCompletionAppendTemplate->count() - 1);
		}
	}
}





void PlayerWindow::setSelectedItemsDurationLimit(double a_NewDurationLimit)
{
	auto player = m_Components.get<Player>();
	const auto & items = player->playlist().items();
	for (const auto & row: m_UI->tblPlaylist->selectionModel()->selectedRows())
	{
		items[static_cast<size_t>(row.row())]->setDurationLimit(a_NewDurationLimit);
	}
	// Update the items, starting with the current one:
	// (No changes to historic data)
	player->updateTrackTimesFromCurrent();
}





QVariant PlayerWindow::objectToAppendUponCompletion() const
{
	return m_UI->cbCompletionAppendTemplate->currentData();
}





void PlayerWindow::showSongs()
{
	DlgSongs dlg(m_Components, nullptr, true, this);
	connect(&dlg, &DlgSongs::addSongToPlaylist,    this, &PlayerWindow::addSongToPlaylist);
	connect(&dlg, &DlgSongs::insertSongToPlaylist, this, &PlayerWindow::insertSongToPlaylist);
	dlg.showMaximized();
	dlg.exec();
}





void PlayerWindow::showTemplates()
{
	DlgTemplatesList dlg(m_Components, this);
	dlg.exec();

	refreshQuickPlayer();
	refreshAppendUponCompletion();
}





void PlayerWindow::showHistory()
{
	DlgHistory dlg(m_Components, this);
	connect(&dlg, &DlgHistory::appendSongToPlaylist, this, &PlayerWindow::addSongToPlaylist);
	connect(&dlg, &DlgHistory::insertSongToPlaylist, this, &PlayerWindow::insertSongToPlaylist);
	dlg.exec();
}





void PlayerWindow::addSongToPlaylist(SongPtr a_Song)
{
	addPlaylistItem(std::make_shared<PlaylistItemSong>(a_Song, nullptr));
}





void PlayerWindow::insertSongToPlaylist(SongPtr a_Song)
{
	const auto & sel = m_UI->tblPlaylist->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		// No selection -> append to end
		return addSongToPlaylist(a_Song);
	}
	auto row = sel[0].row() + 1;
	m_Components.get<Player>()->playlist().insertItem(row, std::make_shared<PlaylistItemSong>(a_Song, nullptr));
	m_UI->tblPlaylist->selectionModel()->clearSelection();
	m_UI->tblPlaylist->selectRow(row);
}





void PlayerWindow::addPlaylistItem(std::shared_ptr<IPlaylistItem> a_Item)
{
	m_Components.get<Player>()->playlist().addItem(a_Item);
}





void PlayerWindow::deleteSelectedPlaylistItems()
{
	const auto & sel = m_UI->tblPlaylist->selectionModel()->selectedRows();
	std::vector<int> rows;
	for (const auto & row: sel)
	{
		rows.push_back(row.row());
	}
	std::sort(rows.begin(), rows.end());
	int numErased = 0;
	auto player = m_Components.get<Player>();
	for (auto row: rows)
	{
		player->playlist().deleteItem(row - numErased);
		numErased += 1;  // Each erased row shifts indices upwards by one
	}
	player->updateTrackTimesFromCurrent();
}




void PlayerWindow::trackDoubleClicked(const QModelIndex & a_Track)
{
	if (a_Track.isValid())
	{
		m_Components.get<Player>()->jumpTo(a_Track.row());
	}
}





void PlayerWindow::volumeSliderMoved(int a_NewValue)
{
	m_Components.get<Player>()->setVolume(static_cast<double>(a_NewValue) / 100);
}





void PlayerWindow::addFromTemplate()
{
	DlgPickTemplate dlg(m_Components, this);
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}
	auto tmpl = dlg.selectedTemplate();
	if (tmpl == nullptr)
	{
		return;
	}
	m_Components.get<Player>()->playlist().addFromTemplate(*m_Components.get<Database>(), *tmpl);
}





void PlayerWindow::tempoValueChanged(int a_NewValue)
{
	auto percent = static_cast<double>(a_NewValue) / 3;
	if (a_NewValue >= 0)
	{
		m_UI->btnTempoReset->setText(QString("+%1 %").arg(QString::number(percent, 'f', 1)));
	}
	else
	{
		m_UI->btnTempoReset->setText(QString("-%1 %").arg(QString::number(-percent, 'f', 1)));
	}
	if (!m_IsInternalChange)
	{
		m_Components.get<Player>()->setTempo(static_cast<double>(percent + 100) / 100);
	}
}





void PlayerWindow::resetTempo()
{
	m_UI->vsTempo->setValue(0);
}





void PlayerWindow::showBackgroundTasks()
{
	DlgBackgroundTaskList dlg(this);
	dlg.exec();
}





void PlayerWindow::periodicUIUpdate()
{
	// Update the player UI:
	auto player = m_Components.get<Player>();
	auto position  = static_cast<int>(player->currentPosition() + 0.5);
	auto remaining = static_cast<int>(player->remainingTime() + 0.5);
	auto total     = static_cast<int>(player->totalTime() + 0.5);
	m_UI->lblPosition->setText( QString( "%1:%2").arg(position  / 60).arg(QString::number(position  % 60), 2, '0'));
	m_UI->lblRemaining->setText(QString("-%1:%2").arg(remaining / 60).arg(QString::number(remaining % 60), 2, '0'));
	m_UI->lblTotalTime->setText(QString( "%1:%2").arg(total     / 60).arg(QString::number(total     % 60), 2, '0'));
	m_UI->lblWallClockTime->setText(QTime::currentTime().toString());

	// Update the SongScan UI:
	auto queueLength = m_Components.get<LengthHashCalculator>()->queueLength() * 2 + m_Components.get<MetadataScanner>()->queueLength();
	if (m_LastLibraryRescanQueue != queueLength)
	{
		m_LastLibraryRescanQueue = queueLength;
		if (queueLength == 0)
		{
			if (m_IsLibraryRescanShown)
			{
				m_UI->wLibraryRescan->hide();
				m_IsLibraryRescanShown = false;
			}
		}
		else
		{
			auto numSongs = static_cast<int>(m_Components.get<Database>()->songs().size() * 2);
			if (numSongs != m_LastLibraryRescanTotal)
			{
				m_UI->pbLibraryRescan->setMaximum(numSongs);
				m_LastLibraryRescanTotal = numSongs;
			}
			m_UI->pbLibraryRescan->setValue(std::max(numSongs - queueLength, 0));
			m_UI->pbLibraryRescan->update();  // For some reason setting the value is not enough to redraw
			if (!m_IsLibraryRescanShown)
			{
				m_UI->wLibraryRescan->show();
				m_IsLibraryRescanShown = true;
			}
		}
	}
}





void PlayerWindow::showPlaylistContextMenu(const QPoint & a_Pos)
{
	// Build the context menu:
	const auto & sel = m_UI->tblPlaylist->selectionModel()->selectedRows();
	QMenu contextMenu;
	contextMenu.addAction(m_UI->actPlay);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRate);
	connect(contextMenu.addAction(QString("    * * * * *")), &QAction::triggered, [this](){ rateSelectedSongs(5); });
	connect(contextMenu.addAction(QString("    * * * *")),   &QAction::triggered, [this](){ rateSelectedSongs(4); });
	connect(contextMenu.addAction(QString("    * * *")),     &QAction::triggered, [this](){ rateSelectedSongs(3); });
	connect(contextMenu.addAction(QString("    * *")),       &QAction::triggered, [this](){ rateSelectedSongs(2); });
	connect(contextMenu.addAction(QString("    *")),         &QAction::triggered, [this](){ rateSelectedSongs(1); });
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actSetDurationLimit);
	for (const auto dl: {30, 45, 60, 75, 90, 120, 150})
	{
		auto actQuickOffer = contextMenu.addAction(QString("    %1").arg(Utils::formatTime(dl)));
		connect(actQuickOffer, &QAction::triggered, [this, dl]() { setSelectedItemsDurationLimit(dl); });
		actQuickOffer->setEnabled(!sel.isEmpty());
	}
	contextMenu.addAction(m_UI->actRemoveDurationLimit);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRemoveFromPlaylist);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRemoveFromLibrary);
	contextMenu.addAction(m_UI->actDeleteFromDisk);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actSongProperties);

	// Update actions based on selection:
	m_UI->actSongProperties->setEnabled(sel.count() == 1);
	m_UI->actRemoveFromLibrary->setEnabled(!sel.isEmpty());
	m_UI->actRemoveFromPlaylist->setEnabled(!sel.isEmpty());
	m_UI->actDeleteFromDisk->setEnabled(!sel.isEmpty());
	m_UI->actPlay->setEnabled(sel.count() == 1);
	m_UI->actRate->setEnabled(!sel.isEmpty());
	m_UI->actSetDurationLimit->setEnabled(!sel.isEmpty());
	m_UI->actRemoveDurationLimit->setEnabled(!sel.isEmpty());

	// Display the menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? a_Pos : widget->mapToGlobal(a_Pos);
	contextMenu.exec(pos, nullptr);
}





void PlayerWindow::showSongProperties()
{
	assert(m_UI->tblPlaylist->selectionModel()->selectedRows().count() == 1);

	auto row = m_UI->tblPlaylist->selectionModel()->selectedRows()[0].row();
	auto item = m_Components.get<Player>()->playlist().items()[static_cast<size_t>(row)];
	auto songItem = std::dynamic_pointer_cast<PlaylistItemSong>(item);
	if (songItem == nullptr)
	{
		return;
	}
	auto song = songItem->song();
	DlgSongProperties dlg(m_Components, song, this);
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
		m_Components.get<Database>()->removeSong(*song, true);
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
		m_Components.get<Database>()->removeSong(*song, false);
	}
}





void PlayerWindow::jumpToAndPlay()
{
	const auto & sel = m_UI->tblPlaylist->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		return;
	}
	auto player = m_Components.get<Player>();
	if (player->playlist().currentItemIndex() == sel[0].row())
	{
		// Already playing this track, bail out:
		return;
	}
	player->jumpTo(sel[0].row());
}





void PlayerWindow::quickPlayItemClicked(QListWidgetItem * a_Item)
{
	// Find the position where to insert in the playlist:
	auto player = m_Components.get<Player>();
	auto & playlist = player->playlist();
	auto idx = m_UI->tblPlaylist->currentIndex().row();
	if (idx < 0)
	{
		idx = static_cast<int>(playlist.items().size());
	}
	else
	{
		idx += 1;
	}

	auto data = a_Item->data(Qt::UserRole);
	auto tmpl = data.value<TemplatePtr>();
	int numAdded = 0;
	if (tmpl != nullptr)
	{
		// Insert songs by a template:
		auto chosen = m_Components.get<Database>()->pickSongsForTemplate(*tmpl);
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
		auto chosen = m_Components.get<Database>()->pickSongForFilter(*filter);
		if (chosen == nullptr)
		{
			qDebug() << ": Failed to add a song by filter " << filter->displayName() << " to playlist.";
			return;
		}
		playlist.insertItem(idx, std::make_shared<PlaylistItemSong>(chosen, filter));
		numAdded = 1;
	}
	if (m_UI->chbImmediatePlayback->checkState() == Qt::Checked)
	{
		player->jumpTo(idx);
		if (!player->isPlaying())
		{
			player->startPlayback();
		}
	}
	m_UI->tblPlaylist->selectRow(idx + numAdded - 1);
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





void PlayerWindow::replaceSong(const QModelIndex & a_Index)
{
	// Is it a replace-able song:
	auto idx = a_Index.row();
	auto player = m_Components.get<Player>();
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
	auto song = m_Components.get<Database>()->pickSongForFilter(*filter, pls->song());
	if (song == pls->song())
	{
		// No replacement available
		return;
	}
	player->playlist().replaceItem(idx, std::make_shared<PlaylistItemSong>(song, filter));

	// Special handling for replacing currently-played song:
	if (player->isTrackLoaded() && a_Index.row() == player->playlist().currentItemIndex())
	{
		// Replacing the currently played song, restart the playback of the new item:
		player->jumpTo(a_Index.row());
	}
}





void PlayerWindow::showRemovedSongs()
{
	DlgRemovedSongs dlg(m_Components, this);
	dlg.exec();
}





void PlayerWindow::playerStartedPlayback()
{
	// Check if starting to play the last item:
	if (!m_Components.get<Player>()->playlist().isAtEnd())
	{
		return;
	}

	// Check if the user agreed to appending:
	if (!m_UI->chbAppendUponCompletion->isChecked())
	{
		return;
	}

	// Append:
	auto selObj = objectToAppendUponCompletion();
	auto player = m_Components.get<Player>();
	auto db = m_Components.get<Database>();
	if (selObj.value<TemplatePtr>() != nullptr)
	{
		player->playlist().addFromTemplate(*db, *selObj.value<TemplatePtr>());
	}
	else if (selObj.value<FilterPtr>() != nullptr)
	{
		player->playlist().addFromFilter(*db, *selObj.value<FilterPtr>());
	}
}





void PlayerWindow::tempoCoeffChanged(qreal a_TempoCoeff)
{
	auto value = static_cast<int>((a_TempoCoeff * 100 - 100) * 3);
	m_IsInternalChange = true;
	m_UI->vsTempo->setValue(value);
	m_IsInternalChange = false;
}





void PlayerWindow::importDB()
{
	DlgImportDB dlg(this);
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}

	Database fromDB(m_Components);
	fromDB.open(dlg.m_FileName);
	DatabaseImport import(fromDB, *m_Components.get<Database>(), dlg.m_Options);
}





void PlayerWindow::savePlaylist()
{
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Save playlist:"),
		QString(),
		tr("M3U playlist (*.m3u)")
	);
	if (fileName.isEmpty())
	{
		return;
	}
	QFile f(fileName);
	if (!f.open(QFile::ReadWrite))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot write file"),
			tr("Cannot write to file %1, playlist NOT saved.").arg(fileName)
		);
		return;
	}
	f.write("#EXTM3U\n");
	for (const auto & i: m_Components.get<Player>()->playlist().items())
	{
		auto si = std::dynamic_pointer_cast<PlaylistItemSong>(i);
		if (si == nullptr)
		{
			continue;
		}
		auto filter = si->filter();
		if (filter != nullptr)
		{
			f.write(tr("#SKAUTAN:TMPL:%1:%2\n")
				.arg(QString::fromUtf8(filter->hash().toHex()))
				.arg(filter->displayName())
				.toUtf8()
			);
		}
		f.write(tr("#SKAUTAN:HASH:%1\n").arg(QString::fromUtf8(si->song()->hash().toHex())).toUtf8());
		f.write(si->song()->fileName().toUtf8());
		f.write("\n\n");
	}
}





void PlayerWindow::toggleLvs()
{
	auto lvs = m_Components.get<LocalVoteServer>();
	if (lvs->isStarted())
	{
		lvs->stopServer();
		m_UI->actLvsStatus->setEnabled(false);
	}
	else
	{
		lvs->startServer();
		m_UI->actLvsStatus->setEnabled(true);
	}
}





void PlayerWindow::djControllerConnected(const QString & a_Name)
{
	Q_UNUSED(a_Name);
	auto mc = m_Components.get<DJControllers>();
	// TODO: Nicer LED control - only light up if there's something to play
	mc->setLedPlay(true);
}





void PlayerWindow::djControllerRemoved()
{
	// Nothing needed yet
}





void PlayerWindow::djControllerSetTempoCoeff(qreal a_TempoCoeff)
{
	m_UI->vsTempo->setValue(static_cast<int>(100 * a_TempoCoeff) - 50);
}





void PlayerWindow::djControllerSetVolume(qreal a_Volume)
{
	auto volume = static_cast<int>(100 * a_Volume);
	m_UI->vsVolume->setValue(volume);
	volumeSliderMoved(volume);
}





void PlayerWindow::djControllerNavigateUp()
{
	if (m_UI->tblPlaylist->currentIndex().row() > 0)
	{
		m_UI->tblPlaylist->selectRow(m_UI->tblPlaylist->currentIndex().row() - 1);
	}
	else
	{
		m_UI->tblPlaylist->selectRow(0);
	}
}





void PlayerWindow::djControllerNavigateDown()
{
	m_UI->tblPlaylist->selectRow(m_UI->tblPlaylist->currentIndex().row() + 1);
}





void PlayerWindow::showLvsStatus()
{
	DlgLvsStatus dlg(m_Components, this);
	dlg.exec();
}

#include "PlayerWindow.h"
#include "assert.h"
#include <QDebug>
#include <QShortcut>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
#include <QInputDialog>
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
#include "HashCalculator.h"
#include "Settings.h"
#include "DlgSongProperties.h"
#include "Utils.h"





PlayerWindow::PlayerWindow(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	HashCalculator & a_Hasher,
	Player & a_Player
):
	Super(nullptr),
	m_UI(new Ui::PlayerWindow),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_HashCalculator(a_Hasher),
	m_Player(a_Player),
	m_PlaylistDelegate(new PlaylistItemDelegate),
	m_IsLibraryRescanShown(true),
	m_LastLibraryRescanTotal(0),
	m_LastLibraryRescanQueue(-1)
{
	m_PlaylistModel.reset(new PlaylistItemModel(m_Player.playlist()));

	m_UI->setupUi(this);
	m_UI->splitter->setCollapsible(1, false);
	Settings::loadSplitterSizes("PlayerWindow", "splitter", *m_UI->splitter);
	m_UI->tblPlaylist->setModel(m_PlaylistModel.get());
	m_UI->tblPlaylist->setItemDelegate(m_PlaylistDelegate.get());
	m_UI->tblPlaylist->setDropIndicatorShown(true);
	m_UI->waveform->setPlayer(m_Player);
	QFont fnt(m_UI->lblPosition->font());
	if (fnt.pixelSize() > 0)
	{
		fnt.setPixelSize(fnt.pixelSize() * 2);
	}
	else
	{
		fnt.setPointSize(fnt.pointSize() * 2);
	}
	m_UI->lblTotalTime->setFont(fnt);
	m_UI->lblTotalTime->setMinimumWidth(m_UI->lblTotalTime->fontMetrics().width("000:00"));
	m_UI->lblPosition->setMinimumWidth(m_UI->lblPosition->fontMetrics().width("000:00"));
	m_UI->lblRemaining->setMinimumWidth(m_UI->lblRemaining->fontMetrics().width("-000:00"));

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
	connect(m_UI->btnSongs,               &QPushButton::clicked,      this, &PlayerWindow::showSongs);
	connect(m_UI->btnTemplates,           &QPushButton::clicked,      this, &PlayerWindow::showTemplates);
	connect(m_UI->btnHistory,             &QPushButton::clicked,      this, &PlayerWindow::showHistory);
	connect(m_UI->btnAddFromTemplate,     &QPushButton::clicked,      this, &PlayerWindow::addFromTemplate);
	connect(m_UI->btnPrev,                &QPushButton::clicked,      this, &PlayerWindow::prevTrack);
	connect(m_UI->btnPlay,                &QPushButton::clicked,      this, &PlayerWindow::playPause);
	connect(m_UI->btnNext,                &QPushButton::clicked,      this, &PlayerWindow::nextTrack);
	connect(m_UI->tblPlaylist,            &QTableView::doubleClicked, this, &PlayerWindow::trackDoubleClicked);
	connect(m_UI->vsVolume,               &QSlider::sliderMoved,      this, &PlayerWindow::volumeSliderMoved);
	connect(m_UI->vsTempo,                &QSlider::valueChanged,     this, &PlayerWindow::tempoValueChanged);
	connect(m_UI->btnTempoReset,          &QToolButton::clicked,      this, &PlayerWindow::resetTempo);
	connect(m_UI->actBackgroundTasks,     &QAction::triggered,        this, &PlayerWindow::showBackgroundTasks);
	connect(m_UI->actSongProperties,      &QAction::triggered,        this, &PlayerWindow::showSongProperties);
	connect(&m_UpdateTimer,               &QTimer::timeout,           this, &PlayerWindow::periodicUIUpdate);
	connect(m_UI->actDeleteFromDisk,      &QAction::triggered,        this, &PlayerWindow::deleteSongsFromDisk);
	connect(m_UI->actRemoveFromLibrary,   &QAction::triggered,        this, &PlayerWindow::removeSongsFromLibrary);
	connect(m_UI->actRemoveFromPlaylist,  &QAction::triggered,        this, &PlayerWindow::removeSongsFromPlaylist);
	connect(m_UI->actPlay,                &QAction::triggered,        this, &PlayerWindow::jumpToAndPlay);
	connect(m_UI->actSetDurationLimit,    &QAction::triggered,        this, &PlayerWindow::setDurationLimit);
	connect(m_UI->actRemoveDurationLimit, &QAction::triggered,        this, &PlayerWindow::removeDurationLimit);
	connect(m_UI->lwQuickPlayer,          &QListWidget::itemClicked,  this, &PlayerWindow::quickPlayerItemClicked);
	connect(m_PlaylistDelegate.get(),     &PlaylistItemDelegate::replaceSong,   this, &PlayerWindow::replaceSong);
	connect(m_UI->tblPlaylist,            &QWidget::customContextMenuRequested, this, &PlayerWindow::showPlaylistContextMenu);
	connect(m_UI->waveform,               &WaveformDisplay::songChanged, &m_DB, &Database::saveSong);

	// Set up the header sections:
	QFontMetrics fm(m_UI->tblPlaylist->horizontalHeader()->font());
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colLength,            fm.width("W000:00:00W"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colGenre,             fm.width("WGenreW"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colMeasuresPerMinute, fm.width("WMPMW"));
	auto defaultWid = m_UI->tblPlaylist->columnWidth(PlaylistItemModel::colDisplayName);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colAuthor,      defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colTitle,       defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colDisplayName, defaultWid * 3);
	Settings::loadHeaderView("PlayerWindow", "tblPlaylist", *m_UI->tblPlaylist->horizontalHeader());
	m_UI->chbImmediatePlayback->setChecked(Settings::loadValue("PlayerWindow", "chbImmediatePlayback.isChecked", true).toBool());

	// Set the TempoReset button's size to avoid layout changes while dragging the tempo slider:
	fm = m_UI->btnTempoReset->fontMetrics();
	m_UI->btnTempoReset->setMinimumWidth(
		m_UI->btnTempoReset->sizeHint().width() - fm.width(m_UI->btnTempoReset->text()) + fm.width("+99.9 %")
	);

	// Set up the Tools button:
	auto menu = new QMenu(this);
	menu->addAction(m_UI->actBackgroundTasks);
	menu->addSeparator();
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
	});

	refreshQuickPlayer();

	m_UpdateTimer.start(200);
}





PlayerWindow::~PlayerWindow()
{
	Settings::saveValue("PlayerWindow", "chbImmediatePlayback.isChecked", m_UI->chbImmediatePlayback->isChecked());
	Settings::saveHeaderView("PlayerWindow", "tblPlaylist", *m_UI->tblPlaylist->horizontalHeader());
	Settings::saveSplitterSizes("PlayerWindow", "splitter", *m_UI->splitter);
}





void PlayerWindow::rateSelectedSongs(double a_LocalRating)
{
	for (auto & song: selectedPlaylistSongs())
	{
		song->setLocalRating(a_LocalRating);
		m_DB.saveSong(song);
	}
}





std::vector<SongPtr> PlayerWindow::selectedPlaylistSongs() const
{
	std::vector<SongPtr> res;
	res.reserve(static_cast<size_t>(m_UI->tblPlaylist->selectionModel()->selectedRows().count()));
	for (const auto & idx: m_UI->tblPlaylist->selectionModel()->selectedRows())
	{
		auto pli = m_Player.playlist().items()[static_cast<size_t>(idx.row())];
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
	m_UI->lwQuickPlayer->clear();
	auto favorites = m_DB.getFavoriteTemplateItems();
	for (const auto & fav: favorites)
	{
		auto item = new QListWidgetItem(fav->displayName(), m_UI->lwQuickPlayer);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(fav.get()));
		item->setBackgroundColor(fav->bgColor());
	}
}





void PlayerWindow::setSelectedItemsDurationLimit(double a_NewDurationLimit)
{
	const auto & items = m_Player.playlist().items();
	for (const auto & row: m_UI->tblPlaylist->selectionModel()->selectedRows())
	{
		items[row.row()]->setDurationLimit(a_NewDurationLimit);
	}
}





void PlayerWindow::showSongs(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgSongs dlg(m_DB, m_MetadataScanner, m_HashCalculator, nullptr, true, this);
	connect(&dlg, &DlgSongs::addSongToPlaylist, this, &PlayerWindow::addSong);
	dlg.showMaximized();
	dlg.exec();
}





void PlayerWindow::showTemplates(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgTemplatesList dlg(m_DB, m_MetadataScanner, m_HashCalculator, this);
	dlg.exec();

	refreshQuickPlayer();
}





void PlayerWindow::showHistory(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgHistory dlg(m_DB, this);
	dlg.exec();
}





void PlayerWindow::addSong(std::shared_ptr<Song> a_Song)
{
	addPlaylistItem(std::make_shared<PlaylistItemSong>(a_Song, nullptr));
}





void PlayerWindow::addPlaylistItem(std::shared_ptr<IPlaylistItem> a_Item)
{
	m_Player.playlist().addItem(a_Item);
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
	for (auto row: rows)
	{
		m_Player.playlist().deleteItem(row - numErased);
		numErased += 1;  // Each erased row shifts indices upwards by one
	}
}




void PlayerWindow::prevTrack(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	m_Player.prevTrack();
}





void PlayerWindow::playPause(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	m_Player.startPause();
}





void PlayerWindow::nextTrack(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	m_Player.nextTrack();
}





void PlayerWindow::trackDoubleClicked(const QModelIndex & a_Track)
{
	if (a_Track.isValid())
	{
		m_Player.jumpTo(a_Track.row());
	}
}





void PlayerWindow::volumeSliderMoved(int a_NewValue)
{
	m_Player.setVolume(static_cast<double>(a_NewValue) / 100);
}





void PlayerWindow::addFromTemplate()
{
	DlgPickTemplate dlg(m_DB, this);
	if (dlg.exec() != QDialog::Accepted)
	{
		return;
	}
	auto tmpl = dlg.selectedTemplate();
	if (tmpl == nullptr)
	{
		return;
	}
	m_Player.playlist().addFromTemplate(m_DB, *tmpl);
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
	m_Player.setTempo(static_cast<double>(percent + 100) / 100);
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
	auto position  = static_cast<int>(m_Player.currentPosition() + 0.5);
	auto remaining = static_cast<int>(m_Player.remainingTime() + 0.5);
	auto total     = static_cast<int>(m_Player.totalTime() + 0.5);
	m_UI->lblPosition->setText( QString( "%1:%2").arg(position  / 60).arg(QString::number(position  % 60), 2, '0'));
	m_UI->lblRemaining->setText(QString("-%1:%2").arg(remaining / 60).arg(QString::number(remaining % 60), 2, '0'));
	m_UI->lblTotalTime->setText(QString( "%1:%2").arg(total     / 60).arg(QString::number(total     % 60), 2, '0'));

	// Update the SongScan UI:
	auto queueLength = m_HashCalculator.queueLength() * 2 + m_MetadataScanner.queueLength();
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
			auto numSongs = static_cast<int>(m_DB.songs().size() * 2);
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
	auto item = m_Player.playlist().items()[static_cast<size_t>(row)];
	auto songItem = std::dynamic_pointer_cast<PlaylistItemSong>(item);
	if (songItem == nullptr)
	{
		return;
	}
	auto song = songItem->song();
	DlgSongProperties dlg(m_DB, song, this);
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
		m_DB.delSong(*song);
		QFile::remove(song->fileName());
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
		m_DB.delSong(*song);
	}
}





void PlayerWindow::removeSongsFromPlaylist()
{
	std::vector<int> rows;
	for (const auto & idx: m_UI->tblPlaylist->selectionModel()->selectedRows())
	{
		rows.push_back(idx.row());
	}
	m_Player.playlist().deleteItems(std::move(rows));
}





void PlayerWindow::jumpToAndPlay()
{
	const auto & sel = m_UI->tblPlaylist->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		return;
	}
	if (m_Player.playlist().currentItemIndex() == sel[0].row())
	{
		// Already playing this track, bail out:
		return;
	}
	m_Player.jumpTo(sel[0].row());
}





void PlayerWindow::quickPlayerItemClicked(QListWidgetItem * a_Item)
{
	auto item = reinterpret_cast<Template::Item *>(a_Item->data(Qt::UserRole).toULongLong());
	if (item == nullptr)
	{
		return;
	}
	if (!m_Player.playlist().addFromTemplateItem(m_DB, item->shared_from_this()))
	{
		qDebug() << ": Failed to add a template item to playlist.";
		return;
	}
	if (m_UI->chbImmediatePlayback->checkState() == Qt::Checked)
	{
		m_Player.pause();
		m_Player.playlist().setCurrentItem(static_cast<int>(m_Player.playlist().items().size() - 1));
		m_Player.start();
	}
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
	auto idx = static_cast<size_t>(a_Index.row());
	if (idx >= m_Player.playlist().items().size())
	{
		return;
	}
	auto pli = m_Player.playlist().items()[idx];
	auto pls = std::dynamic_pointer_cast<PlaylistItemSong>(pli);
	if (pls == nullptr)
	{
		return;
	}
	if (pls->templateItem() == nullptr)
	{
		return;
	}

	// Get a replacement:
	auto song = m_DB.pickSongForTemplateItem(pls->templateItem(), pls->song());
	if (song == pls->song())
	{
		// No replacement available
		return;
	}
	m_Player.playlist().replaceItem(idx, std::make_shared<PlaylistItemSong>(song, pls->templateItem()));

	// Special handling for replacing currently-played song:
	if (m_Player.isPlaying() && a_Index.row() == m_Player.playlist().currentItemIndex())
	{
		// Replacing the currently played song, restart the playback of the new item:
		m_Player.jumpTo(a_Index.row());
	}
}

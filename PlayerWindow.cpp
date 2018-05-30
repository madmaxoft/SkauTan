#include "PlayerWindow.h"
#include "assert.h"
#include <QDebug>
#include <QShortcut>
#include <QMenu>
#include <QMessageBox>
#include <QFile>
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
	m_IsLibraryRescanShown(true),
	m_LastLibraryRescanTotal(0),
	m_LastLibraryRescanQueue(-1)
{
	m_PlaylistModel.reset(new PlaylistItemModel(m_Player.playlist()));

	m_UI->setupUi(this);
	m_UI->tblPlaylist->setModel(m_PlaylistModel.get());
	m_UI->tblPlaylist->setDropIndicatorShown(true);
	m_scDel.reset(new QShortcut(QKeySequence(Qt::Key_Delete), m_UI->tblPlaylist));
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

	// Connect the signals:
	connect(m_UI->btnSongs,              &QPushButton::clicked,      this, &PlayerWindow::showSongs);
	connect(m_UI->btnTemplates,          &QPushButton::clicked,      this, &PlayerWindow::showTemplates);
	connect(m_UI->btnHistory,            &QPushButton::clicked,      this, &PlayerWindow::showHistory);
	connect(m_UI->btnAddFromTemplate,    &QPushButton::clicked,      this, &PlayerWindow::addFromTemplate);
	connect(m_scDel.get(),               &QShortcut::activated,      this, &PlayerWindow::deleteSelectedPlaylistItems);
	connect(m_UI->btnPrev,               &QPushButton::clicked,      this, &PlayerWindow::prevTrack);
	connect(m_UI->btnPlay,               &QPushButton::clicked,      this, &PlayerWindow::playPause);
	connect(m_UI->btnNext,               &QPushButton::clicked,      this, &PlayerWindow::nextTrack);
	connect(m_UI->tblPlaylist,           &QTableView::doubleClicked, this, &PlayerWindow::trackDoubleClicked);
	connect(m_UI->vsVolume,              &QSlider::sliderMoved,      this, &PlayerWindow::volumeSliderMoved);
	connect(m_UI->vsTempo,               &QSlider::valueChanged,     this, &PlayerWindow::tempoValueChanged);
	connect(m_UI->btnTempoReset,         &QToolButton::clicked,      this, &PlayerWindow::resetTempo);
	connect(m_UI->actBackgroundTasks,    &QAction::triggered,        this, &PlayerWindow::showBackgroundTasks);
	connect(m_UI->actSongProperties,     &QAction::triggered,        this, &PlayerWindow::showSongProperties);
	connect(&m_UpdateTimer,              &QTimer::timeout,           this, &PlayerWindow::periodicUIUpdate);
	connect(m_UI->actDeleteFromDisk,     &QAction::triggered,        this, &PlayerWindow::deleteSongsFromDisk);
	connect(m_UI->actRemoveFromLibrary,  &QAction::triggered,        this, &PlayerWindow::removeSongsFromLibrary);
	connect(m_UI->actRemoveFromPlaylist, &QAction::triggered,        this, &PlayerWindow::removeSongsFromPlaylist);
	connect(m_UI->actPlay,               &QAction::triggered,        this, &PlayerWindow::jumpToAndPlay);
	connect(m_UI->lwQuickPlayer,         &QListWidget::itemClicked,  this, &PlayerWindow::quickPlayerItemClicked);
	connect(m_UI->tblPlaylist,           &QWidget::customContextMenuRequested, this, &PlayerWindow::showPlaylistContextMenu);
	connect(m_UI->waveform,              &WaveformDisplay::songChanged, &m_DB, &Database::saveSong);

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

	refreshQuickPlayer();

	m_UpdateTimer.start(200);
}





PlayerWindow::~PlayerWindow()
{
	Settings::saveHeaderView("PlayerWindow", "tblPlaylist", *m_UI->tblPlaylist->horizontalHeader());
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
	addPlaylistItem(std::make_shared<PlaylistItemSong>(a_Song));
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
	QMenu contextMenu;
	contextMenu.addAction(m_UI->actPlay);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRemoveFromLibrary);
	contextMenu.addAction(m_UI->actDeleteFromDisk);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRate);
	connect(contextMenu.addAction(QString("* * * * *")), &QAction::triggered, [this](){ rateSelectedSongs(5); });
	connect(contextMenu.addAction(QString("* * * *")),   &QAction::triggered, [this](){ rateSelectedSongs(4); });
	connect(contextMenu.addAction(QString("* * *")),     &QAction::triggered, [this](){ rateSelectedSongs(3); });
	connect(contextMenu.addAction(QString("* *")),       &QAction::triggered, [this](){ rateSelectedSongs(2); });
	connect(contextMenu.addAction(QString("*")),         &QAction::triggered, [this](){ rateSelectedSongs(1); });
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actSongProperties);

	// Update actions based on selection:
	const auto & sel = m_UI->tblPlaylist->selectionModel()->selectedRows();
	m_UI->actSongProperties->setEnabled(sel.count() == 1);
	m_UI->actRemoveFromLibrary->setEnabled(!sel.isEmpty());
	m_UI->actRemoveFromPlaylist->setEnabled(!sel.isEmpty());
	m_UI->actDeleteFromDisk->setEnabled(!sel.isEmpty());
	m_UI->actPlay->setEnabled(sel.count() == 1);

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
	if (!m_Player.playlist().addFromTemplateItem(m_DB, *item))
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

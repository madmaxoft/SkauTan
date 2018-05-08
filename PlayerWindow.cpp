#include "PlayerWindow.h"
#include "assert.h"
#include <QDebug>
#include <QShortcut>
#include <QMenu>
#include "ui_PlayerWindow.h"
#include "Database.h"
#include "DlgSongs.h"
#include "DlgTemplatesList.h"
#include "DlgHistory.h"
#include "PlaylistItemSong.h"
#include "Player.h"
#include "DlgPickTemplate.h"
#include "DlgQuickPlayer.h"
#include "DlgBackgroundTaskList.h"
#include "PlaybackBuffer.h"





PlayerWindow::PlayerWindow(Database & a_DB, MetadataScanner & a_Scanner, Player & a_Player):
	Super(nullptr),
	m_UI(new Ui::PlayerWindow),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_Player(a_Player)
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
	connect(m_UI->btnQuickPlayer,     &QPushButton::clicked,      this, &PlayerWindow::showQuickPlayer);
	connect(m_UI->btnSongs,           &QPushButton::clicked,      this, &PlayerWindow::showSongs);
	connect(m_UI->btnTemplates,       &QPushButton::clicked,      this, &PlayerWindow::showTemplates);
	connect(m_UI->btnHistory,         &QPushButton::clicked,      this, &PlayerWindow::showHistory);
	connect(m_UI->btnAddFromTemplate, &QPushButton::clicked,      this, &PlayerWindow::addFromTemplate);
	connect(m_scDel.get(),            &QShortcut::activated,      this, &PlayerWindow::deleteSelectedPlaylistItems);
	connect(m_UI->btnPrev,            &QPushButton::clicked,      this, &PlayerWindow::prevTrack);
	connect(m_UI->btnPlay,            &QPushButton::clicked,      this, &PlayerWindow::playPause);
	connect(m_UI->btnNext,            &QPushButton::clicked,      this, &PlayerWindow::nextTrack);
	connect(m_UI->tblPlaylist,        &QTableView::doubleClicked, this, &PlayerWindow::trackDoubleClicked);
	connect(m_UI->vsVolume,           &QSlider::sliderMoved,      this, &PlayerWindow::volumeSliderMoved);
	connect(m_UI->vsTempo,            &QSlider::valueChanged,     this, &PlayerWindow::tempoValueChanged);
	connect(m_UI->btnTempoReset,      &QToolButton::clicked,      this, &PlayerWindow::resetTempo);
	connect(m_UI->btnTools,           &QPushButton::clicked,      this, &PlayerWindow::showToolsMenu);
	connect(m_UI->actBackgroundTasks, &QAction::triggered,        this, &PlayerWindow::showBackgroundTasks);
	connect(&m_UpdateTimer,           &QTimer::timeout,           this, &PlayerWindow::periodicUIUpdate);

	// Set up the header sections:
	QFontMetrics fm(m_UI->tblPlaylist->horizontalHeader()->font());
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colLength,            fm.width("W000:00:00W"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colGenre,             fm.width("WGenreW"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colMeasuresPerMinute, fm.width("WMPMW"));
	auto defaultWid = m_UI->tblPlaylist->columnWidth(PlaylistItemModel::colDisplayName);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colAuthor,      defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colTitle,       defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colDisplayName, defaultWid * 3);

	// Set the TempoReset button's size to avoid layout changes while dragging the tempo slider:
	fm = m_UI->btnTempoReset->fontMetrics();
	m_UI->btnTempoReset->setMinimumWidth(
		m_UI->btnTempoReset->sizeHint().width() - fm.width(m_UI->btnTempoReset->text()) + fm.width("+99.9 %")
	);

	m_UpdateTimer.start(200);
}





PlayerWindow::~PlayerWindow()
{
	// Nothing explicit needed, but the destructor still needs to be defined in the CPP file due to m_UI
}





void PlayerWindow::showQuickPlayer()
{
	DlgQuickPlayer dlg(m_DB, m_Player);
	connect(&dlg, &DlgQuickPlayer::addAndPlayItem, this,      &PlayerWindow::addAndPlayTemplateItem);
	connect(&dlg, &DlgQuickPlayer::pause,          &m_Player, &Player::pause);
	dlg.exec();
}





void PlayerWindow::showSongs(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgSongs dlg(m_DB, m_MetadataScanner, nullptr, true, this);
	connect(&dlg, &DlgSongs::addSongToPlaylist, this, &PlayerWindow::addSong);
	dlg.showMaximized();
	dlg.exec();
}





void PlayerWindow::showTemplates(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgTemplatesList dlg(m_DB, m_MetadataScanner, this);
	dlg.exec();
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





void PlayerWindow::addAndPlayTemplateItem(Template::Item * a_Item)
{
	if (!m_Player.playlist().addFromTemplateItem(m_DB, *a_Item))
	{
		qDebug() << ": Failed to add a template item to playlist.";
		return;
	}
	m_Player.pause();
	m_Player.playlist().setCurrentItem(static_cast<int>(m_Player.playlist().items().size() - 1));
	m_Player.start();
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





void PlayerWindow::showToolsMenu()
{
	QMenu menu;
	menu.addAction(m_UI->actBackgroundTasks);
	menu.addSeparator();
	menu.exec(m_UI->btnTools->mapToGlobal(QPoint(0, 0)));
}





void PlayerWindow::showBackgroundTasks()
{
	DlgBackgroundTaskList dlg(this);
	dlg.exec();
}





void PlayerWindow::periodicUIUpdate()
{
	auto position  = static_cast<int>(m_Player.currentPosition() + 0.5);
	auto remaining = static_cast<int>(m_Player.remainingTime() + 0.5);
	auto total     = static_cast<int>(m_Player.totalTime() + 0.5);
	m_UI->lblPosition->setText( QString( "%1:%2").arg(position  / 60).arg(QString::number(position  % 60), 2, '0'));
	m_UI->lblRemaining->setText(QString("-%1:%2").arg(remaining / 60).arg(QString::number(remaining % 60), 2, '0'));
	m_UI->lblTotalTime->setText(QString( "%1:%2").arg(total     / 60).arg(QString::number(total     % 60), 2, '0'));
}

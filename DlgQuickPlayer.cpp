#include "DlgQuickPlayer.h"
#include "ui_DlgQuickPlayer.h"
#include <QListWidget>
#include <QTimer>
#include <QDebug>
#include "Database.h"
#include "Player.h"
#include "Playlist.h"





DlgQuickPlayer::DlgQuickPlayer(Database & a_DB, Player & a_Player):
	Super(nullptr),
	m_UI(new Ui::DlgQuickPlayer),
	m_DB(a_DB),
	m_Player(a_Player),
	m_UpdateUITimer(new QTimer)
{
	STOPWATCH("DlgQuickPlayer constructor");

	m_UI->setupUi(this);
	m_UI->waveform->setPlayer(a_Player);

	// Insert all favorite template items into the list:
	auto favorites = m_DB.getFavoriteTemplateItems();
	QIcon playIcon(":/img/play.png");
	for (const auto & fav: favorites)
	{
		auto item = new QListWidgetItem(fav->displayName(), m_UI->lwFavorites);
		item->setIcon(playIcon);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(fav.get()));
		item->setBackgroundColor(fav->bgColor());
	}

	// Connect the signals:
	connect(m_UI->lwFavorites,     &QListWidget::itemClicked,     this,  &DlgQuickPlayer::playClickedItem);
	connect(m_UI->btnClose,        &QPushButton::clicked,         this,  &DlgQuickPlayer::close);
	connect(m_UI->btnPause,        &QPushButton::clicked,         this,  &DlgQuickPlayer::pause);
	connect(&a_Player.playlist(),  &Playlist::currentItemChanged, this,  &DlgQuickPlayer::playlistCurrentItemChanged);
	connect(m_UpdateUITimer.get(), &QTimer::timeout,              this,  &DlgQuickPlayer::updateTimePos);
	connect(m_UI->waveform,        &WaveformDisplay::songChanged, &m_DB, &Database::saveSong);

	// Update the UI every 200 msec:
	m_UpdateUITimer->start(200);
}





DlgQuickPlayer::~DlgQuickPlayer()
{
	// Nothing explicit needed, but the destructor still needs to be defined in the CPP file due to m_UI
}





void DlgQuickPlayer::playClickedItem(QListWidgetItem * a_Item)
{
	auto item = reinterpret_cast<Template::Item *>(a_Item->data(Qt::UserRole).toULongLong());
	if (item == nullptr)
	{
		return;
	}
	emit addAndPlayItem(item);
}





void DlgQuickPlayer::pauseClicked()
{
	emit pause();
}





void DlgQuickPlayer::playlistCurrentItemChanged()
{
	auto item = m_Player.playlist().currentItem();
	if (item == nullptr)
	{
		m_UI->lblTrackInfo->clear();
	}
	else
	{
		m_UI->lblTrackInfo->setText(item->displayName());
	}
}





void DlgQuickPlayer::updateTimePos()
{
	auto position  = static_cast<int>(m_Player.currentPosition() + 0.5);
	auto remaining = static_cast<int>(m_Player.remainingTime() + 0.5);
	auto total     = static_cast<int>(m_Player.totalTime() + 0.5);
	m_UI->lblPosition->setText( QString( "%1:%2").arg(position  / 60).arg(QString::number(position  % 60), 2, '0'));
	m_UI->lblRemaining->setText(QString("-%1:%2").arg(remaining / 60).arg(QString::number(remaining % 60), 2, '0'));
	m_UI->lblTotalTime->setText(QString( "%1:%2").arg(total     / 60).arg(QString::number(total     % 60), 2, '0'));
}

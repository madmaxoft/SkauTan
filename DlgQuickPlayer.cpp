#include "DlgQuickPlayer.h"
#include "ui_DlgQuickPlayer.h"
#include <QListWidget>
#include <QTimer>
#include <QDebug>
#include "Database.h"
#include "Player.h"





DlgQuickPlayer::DlgQuickPlayer(Database & a_DB, Playlist & a_Playlist, Player & a_Player):
	Super(nullptr),
	m_UI(new Ui::DlgQuickPlayer),
	m_DB(a_DB),
	m_Playlist(a_Playlist),
	m_Player(a_Player),
	m_UpdateUITimer(new QTimer)
{
	m_UI->setupUi(this);

	// Insert all favorite template items into the list:
	auto favorites = m_DB.getFavoriteTemplateItems();
	QIcon playIcon(":/img/play.png");
	for (const auto & fav: favorites)
	{
		auto item = new QListWidgetItem(fav->displayName(), m_UI->lwFavorites);
		item->setIcon(playIcon);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(fav.get()));
	}

	// Connect the signals:
	connect(m_UI->lwFavorites,     &QListWidget::itemClicked,     this, &DlgQuickPlayer::playClickedItem);
	connect(m_UI->btnClose,        &QPushButton::clicked,         this, &DlgQuickPlayer::close);
	connect(m_UI->btnPause,        &QPushButton::clicked,         this, &DlgQuickPlayer::pause);
	connect(&a_Playlist,           &Playlist::currentItemChanged, this, &DlgQuickPlayer::playlistCurrentItemChanged);
	connect(m_UpdateUITimer.get(), &QTimer::timeout,              this, &DlgQuickPlayer::updateTimePos);

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
	auto item = m_Playlist.currentItem();
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
	auto curItem = m_Playlist.currentItem();
	auto length = (curItem == nullptr) ? 0 : static_cast<int>(curItem->displayLength() + 0.5);
	auto remaining = length - position;
	m_UI->lblPosition->setText(QString("%1:%2").arg(position / 60).arg(QString::number(position % 60), 2, '0'));
	m_UI->lblRemaining->setText(QString("-%1:%2").arg(remaining / 60).arg(QString::number(remaining % 60), 2, '0'));
	m_IsInternalPositionUpdate = true;
	m_UI->hsPosition->setValue(position);
	m_IsInternalPositionUpdate = false;
}

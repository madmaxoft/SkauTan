#include "ClassroomWindow.hpp"
#include <cassert>
#include <QDebug>
#include <QStyledItemDelegate>
#include <QPainter>
#include "ui_ClassroomWindow.h"
#include "../Settings.hpp"
#include "../ComponentCollection.hpp"
#include "../DB/Database.hpp"
#include "../Audio/Player.hpp"
#include "../Playlist.hpp"
#include "../PlaylistItemSong.hpp"





/** Returns the display title for the specified item.
Concatenates the author and title, each only if present. */
static QString songDisplayText(const Song & a_Song)
{
	auto res = a_Song.primaryAuthor().valueOrDefault();
	if (!a_Song.primaryTitle().isEmpty())
	{
		if (!res.isEmpty())
		{
			res.append(" - ");
		}
		res.append(a_Song.primaryTitle().value());
	}
	const auto & mpm = a_Song.primaryMeasuresPerMinute();
	if (mpm.isPresent())
	{
		auto mpmVal = std::floor(mpm.value() + 0.5);
		res.prepend(QString("[%1] ").arg(mpmVal));
	}
	return res;
}





////////////////////////////////////////////////////////////////////////////////
// ClassroomWindow:

ClassroomWindow::ClassroomWindow(ComponentCollection & a_Components):
	m_UI(new Ui::ClassroomWindow),
	m_Components(a_Components),
	m_PlaylistWindow(nullptr)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("ClassroomWindow", *this);
	Settings::loadSplitterSizes("ClassroomWindow", "splMain", m_UI->splMain);
	m_UI->waveform->setPlayer(*m_Components.get<Player>());

	// Connect the signals:
	auto player = m_Components.get<Player>();
	connect(m_UI->btnPlaylistMode, &QPushButton::clicked,              this,         &ClassroomWindow::switchToPlaylistMode);
	connect(m_UI->lwFilters,       &QListWidget::itemSelectionChanged, this,         &ClassroomWindow::updateSongList);
	connect(m_UI->lwSongs,         &QListWidget::itemDoubleClicked,    this,         &ClassroomWindow::songItemDoubleClicked);
	connect(&m_UpdateTimer,        &QTimer::timeout,                   this,         &ClassroomWindow::periodicUIUpdate);
	connect(m_UI->vsVolume,        &QSlider::sliderMoved,              this,         &ClassroomWindow::volumeSliderMoved);
	connect(m_UI->vsTempo,         &QSlider::valueChanged,             this,         &ClassroomWindow::tempoValueChanged);
	connect(player.get(),          &Player::tempoCoeffChanged,         this,         &ClassroomWindow::playerTempoChanged);
	connect(player.get(),          &Player::volumeChanged,             this,         &ClassroomWindow::playerVolumeChanged);
	connect(m_UI->btnPlayPause,    &QPushButton::clicked,              player.get(), &Player::startPausePlayback);
	connect(m_UI->btnStop,         &QPushButton::clicked,              player.get(), &Player::stopPlayback);

	updateFilterList();
	m_UpdateTimer.start(200);
}





ClassroomWindow::~ClassroomWindow()
{
	Settings::saveSplitterSizes("ClassroomWindow", "splMain", m_UI->splMain);
	Settings::saveWindowPos("ClassroomWindow", *this);
}





void ClassroomWindow::setPlaylistWindow(QWidget & a_PlaylistWindow)
{
	m_PlaylistWindow = &a_PlaylistWindow;
}





void ClassroomWindow::updateFilterList()
{
	auto selFilter = selectedFilter();
	auto db = m_Components.get<Database>();
	const auto & filters = db->filters();
	m_UI->lwFilters->clear();
	for (const auto & filter: filters)
	{
		auto item = new QListWidgetItem(filter->displayName());
		item->setData(Qt::UserRole, QVariant::fromValue(filter));
		item->setBackgroundColor(filter->bgColor());
		m_UI->lwFilters->addItem(item);
		if (filter == selFilter)
		{
			m_UI->lwFilters->setCurrentItem(item);
		}
	}
}





std::shared_ptr<Filter> ClassroomWindow::selectedFilter()
{
	auto curItem = m_UI->lwFilters->currentItem();
	if (curItem == nullptr)
	{
		return nullptr;
	}
	return curItem->data(Qt::UserRole).value<FilterPtr>();
}





void ClassroomWindow::startPlayingSong(std::shared_ptr<Song> a_Song)
{
	if (a_Song == nullptr)
	{
		qWarning() << "Requested playback of nullptr song";
		return;
	}
	auto player = m_Components.get<Player>();
	auto & playlist = player->playlist();
	playlist.addItem(std::make_shared<PlaylistItemSong>(a_Song, selectedFilter()));
	player->jumpTo(static_cast<int>(playlist.items().size()) - 1);
	if (!player->isPlaying())
	{
		player->startPlayback();
	}
	m_UI->lblCurrentlyPlaying->setText(tr("Currently playing: %1").arg(songDisplayText(*a_Song)));
}





void ClassroomWindow::switchToPlaylistMode()
{
	if (m_PlaylistWindow == nullptr)
	{
		qWarning() << "PlaylistWindow not available!";
		assert(!"PlaylistWindow not available");
		return;
	}
	Settings::saveValue("UI", "LastMode", 0);
	m_PlaylistWindow->showMaximized();
	this->hide();
}





void ClassroomWindow::updateSongList()
{
	auto curFilter = selectedFilter();
	m_UI->lwSongs->clear();
	if (curFilter == nullptr)
	{
		return;
	}

	auto & dbSD = m_Components.get<Database>()->songSharedDataMap();
	auto root = curFilter->rootNode();
	for (const auto & sd: dbSD)
	{
		// If any song from the list of duplicates satisfies the filter, add it, but only once:
		for (const auto & song: sd.second->duplicates())
		{
			if (root->isSatisfiedBy(*song))
			{
				auto item = new QListWidgetItem(songDisplayText(*song));
				item->setData(Qt::UserRole, QVariant::fromValue(song->shared_from_this()));
				m_UI->lwSongs->addItem(item);
				break;
			}
		}
	}

	m_UI->lwSongs->sortItems();
}





void ClassroomWindow::songItemDoubleClicked(QListWidgetItem * a_Item)
{
	startPlayingSong(a_Item->data(Qt::UserRole).value<SongPtr>());
}





void ClassroomWindow::periodicUIUpdate()
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
}





void ClassroomWindow::volumeSliderMoved(int a_NewValue)
{
	if (!m_IsInternalChange)
	{
		m_Components.get<Player>()->setVolume(static_cast<double>(a_NewValue) / 100);
	}
}





void ClassroomWindow::tempoValueChanged(int a_NewValue)
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





void ClassroomWindow::playerVolumeChanged(qreal a_Volume)
{
	auto value = static_cast<int>(a_Volume * 100);
	m_IsInternalChange = true;
	m_UI->vsVolume->setValue(value);
	m_IsInternalChange = false;
}





void ClassroomWindow::playerTempoChanged(qreal a_TempoCoeff)
{
	auto value = static_cast<int>((a_TempoCoeff * 100 - 100) * 3);
	m_IsInternalChange = true;
	m_UI->vsTempo->setValue(value);
	m_IsInternalChange = false;
}

#include "ClassroomWindow.hpp"
#include <cassert>
#include <QDebug>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QMenu>
#include <QMessageBox>
#include "ui_ClassroomWindow.h"
#include "../Settings.hpp"
#include "../ComponentCollection.hpp"
#include "../DB/Database.hpp"
#include "../Audio/Player.hpp"
#include "../Playlist.hpp"
#include "../PlaylistItemSong.hpp"
#include "../Utils.hpp"
#include "Dlg/DlgSongProperties.hpp"





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
	m_PlaylistWindow(nullptr),
	m_IsInternalChange(false),
	m_SearchFilter("", QRegularExpression::CaseInsensitiveOption),
	m_TicksUntilSetSearchText(0)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("ClassroomWindow", *this);
	Settings::loadSplitterSizes("ClassroomWindow", "splMain", m_UI->splMain);
	m_UI->waveform->setPlayer(*m_Components.get<Player>());

	// Set labels' minimum width to avoid layout changes in runtime:
	m_UI->lblTotalTime->setMinimumWidth(m_UI->lblTotalTime->fontMetrics().width("00:00"));
	m_UI->lblPosition->setMinimumWidth(m_UI->lblPosition->fontMetrics().width("00:00"));
	m_UI->lblRemaining->setMinimumWidth(m_UI->lblRemaining->fontMetrics().width("-00:00"));

	// Set the TempoReset button's size to avoid layout changes while dragging the tempo slider:
	auto fm = m_UI->btnTempoReset->fontMetrics();
	m_UI->btnTempoReset->setMinimumWidth(
		m_UI->btnTempoReset->sizeHint().width() - fm.width(m_UI->btnTempoReset->text()) + fm.width("+99.9 %")
	);

	// Decorate the splitter handle with 2 sunken lines:
	auto lay = new QHBoxLayout(m_UI->splMain->handle(1));
	lay->setSpacing(0);
	lay->setMargin(0);
	for (int i = 0; i < 2; ++i)
	{
		auto frame = new QFrame(m_UI->splMain->handle(1));
		frame->setFrameStyle(QFrame::VLine | QFrame::Sunken);
		frame->setLineWidth(1);
		lay->addWidget(frame);
	}

	// Connect the signals:
	auto player = m_Components.get<Player>();
	connect(m_UI->btnPlaylistMode,      &QPushButton::clicked,                this,         &ClassroomWindow::switchToPlaylistMode);
	connect(m_UI->lwFilters,            &QListWidget::itemSelectionChanged,   this,         &ClassroomWindow::filterItemSelected);
	connect(m_UI->lwSongs,              &QListWidget::itemDoubleClicked,      this,         &ClassroomWindow::songItemDoubleClicked);
	connect(&m_UpdateTimer,             &QTimer::timeout,                     this,         &ClassroomWindow::periodicUIUpdate);
	connect(m_UI->vsVolume,             &QSlider::sliderMoved,                this,         &ClassroomWindow::volumeSliderMoved);
	connect(m_UI->vsTempo,              &QSlider::valueChanged,               this,         &ClassroomWindow::tempoValueChanged);
	connect(m_UI->btnTempoReset,        &QPushButton::clicked,                this,         &ClassroomWindow::tempoResetClicked);
	connect(player.get(),               &Player::tempoCoeffChanged,           this,         &ClassroomWindow::playerTempoChanged);
	connect(player.get(),               &Player::volumeChanged,               this,         &ClassroomWindow::playerVolumeChanged);
	connect(m_UI->btnPlayPause,         &QPushButton::clicked,                player.get(), &Player::startPausePlayback);
	connect(m_UI->btnStop,              &QPushButton::clicked,                player.get(), &Player::stopPlayback);
	connect(m_UI->chbDurationLimit,     &QCheckBox::clicked,                  this,         &ClassroomWindow::durationLimitClicked);
	connect(m_UI->leDurationLimit,      &QLineEdit::textEdited,               this,         &ClassroomWindow::durationLimitEdited);
	connect(m_UI->actPlay,              &QAction::triggered,                  this,         &ClassroomWindow::playSelectedSong);
	connect(m_UI->actRemoveFromLibrary, &QAction::triggered,                  this,         &ClassroomWindow::removeFromLibrary);
	connect(m_UI->actDeleteFromDisk,    &QAction::triggered,                  this,         &ClassroomWindow::deleteFromDisk);
	connect(m_UI->actProperties,        &QAction::triggered,                  this,         &ClassroomWindow::showSongProperties);
	connect(m_UI->lwSongs,              &QWidget::customContextMenuRequested, this,         &ClassroomWindow::showSongListContextMenu);
	connect(m_UI->lblCurrentlyPlaying,  &QWidget::customContextMenuRequested, this,         &ClassroomWindow::showCurSongContextMenu);
	connect(m_UI->leSearchSongs,        &QLineEdit::textEdited,               this,         &ClassroomWindow::searchTextEdited);

	updateFilterList();
	m_UpdateTimer.start(100);
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
	applyDurationLimitSettings();
}





void ClassroomWindow::applyDurationLimitSettings()
{
	auto player = m_Components.get<Player>();
	auto track = player->currentTrack();
	if (track == nullptr)
	{
		qDebug() << "currentTrack == nullptr";
		return;
	}
	if (!m_UI->chbDurationLimit->isChecked())
	{
		track->setDurationLimit(-1);
		return;
	}
	bool isOK;
	auto durationLimit = Utils::parseTime(m_UI->leDurationLimit->text(), isOK);
	if (!isOK)
	{
		// Unparseable time, don't change the currently applied limit
		return;
	}
	track->setDurationLimit(durationLimit);
}





void ClassroomWindow::updateSongItem(QListWidgetItem & a_Item)
{
	auto song = a_Item.data(Qt::UserRole).value<SongPtr>();
	if (song == nullptr)
	{
		assert(!"Invalid song pointer");
		return;
	}
	a_Item.setText(songDisplayText(*song));
	a_Item.setBackgroundColor(song->bgColor().valueOr(QColor(0xff, 0xff, 0xff)));
}





void ClassroomWindow::updateSongItem(Song & a_Song)
{
	auto item = itemFromSong(a_Song);
	if (item != nullptr)
	{
		updateSongItem(*item);
	}
}





void ClassroomWindow::setContextSongBgColor(QColor a_BgColor)
{
	if (m_ContextSong == nullptr)
	{
		return;
	}
	m_ContextSong->setBgColor(a_BgColor);
	m_Components.get<Database>()->saveSong(m_ContextSong);
	updateSongItem(*m_ContextSong);
}





void ClassroomWindow::rateContextSong(double a_LocalRating)
{
	if (m_ContextSong == nullptr)
	{
		return;
	}
	m_ContextSong->setLocalRating(a_LocalRating);
	m_Components.get<Database>()->saveSong(m_ContextSong);
	updateSongItem(*m_ContextSong);
}





QListWidgetItem * ClassroomWindow::itemFromSong(Song & a_Song)
{
	auto numItems = m_UI->lwSongs->count();
	for (int i = 0; i < numItems; ++i)
	{
		auto item = m_UI->lwSongs->item(i);
		if (item == nullptr)
		{
			continue;
		}
		auto itemSong = item->data(Qt::UserRole).value<SongPtr>();
		if (itemSong.get() == &a_Song)
		{
			return item;
		}
	}
	return nullptr;
}





void ClassroomWindow::showSongContextMenu(const QPoint & a_Pos, std::shared_ptr<Song> a_Song)
{
	// The colors for setting the background:
	QColor colors[] =
	{
		{ 0xff, 0xff, 0xff },
		{ 0xff, 0xff, 0x7f },
		{ 0xff, 0x7f, 0xff },
		{ 0x7f, 0xff, 0xff },
		{ 0xff, 0x7f, 0x7f },
		{ 0x7f, 0x7f, 0xff },
		{ 0x7f, 0xff, 0x7f },
	};

	// Build the context menu:
	QMenu contextMenu;
	contextMenu.addAction(m_UI->actPlay);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actSetColor);
	auto size = contextMenu.style()->pixelMetric(QStyle::PM_SmallIconSize);
	for (const auto c: colors)
	{
		auto act = contextMenu.addAction("");
		QPixmap pixmap(size, size);
		pixmap.fill(c);
		act->setIcon(QIcon(pixmap));
		connect(act, &QAction::triggered, [this, c]() { setContextSongBgColor(c); });
	}
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRate);
	connect(contextMenu.addAction(QString("    * * * * *")), &QAction::triggered, [this](){ rateContextSong(5); });
	connect(contextMenu.addAction(QString("    * * * *")),   &QAction::triggered, [this](){ rateContextSong(4); });
	connect(contextMenu.addAction(QString("    * * *")),     &QAction::triggered, [this](){ rateContextSong(3); });
	connect(contextMenu.addAction(QString("    * *")),       &QAction::triggered, [this](){ rateContextSong(2); });
	connect(contextMenu.addAction(QString("    *")),         &QAction::triggered, [this](){ rateContextSong(1); });
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actRemoveFromLibrary);
	contextMenu.addAction(m_UI->actDeleteFromDisk);
	contextMenu.addSeparator();
	contextMenu.addAction(m_UI->actProperties);

	// Display the menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? a_Pos : widget->mapToGlobal(a_Pos);
	m_ContextSong = a_Song;
	contextMenu.exec(pos, nullptr);
}





void ClassroomWindow::applySearchFilterToSongs()
{
	STOPWATCH("Applying search filter to song list");

	m_UI->lwSongs->clear();
	for (const auto & song: m_AllFilterSongs)
	{
		auto item = std::make_unique<QListWidgetItem>();
		item->setData(Qt::UserRole, QVariant::fromValue(song->shared_from_this()));
		updateSongItem(*item);
		if (m_SearchFilter.match(item->text()).hasMatch())
		{
			m_UI->lwSongs->addItem(item.release());
		}
	}
	m_UI->lwSongs->sortItems();
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





void ClassroomWindow::filterItemSelected()
{
	STOPWATCH("Updating song list");

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
				m_AllFilterSongs.push_back(song->shared_from_this());
				break;
			}
		}
	}

	applySearchFilterToSongs();
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

	// If asked to, apply the duration limit settings:
	if (m_TicksToDurationLimitApply > 0)
	{
		m_TicksToDurationLimitApply -= 1;
		if (m_TicksToDurationLimitApply == 0)
		{
			applyDurationLimitSettings();
		}
	}

	// Update the search filter, if appropriate:
	if (m_TicksUntilSetSearchText > 0)
	{
		m_TicksUntilSetSearchText -= 1;
		if (m_TicksUntilSetSearchText == 0)
		{
			m_SearchFilter.setPattern(m_NewSearchText);
			applySearchFilterToSongs();
		}
	}
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





void ClassroomWindow::tempoResetClicked()
{
	m_UI->vsTempo->setValue(0);
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





void ClassroomWindow::durationLimitClicked()
{
	applyDurationLimitSettings();
}





void ClassroomWindow::durationLimitEdited(const QString & a_NewText)
{
	bool isOK;
	Utils::parseTime(a_NewText, isOK);
	if (isOK)
	{
		m_UI->leDurationLimit->setStyleSheet({});
	}
	else
	{
		m_UI->leDurationLimit->setStyleSheet("background-color: #ff7f7f");
	}
	m_TicksToDurationLimitApply = 5;
}





void ClassroomWindow::playSelectedSong()
{
	auto selItem = m_UI->lwSongs->currentItem();
	startPlayingSong(selItem->data(Qt::UserRole).value<SongPtr>());
}





void ClassroomWindow::removeFromLibrary()
{
	// Collect the songs to remove:
	auto selItem = m_UI->lwSongs->currentItem();
	if (selItem == nullptr)
	{
		return;
	}
	auto selSong = selItem->data(Qt::UserRole).value<SongPtr>();
	if (selSong == nullptr)
	{
		assert(!"Invalid song pointer");
		return;
	}
	auto songs = selSong->duplicates();
	if (songs.empty())
	{
		return;
	}

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove song?"),
		tr(
			"Are you sure you want to remove the selected song from the library? The song file will stay "
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





void ClassroomWindow::deleteFromDisk()
{
	// Collect the songs to remove:
	auto selItem = m_UI->lwSongs->currentItem();
	if (selItem == nullptr)
	{
		return;
	}
	auto selSong = selItem->data(Qt::UserRole).value<SongPtr>();
	if (selSong == nullptr)
	{
		assert(!"Invalid song pointer");
		return;
	}
	auto songs = selSong->duplicates();
	if (songs.empty())
	{
		return;
	}

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Delete song?"),
		tr(
			"Are you sure you want to delete the selected song from the disk?"
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





void ClassroomWindow::showSongProperties()
{
	auto selItem = m_UI->lwSongs->currentItem();
	auto selSong = selItem->data(Qt::UserRole).value<SongPtr>();
	DlgSongProperties dlg(m_Components, selSong, this);
	dlg.exec();
	updateSongItem(*selItem);
}





void ClassroomWindow::showSongListContextMenu(const QPoint & a_Pos)
{
	auto selItem = m_UI->lwSongs->currentItem();
	showSongContextMenu(a_Pos, selItem->data(Qt::UserRole).value<SongPtr>());
}





void ClassroomWindow::showCurSongContextMenu(const QPoint & a_Pos)
{
	auto player = m_Components.get<Player>();
	auto curTrack = player->currentTrack();
	if (curTrack == nullptr)
	{
		return;
	}
	auto psi = std::dynamic_pointer_cast<PlaylistItemSong>(curTrack);
	if (psi == nullptr)
	{
		return;
	}
	showSongContextMenu(a_Pos, psi->song());
}





void ClassroomWindow::searchTextEdited(const QString & a_NewSearchText)
{
	m_NewSearchText = a_NewSearchText;
	m_TicksUntilSetSearchText = 3;
}

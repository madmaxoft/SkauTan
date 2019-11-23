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
#include "Dlg/DlgSongs.hpp"
#include "Dlg/DlgManageFilters.hpp"
#include "Dlg/DlgBackgroundTaskList.hpp"
#include "Dlg/DlgRemovedSongs.hpp"
#include "Dlg/DlgImportDB.hpp"
#include "Dlg/DlgLibraryMaintenance.hpp"
#include "Dlg/DlgDebugLog.hpp"





/** Returns the display title for the specified item.
Concatenates the author and title, each only if present.
aTempoCoeff is the current tempo adjust coefficient, from the tempo slider. */
static QString songDisplayText(const Song & aSong, double aTempoCoeff)
{
	auto res = aSong.primaryAuthor().valueOrDefault();
	if (!aSong.primaryTitle().isEmpty())
	{
		if (!res.isEmpty())
		{
			res.append(" - ");
		}
		res.append(aSong.primaryTitle().value());
	}
	const auto & mpm = aSong.primaryMeasuresPerMinute();
	if (mpm.isPresent())
	{
		auto mpmVal = std::floor(aTempoCoeff * mpm.value() + 0.5);
		res.prepend(QString("[%1] ").arg(mpmVal));
	}
	else
	{
		const auto & detectedTempo = aSong.detectedTempo();
		if (detectedTempo.isPresent())
		{
			auto mpmVal = std::floor(aTempoCoeff * detectedTempo.value() + 0.5);
			res.prepend(QString("[%1] ").arg(mpmVal));
		}
	}
	return res;
}





////////////////////////////////////////////////////////////////////////////////
// ClassroomWindow:

ClassroomWindow::ClassroomWindow(ComponentCollection & aComponents):
	mUI(new Ui::ClassroomWindow),
	mComponents(aComponents),
	mPlaylistWindow(nullptr),
	mIsInternalChange(false),
	mSearchFilter("", QRegularExpression::CaseInsensitiveOption),
	mTicksUntilSetSearchText(0),
	mCurrentTempoCoeff(1)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("ClassroomWindow", *this);
	Settings::loadSplitterSizes("ClassroomWindow", "splMain", mUI->splMain);
	mUI->waveform->setPlayer(*mComponents.get<Player>());

	setUpDjControllers();

	// Set labels' minimum width to avoid layout changes in runtime:
	mUI->lblTotalTime->setMinimumWidth(mUI->lblTotalTime->fontMetrics().width("00:00"));
	mUI->lblPosition->setMinimumWidth(mUI->lblPosition->fontMetrics().width("00:00"));
	mUI->lblRemaining->setMinimumWidth(mUI->lblRemaining->fontMetrics().width("-00:00"));

	// Set the TempoReset button's size to avoid layout changes while dragging the tempo slider:
	auto fm = mUI->btnTempoReset->fontMetrics();
	mUI->btnTempoReset->setMinimumWidth(
		mUI->btnTempoReset->sizeHint().width() - fm.width(mUI->btnTempoReset->text()) + fm.width("+99.9 %")
	);

	// Decorate the splitter handle with 2 sunken lines:
	auto lay = new QHBoxLayout(mUI->splMain->handle(1));
	lay->setSpacing(0);
	lay->setMargin(0);
	for (int i = 0; i < 2; ++i)
	{
		auto frame = new QFrame(mUI->splMain->handle(1));
		frame->setFrameStyle(QFrame::VLine | QFrame::Sunken);
		frame->setLineWidth(1);
		lay->addWidget(frame);
	}

	// Connect the signals:
	auto player = mComponents.get<Player>();
	connect(mUI->btnPlaylistMode,       &QPushButton::clicked,                this,         &ClassroomWindow::switchToPlaylistMode);
	connect(mUI->lwFilters,             &QListWidget::itemSelectionChanged,   this,         &ClassroomWindow::filterItemSelected);
	connect(mUI->lwSongs,               &QListWidget::itemDoubleClicked,      this,         &ClassroomWindow::songItemDoubleClicked);
	connect(&mUpdateTimer,              &QTimer::timeout,                     this,         &ClassroomWindow::periodicUIUpdate);
	connect(mUI->vsVolume,              &QSlider::sliderMoved,                this,         &ClassroomWindow::volumeSliderMoved);
	connect(mUI->vsTempo,               &QSlider::valueChanged,               this,         &ClassroomWindow::tempoValueChanged);
	connect(mUI->btnTempoReset,         &QPushButton::clicked,                this,         &ClassroomWindow::tempoResetClicked);
	connect(player.get(),               &Player::tempoCoeffChanged,           this,         &ClassroomWindow::playerTempoChanged);
	connect(player.get(),               &Player::volumeChanged,               this,         &ClassroomWindow::playerVolumeChanged);
	connect(player.get(),               &Player::invalidTrack,                this,         &ClassroomWindow::playerInvalidTrack);
	connect(mUI->btnPlayPause,          &QPushButton::clicked,                player.get(), &Player::startPausePlayback);
	connect(mUI->btnStop,               &QPushButton::clicked,                player.get(), &Player::stopPlayback);
	connect(mUI->chbDurationLimit,      &QCheckBox::clicked,                  this,         &ClassroomWindow::durationLimitClicked);
	connect(mUI->leDurationLimit,       &QLineEdit::textEdited,               this,         &ClassroomWindow::durationLimitEdited);
	connect(mUI->actPlay,               &QAction::triggered,                  this,         &ClassroomWindow::playSelectedSong);
	connect(mUI->actRemoveFromLibrary,  &QAction::triggered,                  this,         &ClassroomWindow::removeFromLibrary);
	connect(mUI->actDeleteFromDisk,     &QAction::triggered,                  this,         &ClassroomWindow::deleteFromDisk);
	connect(mUI->actProperties,         &QAction::triggered,                  this,         &ClassroomWindow::showSongProperties);
	connect(mUI->actShowSongs,          &QAction::triggered,                  this,         &ClassroomWindow::showSongs);
	connect(mUI->actShowFilters,        &QAction::triggered,                  this,         &ClassroomWindow::showFilters);
	connect(mUI->actBackgroundTasks,    &QAction::triggered,                  this,         &ClassroomWindow::showBackgroundTasks);
	connect(mUI->actRemovedSongs,       &QAction::triggered,                  this,         &ClassroomWindow::showRemovedSongs);
	connect(mUI->actImportDB,           &QAction::triggered,                  this,         &ClassroomWindow::importDB);
	connect(mUI->actLibraryMaintenance, &QAction::triggered,                  this,         &ClassroomWindow::libraryMaintenance);
	connect(mUI->actShowDebugLog,       &QAction::triggered,                  this,         &ClassroomWindow::showDebugLog);
	connect(mUI->lwSongs,               &QWidget::customContextMenuRequested, this,         &ClassroomWindow::showSongListContextMenu);
	connect(mUI->lblCurrentlyPlaying,   &QWidget::customContextMenuRequested, this,         &ClassroomWindow::showCurSongContextMenu);
	connect(mUI->leSearchSongs,         &QLineEdit::textEdited,               this,         &ClassroomWindow::searchTextEdited);

	// Set up the Tools button:
	auto menu = new QMenu(this);
	menu->addAction(mUI->actShowSongs);
	menu->addAction(mUI->actShowFilters);
	menu->addSeparator();
	menu->addAction(mUI->actBackgroundTasks);
	menu->addAction(mUI->actRemovedSongs);
	menu->addAction(mUI->actImportDB);
	menu->addAction(mUI->actLibraryMaintenance);
	menu->addSeparator();
	menu->addAction(mUI->actShowDebugLog);
	mUI->btnTools->setMenu(menu);

	// Add the context-menu actions to lwSongs, so that their shortcuts work:
	mUI->lwSongs->addActions({
		mUI->actPlay,
		mUI->actRemoveFromLibrary,
		mUI->actDeleteFromDisk,
		mUI->actProperties,
	});

	updateFilterList();
	mUpdateTimer.start(100);
}





ClassroomWindow::~ClassroomWindow()
{
	Settings::saveSplitterSizes("ClassroomWindow", "splMain", mUI->splMain);
	Settings::saveWindowPos("ClassroomWindow", *this);
}





void ClassroomWindow::setPlaylistWindow(QWidget & aPlaylistWindow)
{
	mPlaylistWindow = &aPlaylistWindow;
}





void ClassroomWindow::updateFilterList()
{
	auto selFilter = selectedFilter();
	auto db = mComponents.get<Database>();
	const auto & filters = db->filters();
	mUI->lwFilters->clear();
	for (const auto & filter: filters)
	{
		auto item = new QListWidgetItem(filter->displayName());
		item->setData(Qt::UserRole, QVariant::fromValue(filter));
		item->setBackgroundColor(filter->bgColor());
		mUI->lwFilters->addItem(item);
		if (filter == selFilter)
		{
			mUI->lwFilters->setCurrentItem(item);
		}
	}
}





std::shared_ptr<Filter> ClassroomWindow::selectedFilter()
{
	auto curItem = mUI->lwFilters->currentItem();
	if (curItem == nullptr)
	{
		return nullptr;
	}
	return curItem->data(Qt::UserRole).value<FilterPtr>();
}





void ClassroomWindow::startPlayingSong(std::shared_ptr<Song> aSong)
{
	if (aSong == nullptr)
	{
		qWarning() << "Requested playback of nullptr song";
		return;
	}
	auto player = mComponents.get<Player>();
	auto & playlist = player->playlist();
	mCurrentSong = aSong;
	playlist.addItem(std::make_shared<PlaylistItemSong>(aSong, selectedFilter()));
	player->jumpTo(static_cast<int>(playlist.items().size()) - 1);
	if (!player->isPlaying())
	{
		player->startPlayback();
	}
	mUI->lblCurrentlyPlaying->setText(tr("Currently playing: %1").arg(songDisplayText(*aSong, mCurrentTempoCoeff)));
	applyDurationLimitSettings();
}





void ClassroomWindow::applyDurationLimitSettings()
{
	auto player = mComponents.get<Player>();
	auto track = player->currentTrack();
	if (track == nullptr)
	{
		qDebug() << "currentTrack == nullptr";
		return;
	}
	if (!mUI->chbDurationLimit->isChecked())
	{
		track->setDurationLimit(-1);
		return;
	}
	bool isOK;
	auto durationLimit = Utils::parseTime(mUI->leDurationLimit->text(), isOK);
	if (!isOK)
	{
		// Unparseable time, don't change the currently applied limit
		return;
	}
	track->setDurationLimit(durationLimit);
}





void ClassroomWindow::updateSongItem(QListWidgetItem & aItem)
{
	auto song = aItem.data(Qt::UserRole).value<SongPtr>();
	if (song == nullptr)
	{
		assert(!"Invalid song pointer");
		return;
	}
	aItem.setText(songDisplayText(*song, mCurrentTempoCoeff));
	QString fileNames;
	for (const auto & s: song->duplicates())
	{
		if (!fileNames.isEmpty())
		{
			fileNames.append('\n');
		}
		fileNames.append(s->fileName());
	}
	aItem.setToolTip(fileNames);
	aItem.setBackgroundColor(song->bgColor().valueOr(QColor(0xff, 0xff, 0xff)));
}





void ClassroomWindow::updateSongItem(Song & aSong)
{
	auto item = itemFromSong(aSong);
	if (item != nullptr)
	{
		updateSongItem(*item);
	}
}





void ClassroomWindow::setContextSongBgColor(QColor aBgColor)
{
	if (mContextSong == nullptr)
	{
		return;
	}
	mContextSong->setBgColor(aBgColor);
	mComponents.get<Database>()->saveSong(mContextSong);
	updateSongItem(*mContextSong);
}





void ClassroomWindow::rateContextSong(double aLocalRating)
{
	if (mContextSong == nullptr)
	{
		return;
	}
	mContextSong->setLocalRating(aLocalRating);
	mComponents.get<Database>()->saveSong(mContextSong);
	updateSongItem(*mContextSong);
}





QListWidgetItem * ClassroomWindow::itemFromSong(Song & aSong)
{
	auto numItems = mUI->lwSongs->count();
	for (int i = 0; i < numItems; ++i)
	{
		auto item = mUI->lwSongs->item(i);
		if (item == nullptr)
		{
			continue;
		}
		auto itemSong = item->data(Qt::UserRole).value<SongPtr>();
		if (itemSong.get() == &aSong)
		{
			return item;
		}
	}
	return nullptr;
}





void ClassroomWindow::showSongContextMenu(const QPoint & aPos, std::shared_ptr<Song> aSong)
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
	contextMenu.addAction(mUI->actPlay);
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actSetColor);
	auto size = contextMenu.style()->pixelMetric(QStyle::PM_SmallIconSize);
	for (const auto & c: colors)
	{
		auto act = contextMenu.addAction("");
		QPixmap pixmap(size, size);
		pixmap.fill(c);
		act->setIcon(QIcon(pixmap));
		connect(act, &QAction::triggered, [this, c]() { setContextSongBgColor(c); });
	}
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actRate);
	connect(contextMenu.addAction(QString("    * * * * *")), &QAction::triggered, [this](){ rateContextSong(5); });
	connect(contextMenu.addAction(QString("    * * * *")),   &QAction::triggered, [this](){ rateContextSong(4); });
	connect(contextMenu.addAction(QString("    * * *")),     &QAction::triggered, [this](){ rateContextSong(3); });
	connect(contextMenu.addAction(QString("    * *")),       &QAction::triggered, [this](){ rateContextSong(2); });
	connect(contextMenu.addAction(QString("    *")),         &QAction::triggered, [this](){ rateContextSong(1); });
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actRemoveFromLibrary);
	contextMenu.addAction(mUI->actDeleteFromDisk);
	contextMenu.addSeparator();
	contextMenu.addAction(mUI->actProperties);

	// Display the menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? aPos : widget->mapToGlobal(aPos);
	mContextSong = aSong;
	contextMenu.exec(pos, nullptr);
}





void ClassroomWindow::applySearchFilterToSongs()
{
	STOPWATCH("Applying search filter to song list");

	mUI->lwSongs->clear();
	for (const auto & song: mAllFilterSongs)
	{
		if (mSearchFilter.match(songDisplayText(*song, mCurrentTempoCoeff)).hasMatch())
		{
			auto item = std::make_unique<QListWidgetItem>();
			item->setData(Qt::UserRole, QVariant::fromValue(song->shared_from_this()));
			updateSongItem(*item);
			mUI->lwSongs->addItem(item.release());
		}
	}
	mUI->lwSongs->sortItems();
}




void ClassroomWindow::applyTempoAdjustToSongs()
{
	int numItems = mUI->lwSongs->count();
	for (int i = 0; i < numItems; ++i)
	{
		updateSongItem(*mUI->lwSongs->item(i));
	}
}





void ClassroomWindow::setUpDjControllers()
{
	const QString CONTEXT = "ClassroomWindow";
	setProperty(DJControllers::CONTEXT_PROPERTY_NAME, CONTEXT);
	auto djc = mComponents.get<DJControllers>();
	mDjKeyHandler    = djc->registerContextKeyHandler(   CONTEXT, this, "handleDjControllerKey");
	mDjSliderHandler = djc->registerContextSliderHandler(CONTEXT, this, "handleDjControllerSlider");
	mDjWheelHandler  = djc->registerContextWheelHandler( CONTEXT, this, "handleDjControllerWheel");
}





void ClassroomWindow::handleDjControllerKey(int aKey)
{
	switch (aKey)
	{
		case AbstractController::skPlayPause1:
		case AbstractController::skPlayPause2:
		{
			mComponents.get<Player>()->startPausePlayback();
			return;
		}
		case AbstractController::skEnter1:
		case AbstractController::skEnter2:
		case AbstractController::skCue1:
		case AbstractController::skCue2:
		{
			playSelectedSong();
			return;
		}
	}
}





void ClassroomWindow::handleDjControllerSlider(int aSlider, qreal aValue)
{
	switch (aSlider)
	{
		case AbstractController::ssVolume1:
		case AbstractController::ssVolume2:
		{
			mUI->vsVolume->setValue(static_cast<int>(aValue * 100));
			return;
		}
		case AbstractController::ssPitch1:
		case AbstractController::ssPitch2:
		{
			mUI->vsTempo->setValue(static_cast<int>(aValue * 100 - 50));
			return;
		}
	}
}





void ClassroomWindow::handleDjControllerWheel(int aWheel, int aNumSteps)
{
	switch (aWheel)
	{
		case AbstractController::swBrowse:
		{
			auto curRow = mUI->lwFilters->currentRow();
			curRow = Utils::clamp(curRow + aNumSteps, 0, mUI->lwFilters->count() - 1);
			mUI->lwFilters->setCurrentRow(curRow);
			return;
		}

		case AbstractController::swJog1:
		case AbstractController::swJog2:
		{
			auto curRow = mUI->lwSongs->currentRow();
			curRow = Utils::clamp(curRow + aNumSteps, 0, mUI->lwSongs->count() - 1);
			mUI->lwSongs->setCurrentRow(curRow);
			return;
		}
	}
}





void ClassroomWindow::switchToPlaylistMode()
{
	if (mPlaylistWindow == nullptr)
	{
		qWarning() << "PlaylistWindow not available!";
		assert(!"PlaylistWindow not available");
		return;
	}
	Settings::saveValue("UI", "LastMode", 0);
	mPlaylistWindow->showMaximized();
	this->hide();
}





void ClassroomWindow::filterItemSelected()
{
	STOPWATCH("Updating song list");

	auto curFilter = selectedFilter();
	mUI->lwSongs->clear();
	mAllFilterSongs.clear();
	if (curFilter == nullptr)
	{
		return;
	}

	auto & dbSD = mComponents.get<Database>()->songSharedDataMap();
	auto root = curFilter->rootNode();
	for (const auto & sd: dbSD)
	{
		// If any song from the list of duplicates satisfies the filter, add it, but only once:
		for (const auto & song: sd.second->duplicates())
		{
			if (root->isSatisfiedBy(*song))
			{
				mAllFilterSongs.push_back(song->shared_from_this());
				break;
			}
		}
	}

	applySearchFilterToSongs();
}





void ClassroomWindow::songItemDoubleClicked(QListWidgetItem * aItem)
{
	startPlayingSong(aItem->data(Qt::UserRole).value<SongPtr>());
}





void ClassroomWindow::periodicUIUpdate()
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

	// If asked to, apply the duration limit settings:
	if (mTicksToDurationLimitApply > 0)
	{
		mTicksToDurationLimitApply -= 1;
		if (mTicksToDurationLimitApply == 0)
		{
			applyDurationLimitSettings();
		}
	}

	// Update the search filter, if appropriate:
	if (mTicksUntilSetSearchText > 0)
	{
		mTicksUntilSetSearchText -= 1;
		if (mTicksUntilSetSearchText == 0)
		{
			mSearchFilter.setPattern(mNewSearchText);
			applySearchFilterToSongs();
		}
	}

	// If asked to, update the song tempos adjusted by the current tempo value:
	if (mTicksUntilUpdateTempo > 0)
	{
		mTicksUntilUpdateTempo -= 1;
		if (mTicksUntilUpdateTempo == 0)
		{
			applyTempoAdjustToSongs();
		}
	}
}





void ClassroomWindow::volumeSliderMoved(int aNewValue)
{
	if (!mIsInternalChange)
	{
		mComponents.get<Player>()->setVolume(static_cast<double>(aNewValue) / 100);
	}
	mUI->lblVolume->setText(QString("%1 %").arg(aNewValue));
}





void ClassroomWindow::tempoValueChanged(int aNewValue)
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
	mComponents.get<Player>()->setTempoCoeff(static_cast<double>(percent + 100) / 100);

	// Schedule an update to the tempo shown with all songs:
	mTicksUntilUpdateTempo = 4;
}





void ClassroomWindow::tempoResetClicked()
{
	mUI->vsTempo->setValue(0);
}





void ClassroomWindow::playerVolumeChanged(qreal aVolume)
{
	auto value = static_cast<int>(aVolume * 100);
	mIsInternalChange = true;
	mUI->vsVolume->setValue(value);
	mIsInternalChange = false;
}





void ClassroomWindow::playerTempoChanged(qreal aTempoCoeff)
{
	mCurrentTempoCoeff = aTempoCoeff;
	auto value = static_cast<int>(round((aTempoCoeff * 100 - 100) * 3));
	mUI->vsTempo->setValue(value);
}





void ClassroomWindow::playerInvalidTrack(IPlaylistItemPtr aTrack)
{
	auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(aTrack);
	if (spi == nullptr)
	{
		return;
	}
	if (spi->song() != mCurrentSong)
	{
		// The player was playing something else, ignore
		return;
	}
	mUI->lblCurrentlyPlaying->setText(tr("FAILED to play: %1").arg(songDisplayText(*mCurrentSong, mCurrentTempoCoeff)));
}





void ClassroomWindow::durationLimitClicked()
{
	applyDurationLimitSettings();
}





void ClassroomWindow::durationLimitEdited(const QString & aNewText)
{
	bool isOK;
	Utils::parseTime(aNewText, isOK);
	if (isOK)
	{
		mUI->leDurationLimit->setStyleSheet({});
	}
	else
	{
		mUI->leDurationLimit->setStyleSheet("background-color: #ff7f7f");
	}
	mTicksToDurationLimitApply = 5;
}





void ClassroomWindow::playSelectedSong()
{
	auto selItem = mUI->lwSongs->currentItem();
	if (selItem == nullptr)
	{
		return;
	}
	startPlayingSong(selItem->data(Qt::UserRole).value<SongPtr>());
}





void ClassroomWindow::removeFromLibrary()
{
	// Collect the songs to remove:
	auto selItem = mUI->lwSongs->currentItem();
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
		mComponents.get<Database>()->removeSong(*song, false);
	}
}





void ClassroomWindow::deleteFromDisk()
{
	// Collect the songs to remove:
	auto selItem = mUI->lwSongs->currentItem();
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
		mComponents.get<Database>()->removeSong(*song, true);
	}
}





void ClassroomWindow::showSongProperties()
{
	auto selItem = mUI->lwSongs->currentItem();
	if (selItem == nullptr)
	{
		return;
	}
	auto selSong = selItem->data(Qt::UserRole).value<SongPtr>();
	DlgSongProperties dlg(mComponents, selSong, this);
	dlg.exec();
	updateSongItem(*selItem);
}





void ClassroomWindow::showSongs()
{
	auto selItem = mUI->lwSongs->currentItem();
	DlgSongs dlg(mComponents, nullptr, true, this);
	dlg.exec();
	if (selItem != nullptr)
	{
		updateSongItem(*selItem);
	}
}





void ClassroomWindow::showFilters()
{
	DlgManageFilters dlg(mComponents, this);
	dlg.exec();
	updateFilterList();
}





void ClassroomWindow::showBackgroundTasks()
{
	DlgBackgroundTaskList dlg(this);
	dlg.exec();
}





void ClassroomWindow::showRemovedSongs()
{
	DlgRemovedSongs dlg(mComponents, this);
	dlg.exec();
}





void ClassroomWindow::importDB()
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





void ClassroomWindow::libraryMaintenance()
{
	DlgLibraryMaintenance dlg(mComponents, this);
	dlg.exec();
}





void ClassroomWindow::showDebugLog()
{
	DlgDebugLog dlg(this);
	dlg.exec();
}





void ClassroomWindow::showSongListContextMenu(const QPoint & aPos)
{
	auto selItem = mUI->lwSongs->currentItem();
	showSongContextMenu(aPos, selItem->data(Qt::UserRole).value<SongPtr>());
}





void ClassroomWindow::showCurSongContextMenu(const QPoint & aPos)
{
	auto player = mComponents.get<Player>();
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
	showSongContextMenu(aPos, psi->song());
}





void ClassroomWindow::searchTextEdited(const QString & aNewSearchText)
{
	mNewSearchText = aNewSearchText;
	mTicksUntilSetSearchText = 3;
}

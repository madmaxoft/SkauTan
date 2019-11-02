#include "DlgSongs.hpp"
#include <cassert>
#include <QFileDialog>
#include <QDebug>
#include <QProcessEnvironment>
#include <QMessageBox>
#include <QComboBox>
#include <QMenu>
#include <QFile>
#include <QInputDialog>
#include "ui_DlgSongs.h"
#include "../../Audio/AVPP.hpp"
#include "../../DB/Database.hpp"
#include "../../MetadataScanner.hpp"
#include "../../Stopwatch.hpp"
#include "../../LengthHashCalculator.hpp"
#include "../../Settings.hpp"
#include "DlgSongProperties.hpp"
#include "DlgTempoDetect.hpp"
#include "DlgTapTempo.hpp"
#include "DlgEditMultipleSongs.hpp"





/** The number of ticks (periodic UI updates) to wait between the user changing the search text
and applying it to the filter. This is to avoid slowdowns while typing the search text. */
static const int TICKS_UNTIL_SET_SEARCH_TEXT = 3;





////////////////////////////////////////////////////////////////////////////////
// DlgSongs:

DlgSongs::DlgSongs(
	ComponentCollection & aComponents,
	std::unique_ptr<QSortFilterProxyModel> && aFilterModel,
	bool aShowManipulators,
	QWidget * aParent
):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::DlgSongs),
	mFilterModel(std::move(aFilterModel)),
	mSongModel(*aComponents.get<Database>()),
	mSongModelFilter(mSongModel),
	mIsLibraryRescanShown(true),
	mLastLibraryRescanTotal(0),
	mLastLibraryRescanQueue(-1),
	mTicksUntilSetSearchText(0)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgSongs", *this);
	if (mFilterModel == nullptr)
	{
		mFilterModel.reset(new QSortFilterProxyModel);
	}
	if (!aShowManipulators)
	{
		mUI->btnAddFolder->hide();
		mUI->btnRemove->hide();
		mUI->btnAddToPlaylist->hide();
	}
	mFilterModel->setSourceModel(&mSongModelFilter);
	mUI->tblSongs->setModel(mFilterModel.get());
	mUI->tblSongs->setItemDelegate(new SongModelEditorDelegate(this));

	// Add the context-menu actions to their respective controls, so that their shortcuts work:
	mUI->tblSongs->addActions({
		mUI->actAddToPlaylist,
		mUI->actInsertIntoPlaylist,
		mUI->actRenameFile,
		mUI->actDeleteFromDisk,
		mUI->actManualToId3,
		mUI->actFileNameToId3,
		mUI->actProperties,
		mUI->actRate,
		mUI->actRemoveFromLibrary,
		mUI->actTempoDetector,
		mUI->actTapTempo,
	});

	// Connect the signals:
	auto db = mComponents.get<Database>();
	connect(mUI->btnAddFile,            &QPushButton::clicked,                   this, &DlgSongs::chooseAddFile);
	connect(mUI->btnAddFolder,          &QPushButton::clicked,                   this, &DlgSongs::chooseAddFolder);
	connect(mUI->btnRemove,             &QPushButton::clicked,                   this, &DlgSongs::removeSelected);
	connect(mUI->btnClose,              &QPushButton::clicked,                   this, &DlgSongs::close);
	connect(mUI->btnAddToPlaylist,      &QPushButton::clicked,                   this, &DlgSongs::addSelectedToPlaylist);
	connect(mUI->btnRescanMetadata,     &QPushButton::clicked,                   this, &DlgSongs::rescanMetadata);
	connect(&mSongModel,                &SongModel::songEdited,                  this, &DlgSongs::modelSongEdited);
	connect(&mSongModel,                &SongModel::rowsInserted,                this, &DlgSongs::updateSongStats);
	connect(db.get(),                    &Database::songFileAdded,                this, &DlgSongs::updateSongStats);
	connect(db.get(),                    &Database::songRemoved,                  this, &DlgSongs::updateSongStats);
	connect(&mPeriodicUiUpdate,         &QTimer::timeout,                        this, &DlgSongs::periodicUiUpdate);
	connect(mUI->tblSongs,              &QTableView::customContextMenuRequested, this, &DlgSongs::showSongsContextMenu);
	connect(mUI->actAddToPlaylist,      &QAction::triggered,                     this, &DlgSongs::addSelectedToPlaylist);
	connect(mUI->actInsertIntoPlaylist, &QAction::triggered,                     this, &DlgSongs::insertSelectedToPlaylist);
	connect(mUI->actDeleteFromDisk,     &QAction::triggered,                     this, &DlgSongs::deleteFromDisk);
	connect(mUI->actProperties,         &QAction::triggered,                     this, &DlgSongs::showProperties);
	connect(mUI->actRate,               &QAction::triggered,                     this, &DlgSongs::rateSelected);
	connect(mUI->actRemoveFromLibrary,  &QAction::triggered,                     this, &DlgSongs::removeSelected);
	connect(mUI->actTempoDetector,      &QAction::triggered,                     this, &DlgSongs::showTempoDetector);
	connect(mUI->actTapTempo,           &QAction::triggered,                     this, &DlgSongs::showTapTempo);
	connect(mUI->actManualToId3,        &QAction::triggered,                     this, &DlgSongs::moveManualToId3);
	connect(mUI->actFileNameToId3,      &QAction::triggered,                     this, &DlgSongs::copyFileNameToId3);
	connect(mUI->actRenameFile,         &QAction::triggered,                     this, &DlgSongs::renameFile);

	initFilterSearch();
	createContextMenu();

	Settings::loadHeaderView("DlgSongs", "tblSongs", *mUI->tblSongs->horizontalHeader());

	// Make the dialog have Maximize button on Windows:
	setWindowFlags(Qt::Window);

	updateSongStats();

	mPeriodicUiUpdate.start(100);
}





DlgSongs::~DlgSongs()
{
	Settings::saveHeaderView("DlgSongs", "tblSongs", *mUI->tblSongs->horizontalHeader());
	Settings::saveWindowPos("DlgSongs", *this);
}





void DlgSongs::addFiles(const QStringList & aFileNames)
{
	// Duplicates are skippen inside mDB, no need to handle them here
	QStringList songs;
	for (const auto & fnam: aFileNames)
	{
		QFileInfo fi(fnam);
		if (!fi.exists())
		{
			continue;
		}
		songs.append(fnam);
	}
	if (songs.empty())
	{
		return;
	}
	qDebug() << "Adding " << songs.size() << " song files";
	mComponents.get<Database>()->addSongFiles(songs);
}





void DlgSongs::addFolderRecursive(const QString & aPath)
{
	QDir dir(aPath + "/");
	QStringList songs;
	for (const auto & item: dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
	{
		if (item.isDir())
		{
			addFolderRecursive(item.absoluteFilePath());
			continue;
		}
		if (!item.isFile())
		{
			continue;
		}
		songs.append(item.absoluteFilePath());
	}
	if (songs.empty())
	{
		return;
	}
	qDebug() << "Adding " << songs.size() << " songs from folder " << aPath;
	mComponents.get<Database>()->addSongFiles(songs);
}





void DlgSongs::updateSongStats()
{
	auto numFiltered = static_cast<size_t>(mFilterModel->rowCount());
	auto numTotal = mComponents.get<Database>()->songs().size();
	if (numFiltered == numTotal)
	{
		mUI->lblStats->setText(tr("Total songs: %1").arg(numTotal));
	}
	else
	{
		mUI->lblStats->setText(tr("Total songs: %1 (filtered out of %2)").arg(numFiltered).arg(numTotal));
	}
}





void DlgSongs::initFilterSearch()
{
	mUI->cbFilter->addItem(tr("All songs"),                          SongModelFilter::fltNone);
	mUI->cbFilter->addItem(tr("Songs without ID3 tag"),              SongModelFilter::fltNoId3);
	mUI->cbFilter->addItem(tr("Songs with no genre"),                SongModelFilter::fltNoGenre);
	mUI->cbFilter->addItem(tr("Songs with no tempo"),                SongModelFilter::fltNoMeasuresPerMinute);
	mUI->cbFilter->addItem(tr("Songs with warnings"),                SongModelFilter::fltWarnings);
	mUI->cbFilter->addItem(tr("Songs not matching any filters"),     SongModelFilter::fltNoFilterMatch);
	mUI->cbFilter->addItem(tr("Songs with duplicates"),              SongModelFilter::fltDuplicates);
	mUI->cbFilter->addItem(tr("Songs with skip-start"),              SongModelFilter::fltSkipStart);
	mUI->cbFilter->addItem(tr("Songs where tempo detection failed"), SongModelFilter::fltFailedTempoDetect);

	// Bind signals / slots:
	// Cannot bind overloaded signal via fn ptr in older Qt, need to use SIGNAL() / SLOT()
	connect(mUI->cbFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChosen(int)));
	connect(mUI->leSearch, &QLineEdit::textEdited,           this, &DlgSongs::searchTextEdited);

	mSongModelFilter.setFavoriteFilters(mComponents.get<Database>()->getFavoriteFilters());
}





void DlgSongs::createContextMenu()
{
	// Create the context menu:
	mContextMenu.reset(new QMenu());
	mContextMenu->addAction(mUI->actAddToPlaylist);
	mContextMenu->addAction(mUI->actInsertIntoPlaylist);
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actRenameFile);
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actRemoveFromLibrary);
	mContextMenu->addAction(mUI->actDeleteFromDisk);
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actManualToId3);
	mContextMenu->addAction(mUI->actFileNameToId3);
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actRate);
	connect(mContextMenu->addAction(QString("    * * * * *")), &QAction::triggered, [this](){ rateSelectedSongs(5); });
	connect(mContextMenu->addAction(QString("    * * * *")),   &QAction::triggered, [this](){ rateSelectedSongs(4); });
	connect(mContextMenu->addAction(QString("    * * *")),     &QAction::triggered, [this](){ rateSelectedSongs(3); });
	connect(mContextMenu->addAction(QString("    * *")),       &QAction::triggered, [this](){ rateSelectedSongs(2); });
	connect(mContextMenu->addAction(QString("    *")),         &QAction::triggered, [this](){ rateSelectedSongs(1); });
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actTapTempo);
	mContextMenu->addAction(mUI->actTempoDetector);
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actProperties);
}





SongPtr DlgSongs::songFromIndex(const QModelIndex & aIndex)
{
	return mSongModel.songFromIndex(
		mSongModelFilter.mapToSource(
			mFilterModel->mapToSource(aIndex)
		)
	);
}





void DlgSongs::rateSelectedSongs(double aRating)
{
	auto db = mComponents.get<Database>();
	foreach(const auto & idx, mUI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		if (song == nullptr)
		{
			qWarning() << "Got a nullptr song from index " << idx;
			continue;
		}
		song->setLocalRating(aRating);
		emit mSongModel.songEdited(song);
		db->saveSong(song);
	}
}





void DlgSongs::chooseAddFile()
{
	auto files = QFileDialog::getOpenFileNames(
		this,
		tr("SkauTan: Choose files to add"),
		QProcessEnvironment::systemEnvironment().value("SKAUTAN_MUSIC_PATH", "")
	);
	if (files.isEmpty())
	{
		return;
	}
	addFiles(files);
}





void DlgSongs::chooseAddFolder()
{
	auto dir = QFileDialog::getExistingDirectory(
		this,
		tr("SkauTan: Choose folder to add"),
		QProcessEnvironment::systemEnvironment().value("SKAUTAN_MUSIC_PATH", "")
	);
	if (dir.isEmpty())
	{
		return;
	}
	addFolderRecursive(dir);
}





void DlgSongs::removeSelected()
{
	// Collect the songs to remove:
	std::vector<SongPtr> songs;
	foreach(const auto & idx, mUI->tblSongs->selectionModel()->selectedRows())
	{
		songs.push_back(songFromIndex(idx));
	}
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
	auto db = mComponents.get<Database>();
	for (const auto & song: songs)
	{
		db->removeSong(*song, false);
	}
}





void DlgSongs::deleteFromDisk()
{
	// Collect the songs to remove:
	std::vector<SongPtr> songs;
	foreach(const auto & idx, mUI->tblSongs->selectionModel()->selectedRows())
	{
		songs.push_back(songFromIndex(idx));
	}
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

	// Remove from the DB and delete files from disk:
	auto db = mComponents.get<Database>();
	for (const auto & song: songs)
	{
		db->removeSong(*song, true);
	}
}





void DlgSongs::addSelectedToPlaylist()
{
	foreach(const auto & idx, mUI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		emit addSongToPlaylist(song);
	}
}





void DlgSongs::insertSelectedToPlaylist()
{
	std::vector<SongPtr> songs;
	foreach(const auto & idx, mUI->tblSongs->selectionModel()->selectedRows())
	{
		songs.push_back(songFromIndex(idx));
	}
	for (auto itr = songs.crbegin(), end = songs.crend(); itr != end; ++itr)
	{
		emit insertSongToPlaylist(*itr);
	}
}





void DlgSongs::rescanMetadata()
{
	auto scanner = mComponents.get<MetadataScanner>();
	foreach(const auto & idx, mUI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		scanner->queueScanSong(song);
	}
}





void DlgSongs::modelSongEdited(SongPtr aSong)
{
	mComponents.get<Database>()->saveSong(aSong);
}





void DlgSongs::periodicUiUpdate()
{
	// Update the LibraryRescan UI:
	// Hash calc is calculated twice for the queue length, because after calculating the hash,
	// songs will go to metadata updater anyway.
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

	// Update the search filter, if appropriate:
	if (mTicksUntilSetSearchText > 0)
	{
		mTicksUntilSetSearchText -= 1;
		if (mTicksUntilSetSearchText == 0)
		{
			mSongModelFilter.setSearchString(mNewSearchText);
			updateSongStats();
		}
	}
}





void DlgSongs::filterChosen(int aIndex)
{
	auto filter = static_cast<SongModelFilter::EFilter>(mUI->cbFilter->itemData(aIndex).toInt());
	mSongModelFilter.setFilter(filter);
	updateSongStats();
}





void DlgSongs::searchTextEdited(const QString & aNewText)
{
	mNewSearchText = aNewText;
	mTicksUntilSetSearchText = TICKS_UNTIL_SET_SEARCH_TEXT;
}





void DlgSongs::showSongsContextMenu(const QPoint & aPos)
{
	// Update the actions based on the selection:
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	mUI->actAddToPlaylist->setEnabled(!sel.isEmpty());
	mUI->actInsertIntoPlaylist->setEnabled(!sel.isEmpty());
	mUI->actRenameFile->setEnabled(sel.count() == 1);
	mUI->actDeleteFromDisk->setEnabled(!sel.isEmpty());
	mUI->actProperties->setEnabled(!sel.isEmpty());
	mUI->actRemoveFromLibrary->setEnabled(!sel.isEmpty());
	mUI->actRate->setEnabled(!sel.isEmpty());
	mUI->actTempoDetector->setEnabled(sel.count() == 1);

	// Show the context menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? aPos : widget->mapToGlobal(aPos);
	mContextMenu->exec(pos, nullptr);
}





void DlgSongs::showProperties()
{
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		return;
	}
	if (sel.size() == 1)
	{
		auto song = songFromIndex(sel[0]);
		DlgSongProperties dlg(mComponents, song, this);
		dlg.exec();
	}
	else
	{
		std::vector<SongPtr> songs;
		for (const auto & s: sel)
		{
			songs.push_back(songFromIndex(s));
		}
		DlgEditMultipleSongs dlg(mComponents, std::move(songs), this);
		dlg.exec();
	}
}





void DlgSongs::rateSelected()
{
	bool isOK;
	auto rating = QInputDialog::getDouble(
		this,
		tr("SkauTan: Rate songs"),
		tr("Rating:"),
		5, 0, 5,
		1, &isOK
	);
	if (!isOK)
	{
		return;
	}

	// Apply the rating:
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	auto db = mComponents.get<Database>();
	for (const auto & idx: sel)
	{
		auto song = songFromIndex(idx);
		if (song == nullptr)
		{
			qWarning() << "Received a nullptr song from index " << idx;
			continue;
		}
		song->setLocalRating(rating);
		emit mSongModel.songEdited(song);
		db->saveSong(song);
	}
}





void DlgSongs::showTempoDetector()
{
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		return;
	}
	auto song = songFromIndex(sel[0]);
	assert(song != nullptr);
	DlgTempoDetect dlg(mComponents, song, this);
	dlg.exec();
}





void DlgSongs::showTapTempo()
{
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	if (sel.isEmpty())
	{
		return;
	}
	auto song = songFromIndex(sel[0]);
	assert(song != nullptr);
	DlgTapTempo dlg(mComponents, song, this);
	dlg.exec();
}





void DlgSongs::moveManualToId3()
{
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	auto db = mComponents.get<Database>();
	for (const auto & row: sel)
	{
		auto song = songFromIndex(row);
		assert(song != nullptr);
		auto tagFile = MetadataScanner::readTagFromFile(song->fileName());
		if (!tagFile.first)
		{
			// Song file not readable / tag not parseable
			continue;
		}
		auto & tagManual = song->tagManual();
		if (!tagManual.mAuthor.isEmpty())
		{
			tagFile.second.mAuthor = tagManual.mAuthor.value();
		}
		if (!tagManual.mTitle.isEmpty())
		{
			tagFile.second.mTitle = tagManual.mTitle.value();
		}
		if (!tagManual.mGenre.isEmpty())
		{
			tagFile.second.mGenre = tagManual.mGenre.value();
		}
		if (tagManual.mMeasuresPerMinute.isPresent())
		{
			tagFile.second.mMeasuresPerMinute = tagManual.mMeasuresPerMinute.value();
		}
		MetadataScanner::writeTagToSong(song, tagFile.second);
		song->clearManualTag();
		db->saveSong(song);
	}
}





void DlgSongs::copyFileNameToId3()
{
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	auto db = mComponents.get<Database>();
	for (const auto & row: sel)
	{
		auto song = songFromIndex(row);
		assert(song != nullptr);
		auto tagFile = MetadataScanner::readTagFromFile(song->fileName());
		if (!tagFile.first)
		{
			// Song file not readable / tag not parseable
			continue;
		}
		auto & tagFileName = song->tagFileName();
		if (!tagFileName.mAuthor.isEmpty())
		{
			tagFile.second.mAuthor = tagFileName.mAuthor.value();
		}
		if (!tagFileName.mTitle.isEmpty())
		{
			tagFile.second.mTitle = tagFileName.mTitle.value();
		}
		if (!tagFileName.mGenre.isEmpty())
		{
			tagFile.second.mGenre = tagFileName.mGenre.value();
		}
		if (tagFileName.mMeasuresPerMinute.isPresent())
		{
			tagFile.second.mMeasuresPerMinute = tagFileName.mMeasuresPerMinute.value();
		}
		MetadataScanner::writeTagToSong(song, tagFile.second);
		db->saveSong(song);
	}
}





void DlgSongs::renameFile()
{
	const auto & sel = mUI->tblSongs->selectionModel()->selectedRows();
	if (sel.count() != 1)
	{
		assert(!"Bad number of selected items");
		return;
	}
	auto song = songFromIndex(sel[0]);
	if (song == nullptr)
	{
		assert(!"Bad songptr");
		return;
	}
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Rename file"),
		song->fileName()
	);
	if (fileName.isEmpty())
	{
		return;
	}
	auto db = mComponents.get<Database>();
	if (!db->renameFile(*song, fileName))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Renaming file failed"),
			tr("Cannot rename file from %1 to %2.").arg(song->fileName()).arg(fileName)
		);
		return;
	}
	song->setFileNameTag(MetadataScanner::parseFileNameIntoMetadata(fileName));
	db->saveSong(song);
}

#include "DlgSongProperties.hpp"
#include <cassert>
#include <ctime>
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <QClipboard>
#include <QFileDialog>
#include "ui_DlgSongProperties.h"
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "../../Utils.hpp"
#include "../../ComponentCollection.hpp"
#include "../../MetadataScanner.hpp"
#include "../../BackgroundTasks.hpp"
#include "../../LengthHashCalculator.hpp"
#include "../../SongTempoDetector.hpp"
#include "DlgTapTempo.hpp"





/** Converts the BPM in text form to the number to be output into the CopyTag strings. */
static double bpmToCopy(const QString & aTextBpm)
{
	if (aTextBpm.isEmpty())
	{
		return -1;
	}
	return aTextBpm.toDouble();
}





////////////////////////////////////////////////////////////////////////////////
// DlgSongProperties:

DlgSongProperties::DlgSongProperties(
	ComponentCollection & aComponents,
	SongPtr aSong,
	QWidget * aParent
) :
	Super(aParent),
	mUI(new Ui::DlgSongProperties),
	mComponents(aComponents),
	mSong(aSong),
	mDuplicates(aSong->duplicates())
{
	// Initialize the ChangeSets:
	mTagManual = mSong->tagManual();
	mNotes = mSong->notes();
	// TODO: move this to a background thread
	for (auto & song: mDuplicates)
	{
		mOriginalID3[song] = MetadataScanner::readTagFromFile(song->fileName());
	}

	// Initialize the UI:
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgSongProperties", *this);
	auto genres = Song::recognizedGenres();
	genres.insert(0, "");
	mUI->cbManualGenre->addItems(genres);
	mUI->cbManualGenre->setMaxVisibleItems(genres.count());
	mUI->lwDuplicates->addActions({
		mUI->actRenameFile,
		mUI->actRemoveFromLibrary,
		mUI->actDeleteFromDisk,
	});

	// Connect the signals:
	auto td = mComponents.get<SongTempoDetector>();
	connect(mUI->btnCancel,                 &QPushButton::clicked,                 this, &DlgSongProperties::reject);
	connect(mUI->btnOK,                     &QPushButton::clicked,                 this, &DlgSongProperties::applyAndClose);
	connect(mUI->leManualAuthor,            &QLineEdit::textEdited,                this, &DlgSongProperties::authorTextEdited);
	connect(mUI->leManualTitle,             &QLineEdit::textEdited,                this, &DlgSongProperties::titleTextEdited);
	connect(mUI->cbManualGenre,             &QComboBox::currentTextChanged,        this, &DlgSongProperties::genreSelected);
	connect(mUI->leManualMeasuresPerMinute, &QLineEdit::textEdited,                this, &DlgSongProperties::measuresPerMinuteTextEdited);
	connect(mUI->pteNotes,                  &QPlainTextEdit::textChanged,          this, &DlgSongProperties::notesChanged);
	connect(mUI->lwDuplicates,              &QListWidget::currentRowChanged,       this, &DlgSongProperties::switchDuplicate);
	connect(mUI->leId3Author,               &QLineEdit::textEdited,                this, &DlgSongProperties::id3AuthorEdited);
	connect(mUI->leId3Title,                &QLineEdit::textEdited,                this, &DlgSongProperties::id3TitleEdited);
	connect(mUI->leId3Genre,                &QLineEdit::textEdited,                this, &DlgSongProperties::id3GenreEdited);
	connect(mUI->leId3Comment,              &QLineEdit::textEdited,                this, &DlgSongProperties::id3CommentEdited);
	connect(mUI->leId3MeasuresPerMinute,    &QLineEdit::textEdited,                this, &DlgSongProperties::id3MeasuresPerMinuteEdited);
	connect(mUI->actRenameFile,             &QAction::triggered,                   this, &DlgSongProperties::renameFile);
	connect(mUI->actRemoveFromLibrary,      &QAction::triggered,                   this, &DlgSongProperties::removeFromLibrary);
	connect(mUI->actDeleteFromDisk,         &QAction::triggered,                   this, &DlgSongProperties::deleteFromDisk);
	connect(mUI->btnCopyId3Tag,             &QPushButton::clicked,                 this, &DlgSongProperties::copyId3Tag);
	connect(mUI->btnCopyPid3Tag,            &QPushButton::clicked,                 this, &DlgSongProperties::copyPid3Tag);
	connect(mUI->btnCopyFilenameTag,        &QPushButton::clicked,                 this, &DlgSongProperties::copyFilenameTag);
	connect(mUI->btnTapTempo,               &QPushButton::clicked,                 this, &DlgSongProperties::showTapTempo);
	connect(td.get(),                       &SongTempoDetector::songTempoDetected, this, &DlgSongProperties::songTempoDetected);

	// Set the read-only edit boxes' palette to greyed-out:
	auto p = palette();
	p.setColor(QPalette::Active,   QPalette::Base, p.color(QPalette::Disabled, QPalette::Base));
	p.setColor(QPalette::Inactive, QPalette::Base, p.color(QPalette::Disabled, QPalette::Base));
	mUI->leHash->setPalette(p);
	mUI->leLength->setPalette(p);
	mUI->leDetectedMeasuresPerMinute->setPalette(p);
	mUI->lePid3Author->setPalette(p);
	mUI->lePid3Title->setPalette(p);
	mUI->lePid3Genre->setPalette(p);
	mUI->lePid3MeasuresPerMinute->setPalette(p);
	mUI->leFilenameAuthor->setPalette(p);
	mUI->leFilenameTitle->setPalette(p);
	mUI->leFilenameGenre->setPalette(p);
	mUI->leFilenameMeasuresPerMinute->setPalette(p);

	// Fill in the data:
	mIsInternalChange = true;
	mUI->leHash->setText(Utils::toHex(aSong->hash()));
	auto length = mSong->length();
	if (length.isPresent())
	{
		auto numSeconds = static_cast<int>(std::floor(length.value() + 0.5));
		mUI->leLength->setText(tr("%1:%2 (%n seconds)", "SongLength", numSeconds)
			.arg(QString::number(numSeconds / 60))
			.arg(QString::number(numSeconds % 60), 2, '0')
		);
	}
	QString detectedMpm;
	if (mSong->sharedData()->mDetectedTempo.isPresent())
	{
		detectedMpm = tr("%1 (detection in progress)").arg(mSong->sharedData()->mDetectedTempo.value());
	}
	else
	{
		detectedMpm = tr("unknown (detection in progress)");
	}
	mUI->leDetectedMeasuresPerMinute->setText(detectedMpm);
	mUI->leManualAuthor->setText(mTagManual.mAuthor.valueOrDefault());
	mUI->leManualTitle->setText(mTagManual.mTitle.valueOrDefault());
	mUI->cbManualGenre->setCurrentText(mTagManual.mGenre.valueOrDefault());
	if (mTagManual.mMeasuresPerMinute.isPresent())
	{
		mUI->leManualMeasuresPerMinute->setText(QLocale::system().toString(mTagManual.mMeasuresPerMinute.value()));
	}
	else
	{
		mUI->leManualMeasuresPerMinute->clear();
	}
	mUI->pteNotes->setPlainText(mNotes.valueOrDefault());
	mIsInternalChange = false;
	fillDuplicates();
	selectSong(*mSong);
	mComponents.get<SongTempoDetector>()->queueDetect(mSong->sharedData());
}





DlgSongProperties::~DlgSongProperties()
{
	Settings::saveWindowPos("DlgSongProperties", *this);
}





void DlgSongProperties::fillDuplicates()
{
	int row = 0;
	for (const auto & song: mDuplicates)
	{
		auto item = new QListWidgetItem(song->fileName());
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(song));
		mUI->lwDuplicates->addItem(item);
		row += 1;
	}
}





void DlgSongProperties::selectSong(const Song & aSong)
{
	// Select the song in tblDuplicates:
	mIsInternalChange = true;
	mSong = songPtrFromRef(aSong);
	assert(mSong != nullptr);
	auto numRows = mUI->lwDuplicates->count();
	for (int row = 0; row < numRows; ++row)
	{
		auto itemSong = reinterpret_cast<const Song *>(mUI->lwDuplicates->item(row)->data(Qt::UserRole).toULongLong());
		if (itemSong == &aSong)
		{
			mUI->lwDuplicates->setCurrentRow(row);
			break;
		}
	}

	// Fill in the ID3 tag, including user-made changes:
	const auto & orig = mOriginalID3[&aSong];
	if (orig.first)
	{
		auto itr = mTagID3Changes.find(&aSong);
		auto tag = orig.second;
		if (itr != mTagID3Changes.end())
		{
			// The user made some changes, apply them into the tag:
			tag = MetadataScanner::applyTagChanges(tag, itr->second);
		}
		mUI->leId3Author->setText(tag.mAuthor.valueOrDefault());
		mUI->leId3Title->setText(tag.mTitle.valueOrDefault());
		mUI->leId3Comment->setText(tag.mComment.valueOrDefault());
		mUI->leId3Genre->setText(tag.mGenre.valueOrDefault());
		if (tag.mMeasuresPerMinute.isPresent())
		{
			const auto loc = QLocale::system();
			mUI->leId3MeasuresPerMinute->setText(loc.toString(tag.mMeasuresPerMinute.value()));
		}
		else
		{
			mUI->leId3MeasuresPerMinute->clear();
		}
		// TODO: Indicate changed fields by using a different bg color or font
	}
	else
	{
		mUI->leId3Author->clear();
		mUI->leId3Title->clear();
		mUI->leId3Comment->clear();
		mUI->leId3Genre->clear();
		mUI->leId3MeasuresPerMinute->clear();
	}
	mUI->leId3Author->setReadOnly(!orig.first);
	mUI->leId3Title->setReadOnly(!orig.first);
	mUI->leId3Comment->setReadOnly(!orig.first);
	mUI->leId3Genre->setReadOnly(!orig.first);
	mUI->leId3MeasuresPerMinute->setReadOnly(!orig.first);
	auto pal = orig.first ? mUI->leManualAuthor->palette() : mUI->leFilenameAuthor->palette();
	mUI->leId3Author->setPalette(pal);
	mUI->leId3Title->setPalette(pal);
	mUI->leId3Comment->setPalette(pal);
	mUI->leId3Genre->setPalette(pal);
	mUI->leId3MeasuresPerMinute->setPalette(pal);

	// Fill in the parsed ID3 values, updated with the user-made changes:
	updateParsedId3();

	// Fill in the filename-deduced values:
	mUI->leFilenameAuthor->setText(aSong.tagFileName().mAuthor.valueOrDefault());
	mUI->leFilenameTitle->setText(aSong.tagFileName().mTitle.valueOrDefault());
	mUI->leFilenameGenre->setText(aSong.tagFileName().mGenre.valueOrDefault());
	if (aSong.tagFileName().mMeasuresPerMinute.isPresent())
	{
		const auto loc = QLocale::system();
		mUI->leFilenameMeasuresPerMinute->setText(loc.toString(aSong.tagFileName().mMeasuresPerMinute.value()));
	}
	else
	{
		mUI->leFilenameMeasuresPerMinute->clear();
	}
	mIsInternalChange = false;
}





SongPtr DlgSongProperties::songPtrFromRef(const Song & aSong)
{
	for (const auto & song: mDuplicates)
	{
		if (song == &aSong)
		{
			return song->shared_from_this();
		}
	}
	return nullptr;
}





void DlgSongProperties::updateParsedId3()
{
	// If the ID3 tag is invalid, bail out:
	const auto & orig = mOriginalID3[mSong.get()];
	if (!orig.first)
	{
		mUI->lePid3Author->clear();
		mUI->lePid3Title->clear();
		mUI->lePid3Genre->clear();
		mUI->lePid3MeasuresPerMinute->clear();
		return;
	}

	// Create a merged tag value from the original and user-edits:
	MetadataScanner::Tag toDisplay(orig.second);
	auto itr = mTagID3Changes.find(mSong.get());
	if (itr != mTagID3Changes.end())
	{
		toDisplay = MetadataScanner::applyTagChanges(toDisplay, itr->second);
	}

	// Fill in the parsed values:
	const auto parsedId3 = MetadataScanner::parseId3Tag(toDisplay);
	mUI->lePid3Author->setText(parsedId3.mAuthor.valueOrDefault());
	mUI->lePid3Title->setText(parsedId3.mTitle.valueOrDefault());
	mUI->lePid3Genre->setText(parsedId3.mGenre.valueOrDefault());
	if (parsedId3.mMeasuresPerMinute.isPresent())
	{
		const auto loc = QLocale::system();
		mUI->lePid3MeasuresPerMinute->setText(loc.toString(parsedId3.mMeasuresPerMinute.value()));
	}
	else
	{
		mUI->lePid3MeasuresPerMinute->clear();
	}
}





void DlgSongProperties::applyAndClose()
{
	// Save the shared data:
	mSong->sharedData()->mTagManual = mTagManual;
	mSong->sharedData()->mNotes = mNotes;
	auto db = mComponents.get<Database>();
	db->saveSongSharedData(mSong->sharedData());

	// Apply the tag changes:
	for (auto song: mDuplicates)
	{
		const auto itr = mTagID3Changes.find(song);
		if (itr != mTagID3Changes.cend())
		{
			assert(mOriginalID3[song].first);  // The original tag must be valid
			auto tag = MetadataScanner::applyTagChanges(mOriginalID3[song].second, itr->second);

			// Write the tag to a file after creating a backup:
			QFileInfo fi(song->fileName());
			auto backupName = tr("%3/backup-%2-%1", "Backup filename format")
				.arg(fi.fileName())
				.arg(std::time(nullptr))
				.arg(fi.path());
			if (!QFile::copy(song->fileName(), backupName))
			{
				qWarning() << "Cannot create backup file for " << song->fileName();
				QMessageBox::warning(
					this,
					tr("SkauTan: Unable to write ID3"),
					tr("SkauTan cannot create a backup copy of file %1 before writing the ID3 tag. "
						"Without creating a backup the ID3 tag cannot be written. "
						"Your changes to the ID3 tag were lost."
					).arg(song->fileName()),
					QMessageBox::Ok, QMessageBox::NoButton
				);
				return;
			}
			MetadataScanner::writeTagToSong(song->shared_from_this(), tag);

			// Check that the song hash hasn't changed; if yes, restore the file from a backup
			auto hashAndLength = LengthHashCalculator::calculateSongHashAndLength(song->fileName());
			if (hashAndLength.first.isEmpty() || hashAndLength.first != song->hash())
			{
				qWarning() << "Song has a different hash after writing tag, reverting to backup: " << song->fileName();
				if (!QFile::remove(song->fileName()))
				{
					qWarning() << "Cannot remove new song file before reverting to backup: " << song->fileName();
				}
				if (!QFile::rename(backupName, song->fileName()))
				{
					qWarning() << "Cannot rename backup back to original filename: " << song->fileName();
				}
				QMessageBox::warning(
					this,
					tr("SkauTan: Unable to write ID3"),
					tr("SkauTan detected that the ID3 tag in file %1 is corrupt, "
						"changing it damaged the file. The file has been restored from backup, "
						"your changes to the ID3 tag were lost."
					).arg(song->fileName()),
					QMessageBox::Ok, QMessageBox::NoButton
				);
			}
			else
			{
				QFile::remove(backupName);
			}

			// Save the song in the DB:
			db->saveSong(song->shared_from_this());
		}
	}
	accept();
}





void DlgSongProperties::authorTextEdited(const QString & aNewText)
{
	if (!mIsInternalChange)
	{
		mTagManual.mAuthor = aNewText;
	}
}





void DlgSongProperties::titleTextEdited(const QString & aNewText)
{
	if (!mIsInternalChange)
	{
		mTagManual.mTitle = aNewText;
	}
}





void DlgSongProperties::genreSelected(const QString & aNewGenre)
{
	if (!mIsInternalChange)
	{
		mTagManual.mGenre = aNewGenre;
	}
}





void DlgSongProperties::measuresPerMinuteTextEdited(const QString & aNewText)
{
	if (mIsInternalChange)
	{
		return;
	}
	if (aNewText.isEmpty())
	{
		mUI->leManualMeasuresPerMinute->setStyleSheet("");
		mTagManual.mMeasuresPerMinute.reset();
		return;
	}
	bool isOK;
	auto mpm = QLocale::system().toDouble(aNewText, &isOK);
	if (isOK)
	{
		mUI->leManualMeasuresPerMinute->setStyleSheet("");
		mTagManual.mMeasuresPerMinute = mpm;
	}
	else
	{
		mUI->leManualMeasuresPerMinute->setStyleSheet("background-color:#fcc");
	}
}





void DlgSongProperties::notesChanged()
{
	mNotes = mUI->pteNotes->toPlainText();
}





void DlgSongProperties::switchDuplicate(int aRow)
{
	if ((aRow < 0) || (aRow >= static_cast<int>(mDuplicates.size())))
	{
		qWarning() << "Invalid row: " << aRow << " out of " << mDuplicates.size();
		return;
	}
	auto song = mDuplicates[static_cast<size_t>(aRow)];
	selectSong(*song);
}





void DlgSongProperties::id3AuthorEdited(const QString & aNewText)
{
	if (!mIsInternalChange)
	{
		mTagID3Changes[mSong.get()].mAuthor = aNewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3TitleEdited(const QString & aNewText)
{
	if (!mIsInternalChange)
	{
		mTagID3Changes[mSong.get()].mTitle = aNewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3GenreEdited(const QString & aNewText)
{
	if (!mIsInternalChange)
	{
		mTagID3Changes[mSong.get()].mGenre = aNewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3CommentEdited(const QString & aNewText)
{
	if (!mIsInternalChange)
	{
		mTagID3Changes[mSong.get()].mComment = aNewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3MeasuresPerMinuteEdited(const QString & aNewText)
{
	if (mIsInternalChange)
	{
		return;
	}
	if (aNewText.isEmpty())
	{
		mUI->leId3MeasuresPerMinute->setStyleSheet("");
		mTagID3Changes[mSong.get()].mMeasuresPerMinute.reset();
		updateParsedId3();
		return;
	}
	bool isOK;
	auto mpm = QLocale::system().toDouble(aNewText, &isOK);
	if (isOK)
	{
		mUI->leId3MeasuresPerMinute->setStyleSheet("");
		mTagID3Changes[mSong.get()].mMeasuresPerMinute = mpm;
		updateParsedId3();
	}
	else
	{
		mUI->leId3MeasuresPerMinute->setStyleSheet("background-color:#fcc");
	}
}





void DlgSongProperties::renameFile()
{
	const auto row = mUI->lwDuplicates->currentRow();
	assert(row >= 0);
	assert(row < mDuplicates.size());
	auto song = mDuplicates[static_cast<size_t>(row)];
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
	db->saveSong(song->shared_from_this());
	mUI->lwDuplicates->currentItem()->setText(fileName);
}





void DlgSongProperties::removeFromLibrary()
{
	auto row = mUI->lwDuplicates->currentRow();
	if ((row < 0) || (row >= static_cast<int>(mDuplicates.size())))
	{
		qWarning() << "Invalid row: " << row;
		return;
	}
	auto song = mDuplicates[static_cast<size_t>(row)]->shared_from_this();
	assert(song != nullptr);

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove songs?"),
		tr(
			"Are you sure you want to remove the song %1 from the library? The song file will stay "
			"on the disk, but all properties set in the library will be lost.\n\n"
			"This operation cannot be undone!"
		).arg(song->fileName()),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Remove from the DB:
	mComponents.get<Database>()->removeSong(*song, false);
	mDuplicates.erase(mDuplicates.begin() + row);

	// Remove from the UI:
	delete mUI->lwDuplicates->takeItem(row);
	switchDuplicate(0);
}





void DlgSongProperties::deleteFromDisk()
{
	auto row = mUI->lwDuplicates->currentRow();
	if ((row < 0) || (row >= static_cast<int>(mDuplicates.size())))
	{
		qWarning() << "Invalid row: " << row;
		return;
	}
	auto song = mDuplicates[static_cast<size_t>(row)]->shared_from_this();
	assert(song != nullptr);

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove songs?"),
		tr(
			"Are you sure you want to delete the file %1 from the disk? "
			"The file will be deleted and all its properties set in the library will be lost.\n\n"
			"This operation cannot be undone!"
		).arg(song->fileName()),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Delete from the disk:
	mComponents.get<Database>()->removeSong(*song, true);
	mDuplicates.erase(mDuplicates.begin() + row);
	delete mUI->lwDuplicates->takeItem(row);
}





void DlgSongProperties::copyId3Tag()
{
	QGuiApplication::clipboard()->setText(
		QString::fromUtf8("{\"%1\", \"%2\", \"%3\", \"%4\", %5}")
		.arg(mUI->leId3Author->text())
		.arg(mUI->leId3Title->text())
		.arg(mUI->leId3Comment->text())
		.arg(mUI->leId3Genre->text())
		.arg(bpmToCopy(mUI->leId3MeasuresPerMinute->text()))
	);
}





void DlgSongProperties::copyPid3Tag()
{
	QGuiApplication::clipboard()->setText(
		QString::fromUtf8("{\"%1\", \"%2\", \"%3\", %4}")
		.arg(mUI->lePid3Author->text())
		.arg(mUI->lePid3Title->text())
		.arg(mUI->lePid3Genre->text())
		.arg(bpmToCopy(mUI->lePid3MeasuresPerMinute->text()))
	);
}





void DlgSongProperties::copyFilenameTag()
{
	QGuiApplication::clipboard()->setText(
		QString::fromUtf8("{\"%1\", \"%2\", \"%3\", %4}")
		.arg(mUI->leFilenameAuthor->text())
		.arg(mUI->leFilenameTitle->text())
		.arg(mUI->leFilenameGenre->text())
		.arg(bpmToCopy(mUI->leFilenameMeasuresPerMinute->text()))
	);
}





void DlgSongProperties::showTapTempo()
{
	DlgTapTempo dlg(mComponents, mSong, this);
	if (dlg.exec() == QDialog::Rejected)
	{
		return;
	}
	mTagManual.mMeasuresPerMinute = mSong->tagManual().mMeasuresPerMinute;  // Reload from the saved value
	if (mTagManual.mMeasuresPerMinute.isPresent())
	{
		mUI->leManualMeasuresPerMinute->setText(QLocale::system().toString(mTagManual.mMeasuresPerMinute.value()));
	}
	else
	{
		mUI->leManualMeasuresPerMinute->clear();
	}
}





void DlgSongProperties::songTempoDetected(Song::SharedDataPtr aSongSD)
{
	if (aSongSD != mSong->sharedData())
	{
		return;
	}

	QString detectedMpm;
	if (mSong->sharedData()->mDetectedTempo.isPresent())
	{
		const auto loc = QLocale::system();
		detectedMpm = loc.toString(mSong->sharedData()->mDetectedTempo.value());
	}
	else
	{
		detectedMpm = tr("unknown (detection failed)");
	}
	mUI->leDetectedMeasuresPerMinute->setText(detectedMpm);
}

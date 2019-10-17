#include "DlgSongProperties.hpp"
#include <cassert>
#include <ctime>
#include <QDebug>
#include <QMessageBox>
#include <QFileInfo>
#include <QClipboard>
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
static double bpmToCopy(const QString & a_TextBpm)
{
	if (a_TextBpm.isEmpty())
	{
		return -1;
	}
	return a_TextBpm.toDouble();
}





////////////////////////////////////////////////////////////////////////////////
// DlgSongProperties:

DlgSongProperties::DlgSongProperties(
	ComponentCollection & a_Components,
	SongPtr a_Song,
	QWidget * a_Parent
) :
	Super(a_Parent),
	m_UI(new Ui::DlgSongProperties),
	m_Components(a_Components),
	m_Song(a_Song),
	m_Duplicates(a_Song->duplicates())
{
	// Initialize the ChangeSets:
	m_TagManual = m_Song->tagManual();
	m_Notes = m_Song->notes();
	// TODO: move this to a background thread
	for (auto & song: m_Duplicates)
	{
		m_OriginalID3[song] = MetadataScanner::readTagFromFile(song->fileName());
	}

	// Initialize the UI:
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgSongProperties", *this);
	auto genres = Song::recognizedGenres();
	genres.insert(0, "");
	m_UI->cbManualGenre->addItems(genres);
	m_UI->cbManualGenre->setMaxVisibleItems(genres.count());
	m_UI->lwDuplicates->addActions({
		m_UI->actRemoveFromLibrary,
		m_UI->actDeleteFromDisk,
	});

	// Connect the signals:
	auto td = m_Components.get<SongTempoDetector>();
	connect(m_UI->btnCancel,                 &QPushButton::clicked,                 this, &DlgSongProperties::reject);
	connect(m_UI->btnOK,                     &QPushButton::clicked,                 this, &DlgSongProperties::applyAndClose);
	connect(m_UI->leManualAuthor,            &QLineEdit::textEdited,                this, &DlgSongProperties::authorTextEdited);
	connect(m_UI->leManualTitle,             &QLineEdit::textEdited,                this, &DlgSongProperties::titleTextEdited);
	connect(m_UI->cbManualGenre,             &QComboBox::currentTextChanged,        this, &DlgSongProperties::genreSelected);
	connect(m_UI->leManualMeasuresPerMinute, &QLineEdit::textEdited,                this, &DlgSongProperties::measuresPerMinuteTextEdited);
	connect(m_UI->pteNotes,                  &QPlainTextEdit::textChanged,          this, &DlgSongProperties::notesChanged);
	connect(m_UI->lwDuplicates,              &QListWidget::currentRowChanged,       this, &DlgSongProperties::switchDuplicate);
	connect(m_UI->leId3Author,               &QLineEdit::textEdited,                this, &DlgSongProperties::id3AuthorEdited);
	connect(m_UI->leId3Title,                &QLineEdit::textEdited,                this, &DlgSongProperties::id3TitleEdited);
	connect(m_UI->leId3Genre,                &QLineEdit::textEdited,                this, &DlgSongProperties::id3GenreEdited);
	connect(m_UI->leId3Comment,              &QLineEdit::textEdited,                this, &DlgSongProperties::id3CommentEdited);
	connect(m_UI->leId3MeasuresPerMinute,    &QLineEdit::textEdited,                this, &DlgSongProperties::id3MeasuresPerMinuteEdited);
	connect(m_UI->actRemoveFromLibrary,      &QAction::triggered,                   this, &DlgSongProperties::removeFromLibrary);
	connect(m_UI->actDeleteFromDisk,         &QAction::triggered,                   this, &DlgSongProperties::deleteFromDisk);
	connect(m_UI->btnCopyId3Tag,             &QPushButton::clicked,                 this, &DlgSongProperties::copyId3Tag);
	connect(m_UI->btnCopyPid3Tag,            &QPushButton::clicked,                 this, &DlgSongProperties::copyPid3Tag);
	connect(m_UI->btnCopyFilenameTag,        &QPushButton::clicked,                 this, &DlgSongProperties::copyFilenameTag);
	connect(m_UI->btnTapTempo,               &QPushButton::clicked,                 this, &DlgSongProperties::showTapTempo);
	connect(td.get(),                        &SongTempoDetector::songTempoDetected, this, &DlgSongProperties::songTempoDetected);

	// Set the read-only edit boxes' palette to greyed-out:
	auto p = palette();
	p.setColor(QPalette::Active,   QPalette::Base, p.color(QPalette::Disabled, QPalette::Base));
	p.setColor(QPalette::Inactive, QPalette::Base, p.color(QPalette::Disabled, QPalette::Base));
	m_UI->leHash->setPalette(p);
	m_UI->leLength->setPalette(p);
	m_UI->leDetectedMeasuresPerMinute->setPalette(p);
	m_UI->lePid3Author->setPalette(p);
	m_UI->lePid3Title->setPalette(p);
	m_UI->lePid3Genre->setPalette(p);
	m_UI->lePid3MeasuresPerMinute->setPalette(p);
	m_UI->leFilenameAuthor->setPalette(p);
	m_UI->leFilenameTitle->setPalette(p);
	m_UI->leFilenameGenre->setPalette(p);
	m_UI->leFilenameMeasuresPerMinute->setPalette(p);

	// Fill in the data:
	m_IsInternalChange = true;
	m_UI->leHash->setText(Utils::toHex(a_Song->hash()));
	auto length = m_Song->length();
	if (length.isPresent())
	{
		auto numSeconds = static_cast<int>(std::floor(length.value() + 0.5));
		m_UI->leLength->setText(tr("%1:%2 (%n seconds)", "SongLength", numSeconds)
			.arg(QString::number(numSeconds / 60))
			.arg(QString::number(numSeconds % 60), 2, '0')
		);
	}
	QString detectedMpm;
	if (m_Song->sharedData()->m_DetectedTempo.isPresent())
	{
		detectedMpm = tr("%1 (detection in progress)").arg(m_Song->sharedData()->m_DetectedTempo.value());
	}
	else
	{
		detectedMpm = tr("unknown (detection in progress)");
	}
	m_UI->leDetectedMeasuresPerMinute->setText(detectedMpm);
	m_UI->leManualAuthor->setText(m_TagManual.m_Author.valueOrDefault());
	m_UI->leManualTitle->setText(m_TagManual.m_Title.valueOrDefault());
	m_UI->cbManualGenre->setCurrentText(m_TagManual.m_Genre.valueOrDefault());
	if (m_TagManual.m_MeasuresPerMinute.isPresent())
	{
		m_UI->leManualMeasuresPerMinute->setText(QLocale::system().toString(m_TagManual.m_MeasuresPerMinute.value()));
	}
	else
	{
		m_UI->leManualMeasuresPerMinute->clear();
	}
	m_UI->pteNotes->setPlainText(m_Notes.valueOrDefault());
	m_IsInternalChange = false;
	fillDuplicates();
	selectSong(*m_Song);
	m_Components.get<SongTempoDetector>()->queueDetect(m_Song->sharedData());
}





DlgSongProperties::~DlgSongProperties()
{
	Settings::saveWindowPos("DlgSongProperties", *this);
}





void DlgSongProperties::fillDuplicates()
{
	int row = 0;
	for (const auto & song: m_Duplicates)
	{
		auto item = new QListWidgetItem(song->fileName());
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(song));
		m_UI->lwDuplicates->addItem(item);
		row += 1;
	}
}





void DlgSongProperties::selectSong(const Song & a_Song)
{
	// Select the song in tblDuplicates:
	m_IsInternalChange = true;
	m_Song = songPtrFromRef(a_Song);
	assert(m_Song != nullptr);
	auto numRows = m_UI->lwDuplicates->count();
	for (int row = 0; row < numRows; ++row)
	{
		auto itemSong = reinterpret_cast<const Song *>(m_UI->lwDuplicates->item(row)->data(Qt::UserRole).toULongLong());
		if (itemSong == &a_Song)
		{
			m_UI->lwDuplicates->setCurrentRow(row);
			break;
		}
	}

	// Fill in the ID3 tag, including user-made changes:
	const auto & orig = m_OriginalID3[&a_Song];
	if (orig.first)
	{
		auto itr = m_TagID3Changes.find(&a_Song);
		auto tag = orig.second;
		if (itr != m_TagID3Changes.end())
		{
			// The user made some changes, apply them into the tag:
			tag = MetadataScanner::applyTagChanges(tag, itr->second);
		}
		m_UI->leId3Author->setText(tag.m_Author.valueOrDefault());
		m_UI->leId3Title->setText(tag.m_Title.valueOrDefault());
		m_UI->leId3Comment->setText(tag.m_Comment.valueOrDefault());
		m_UI->leId3Genre->setText(tag.m_Genre.valueOrDefault());
		if (tag.m_MeasuresPerMinute.isPresent())
		{
			const auto loc = QLocale::system();
			m_UI->leId3MeasuresPerMinute->setText(loc.toString(tag.m_MeasuresPerMinute.value()));
		}
		else
		{
			m_UI->leId3MeasuresPerMinute->clear();
		}
		// TODO: Indicate changed fields by using a different bg color or font
	}
	else
	{
		m_UI->leId3Author->clear();
		m_UI->leId3Title->clear();
		m_UI->leId3Comment->clear();
		m_UI->leId3Genre->clear();
		m_UI->leId3MeasuresPerMinute->clear();
	}
	m_UI->leId3Author->setReadOnly(!orig.first);
	m_UI->leId3Title->setReadOnly(!orig.first);
	m_UI->leId3Comment->setReadOnly(!orig.first);
	m_UI->leId3Genre->setReadOnly(!orig.first);
	m_UI->leId3MeasuresPerMinute->setReadOnly(!orig.first);
	auto pal = orig.first ? m_UI->leManualAuthor->palette() : m_UI->leFilenameAuthor->palette();
	m_UI->leId3Author->setPalette(pal);
	m_UI->leId3Title->setPalette(pal);
	m_UI->leId3Comment->setPalette(pal);
	m_UI->leId3Genre->setPalette(pal);
	m_UI->leId3MeasuresPerMinute->setPalette(pal);

	// Fill in the parsed ID3 values, updated with the user-made changes:
	updateParsedId3();

	// Fill in the filename-deduced values:
	m_UI->leFilenameAuthor->setText(a_Song.tagFileName().m_Author.valueOrDefault());
	m_UI->leFilenameTitle->setText(a_Song.tagFileName().m_Title.valueOrDefault());
	m_UI->leFilenameGenre->setText(a_Song.tagFileName().m_Genre.valueOrDefault());
	if (a_Song.tagFileName().m_MeasuresPerMinute.isPresent())
	{
		const auto loc = QLocale::system();
		m_UI->leFilenameMeasuresPerMinute->setText(loc.toString(a_Song.tagFileName().m_MeasuresPerMinute.value()));
	}
	else
	{
		m_UI->leFilenameMeasuresPerMinute->clear();
	}
	m_IsInternalChange = false;
}





SongPtr DlgSongProperties::songPtrFromRef(const Song & a_Song)
{
	for (const auto & song: m_Duplicates)
	{
		if (song == &a_Song)
		{
			return song->shared_from_this();
		}
	}
	return nullptr;
}





void DlgSongProperties::updateParsedId3()
{
	// If the ID3 tag is invalid, bail out:
	const auto & orig = m_OriginalID3[m_Song.get()];
	if (!orig.first)
	{
		m_UI->lePid3Author->clear();
		m_UI->lePid3Title->clear();
		m_UI->lePid3Genre->clear();
		m_UI->lePid3MeasuresPerMinute->clear();
		return;
	}

	// Create a merged tag value from the original and user-edits:
	MetadataScanner::Tag toDisplay(orig.second);
	auto itr = m_TagID3Changes.find(m_Song.get());
	if (itr != m_TagID3Changes.end())
	{
		toDisplay = MetadataScanner::applyTagChanges(toDisplay, itr->second);
	}

	// Fill in the parsed values:
	const auto parsedId3 = MetadataScanner::parseId3Tag(toDisplay);
	m_UI->lePid3Author->setText(parsedId3.m_Author.valueOrDefault());
	m_UI->lePid3Title->setText(parsedId3.m_Title.valueOrDefault());
	m_UI->lePid3Genre->setText(parsedId3.m_Genre.valueOrDefault());
	if (parsedId3.m_MeasuresPerMinute.isPresent())
	{
		const auto loc = QLocale::system();
		m_UI->lePid3MeasuresPerMinute->setText(loc.toString(parsedId3.m_MeasuresPerMinute.value()));
	}
	else
	{
		m_UI->lePid3MeasuresPerMinute->clear();
	}
}





void DlgSongProperties::applyAndClose()
{
	// Save the shared data:
	m_Song->sharedData()->m_TagManual = m_TagManual;
	m_Song->sharedData()->m_Notes = m_Notes;
	auto db = m_Components.get<Database>();
	db->saveSongSharedData(m_Song->sharedData());

	// Apply the tag changes:
	for (auto song: m_Duplicates)
	{
		const auto itr = m_TagID3Changes.find(song);
		if (itr != m_TagID3Changes.cend())
		{
			assert(m_OriginalID3[song].first);  // The original tag must be valid
			auto tag = MetadataScanner::applyTagChanges(m_OriginalID3[song].second, itr->second);

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





void DlgSongProperties::authorTextEdited(const QString & a_NewText)
{
	if (!m_IsInternalChange)
	{
		m_TagManual.m_Author = a_NewText;
	}
}





void DlgSongProperties::titleTextEdited(const QString & a_NewText)
{
	if (!m_IsInternalChange)
	{
		m_TagManual.m_Title = a_NewText;
	}
}





void DlgSongProperties::genreSelected(const QString & a_NewGenre)
{
	if (!m_IsInternalChange)
	{
		m_TagManual.m_Genre = a_NewGenre;
	}
}





void DlgSongProperties::measuresPerMinuteTextEdited(const QString & a_NewText)
{
	if (m_IsInternalChange)
	{
		return;
	}
	if (a_NewText.isEmpty())
	{
		m_UI->leManualMeasuresPerMinute->setStyleSheet("");
		m_TagManual.m_MeasuresPerMinute.reset();
		return;
	}
	bool isOK;
	auto mpm = QLocale::system().toDouble(a_NewText, &isOK);
	if (isOK)
	{
		m_UI->leManualMeasuresPerMinute->setStyleSheet("");
		m_TagManual.m_MeasuresPerMinute = mpm;
	}
	else
	{
		m_UI->leManualMeasuresPerMinute->setStyleSheet("background-color:#fcc");
	}
}





void DlgSongProperties::notesChanged()
{
	m_Notes = m_UI->pteNotes->toPlainText();
}





void DlgSongProperties::switchDuplicate(int a_Row)
{
	if ((a_Row < 0) || (a_Row >= static_cast<int>(m_Duplicates.size())))
	{
		qWarning() << "Invalid row: " << a_Row << " out of " << m_Duplicates.size();
		return;
	}
	auto song = m_Duplicates[static_cast<size_t>(a_Row)];
	selectSong(*song);
}





void DlgSongProperties::id3AuthorEdited(const QString & a_NewText)
{
	if (!m_IsInternalChange)
	{
		m_TagID3Changes[m_Song.get()].m_Author = a_NewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3TitleEdited(const QString & a_NewText)
{
	if (!m_IsInternalChange)
	{
		m_TagID3Changes[m_Song.get()].m_Title = a_NewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3GenreEdited(const QString & a_NewText)
{
	if (!m_IsInternalChange)
	{
		m_TagID3Changes[m_Song.get()].m_Genre = a_NewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3CommentEdited(const QString & a_NewText)
{
	if (!m_IsInternalChange)
	{
		m_TagID3Changes[m_Song.get()].m_Comment = a_NewText;
		updateParsedId3();
	}
}





void DlgSongProperties::id3MeasuresPerMinuteEdited(const QString & a_NewText)
{
	if (m_IsInternalChange)
	{
		return;
	}
	if (a_NewText.isEmpty())
	{
		m_UI->leId3MeasuresPerMinute->setStyleSheet("");
		m_TagID3Changes[m_Song.get()].m_MeasuresPerMinute.reset();
		updateParsedId3();
		return;
	}
	bool isOK;
	auto mpm = QLocale::system().toDouble(a_NewText, &isOK);
	if (isOK)
	{
		m_UI->leId3MeasuresPerMinute->setStyleSheet("");
		m_TagID3Changes[m_Song.get()].m_MeasuresPerMinute = mpm;
		updateParsedId3();
	}
	else
	{
		m_UI->leId3MeasuresPerMinute->setStyleSheet("background-color:#fcc");
	}
}





void DlgSongProperties::removeFromLibrary()
{
	auto row = m_UI->lwDuplicates->currentRow();
	if ((row < 0) || (row >= static_cast<int>(m_Duplicates.size())))
	{
		qWarning() << "Invalid row: " << row;
		return;
	}
	auto song = m_Duplicates[static_cast<size_t>(row)]->shared_from_this();
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
	m_Components.get<Database>()->removeSong(*song, false);
	m_Duplicates.erase(m_Duplicates.begin() + row);
	delete m_UI->lwDuplicates->takeItem(row);
}





void DlgSongProperties::deleteFromDisk()
{
	auto row = m_UI->lwDuplicates->currentRow();
	if ((row < 0) || (row >= static_cast<int>(m_Duplicates.size())))
	{
		qWarning() << "Invalid row: " << row;
		return;
	}
	auto song = m_Duplicates[static_cast<size_t>(row)]->shared_from_this();
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
	m_Components.get<Database>()->removeSong(*song, true);
	m_Duplicates.erase(m_Duplicates.begin() + row);
	delete m_UI->lwDuplicates->takeItem(row);
}





void DlgSongProperties::copyId3Tag()
{
	QGuiApplication::clipboard()->setText(
		QString::fromUtf8("{\"%1\", \"%2\", \"%3\", \"%4\", %5}")
		.arg(m_UI->leId3Author->text())
		.arg(m_UI->leId3Title->text())
		.arg(m_UI->leId3Comment->text())
		.arg(m_UI->leId3Genre->text())
		.arg(bpmToCopy(m_UI->leId3MeasuresPerMinute->text()))
	);
}





void DlgSongProperties::copyPid3Tag()
{
	QGuiApplication::clipboard()->setText(
		QString::fromUtf8("{\"%1\", \"%2\", \"%3\", %4}")
		.arg(m_UI->lePid3Author->text())
		.arg(m_UI->lePid3Title->text())
		.arg(m_UI->lePid3Genre->text())
		.arg(bpmToCopy(m_UI->lePid3MeasuresPerMinute->text()))
	);
}





void DlgSongProperties::copyFilenameTag()
{
	QGuiApplication::clipboard()->setText(
		QString::fromUtf8("{\"%1\", \"%2\", \"%3\", %4}")
		.arg(m_UI->leFilenameAuthor->text())
		.arg(m_UI->leFilenameTitle->text())
		.arg(m_UI->leFilenameGenre->text())
		.arg(bpmToCopy(m_UI->leFilenameMeasuresPerMinute->text()))
	);
}





void DlgSongProperties::showTapTempo()
{
	DlgTapTempo dlg(m_Components, m_Song, this);
	if (dlg.exec() == QDialog::Rejected)
	{
		return;
	}
	m_TagManual.m_MeasuresPerMinute = m_Song->tagManual().m_MeasuresPerMinute;  // Reload from the saved value
	if (m_TagManual.m_MeasuresPerMinute.isPresent())
	{
		m_UI->leManualMeasuresPerMinute->setText(QLocale::system().toString(m_TagManual.m_MeasuresPerMinute.value()));
	}
	else
	{
		m_UI->leManualMeasuresPerMinute->clear();
	}
}





void DlgSongProperties::songTempoDetected(Song::SharedDataPtr a_SongSD)
{
	if (a_SongSD != m_Song->sharedData())
	{
		return;
	}

	QString detectedMpm;
	if (m_Song->sharedData()->m_DetectedTempo.isPresent())
	{
		const auto loc = QLocale::system();
		detectedMpm = loc.toString(m_Song->sharedData()->m_DetectedTempo.value());
	}
	else
	{
		detectedMpm = tr("unknown (detection failed)");
	}
	m_UI->leDetectedMeasuresPerMinute->setText(detectedMpm);
}

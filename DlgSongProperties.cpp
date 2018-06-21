#include "DlgSongProperties.h"
#include <assert.h>
#include <QDebug>
#include <QMessageBox>
#include "ui_DlgSongProperties.h"
#include "Database.h"
#include "Settings.h"





DlgSongProperties::DlgSongProperties(
	Database & a_DB,
	SongPtr a_Song,
	QWidget * a_Parent
) :
	Super(a_Parent),
	m_UI(new Ui::DlgSongProperties),
	m_DB(a_DB),
	m_Song(a_Song),
	m_Duplicates(a_Song->duplicates())
{
	// Initialize the ChangeSets:
	m_TagManual = m_Song->tagManual();
	m_Notes = m_Song->notes();
	for (const auto & song: m_Duplicates)
	{
		m_TagID3Changes[song] = song->tagId3();
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
	connect(m_UI->btnCancel,                 &QPushButton::clicked,           this, &DlgSongProperties::reject);
	connect(m_UI->btnOK,                     &QPushButton::clicked,           this, &DlgSongProperties::applyAndClose);
	connect(m_UI->leManualAuthor,            &QLineEdit::textEdited,          this, &DlgSongProperties::authorTextEdited);
	connect(m_UI->leManualTitle,             &QLineEdit::textEdited,          this, &DlgSongProperties::titleTextEdited);
	connect(m_UI->cbManualGenre,             &QComboBox::currentTextChanged,  this, &DlgSongProperties::genreSelected);
	connect(m_UI->leManualMeasuresPerMinute, &QLineEdit::textEdited,          this, &DlgSongProperties::measuresPerMinuteTextEdited);
	connect(m_UI->pteNotes,                  &QPlainTextEdit::textChanged,    this, &DlgSongProperties::notesChanged);
	connect(m_UI->lwDuplicates,              &QListWidget::currentRowChanged, this, &DlgSongProperties::switchDuplicate);
	connect(m_UI->actRemoveFromLibrary,      &QAction::triggered,             this, &DlgSongProperties::removeFromLibrary);
	connect(m_UI->actDeleteFromDisk,         &QAction::triggered,             this, &DlgSongProperties::deleteFromDisk);

	// Set the read-only edit boxes' palette to greyed-out:
	auto p = palette();
	p.setColor(QPalette::Active,   QPalette::Base, p.color(QPalette::Disabled, QPalette::Base));
	p.setColor(QPalette::Inactive, QPalette::Base, p.color(QPalette::Disabled, QPalette::Base));
	m_UI->leHash->setPalette(p);
	m_UI->leLength->setPalette(p);
	m_UI->leId3Author->setPalette(p);
	m_UI->leId3Title->setPalette(p);
	m_UI->leId3Genre->setPalette(p);
	m_UI->leId3MeasuresPerMinute->setPalette(p);
	m_UI->leFilenameAuthor->setPalette(p);
	m_UI->leFilenameTitle->setPalette(p);
	m_UI->leFilenameGenre->setPalette(p);
	m_UI->leFilenameMeasuresPerMinute->setPalette(p);

	// Fill in the data:
	m_IsInternalChange = true;
	// TODO: m_UI->leHash->setText(hexString(a_Song.hash().toByteArray()));
	auto length = m_Song->length();
	if (length.isValid())
	{
		auto numSeconds = static_cast<int>(length.toDouble() + 0.5);
		m_UI->leLength->setText(tr("%1:%2 (%n seconds)", "SongLength", numSeconds)
			.arg(QString::number(numSeconds / 60))
			.arg(QString::number(numSeconds % 60), 2, '0')
		);
	}
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

	const auto loc = QLocale::system();
	m_UI->leId3Author->setText(a_Song.tagId3().m_Author.valueOrDefault());
	m_UI->leId3Title->setText(a_Song.tagId3().m_Title.valueOrDefault());
	m_UI->leId3Genre->setText(a_Song.tagId3().m_Genre.valueOrDefault());
	if (a_Song.tagId3().m_MeasuresPerMinute.isPresent())
	{
		m_UI->leId3MeasuresPerMinute->setText(loc.toString(a_Song.tagId3().m_MeasuresPerMinute.value()));
	}
	else
	{
		m_UI->leId3MeasuresPerMinute->clear();
	}
	m_UI->leFilenameAuthor->setText(a_Song.tagFileName().m_Author.valueOrDefault());
	m_UI->leFilenameTitle->setText(a_Song.tagFileName().m_Title.valueOrDefault());
	m_UI->leFilenameGenre->setText(a_Song.tagFileName().m_Genre.valueOrDefault());
	if (a_Song.tagFileName().m_MeasuresPerMinute.isPresent())
	{
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





void DlgSongProperties::applyAndClose()
{
	m_Song->sharedData()->m_TagManual = m_TagManual;
	m_Song->sharedData()->m_Notes = m_Notes;
	m_DB.saveSongSharedData(m_Song->sharedData());
	for (const auto & song: m_Duplicates)
	{
		const auto & cs = m_TagID3Changes[song];
		// TODO: Apply the tag changes (#128)
		Q_UNUSED(cs);
		// m_DB.saveSong(song);
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
	m_DB.removeSong(*song, false);
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
	m_DB.removeSong(*song, true);
	m_Duplicates.erase(m_Duplicates.begin() + row);
	delete m_UI->lwDuplicates->takeItem(row);
}

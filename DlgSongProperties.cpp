#include "DlgSongProperties.h"
#include <assert.h>
#include "ui_DlgSongProperties.h"





DlgSongProperties::DlgSongProperties(
	SongPtr a_Song,
	QWidget * a_Parent
) :
	Super(a_Parent),
	m_UI(new Ui::DlgSongProperties),
	m_Song(a_Song),
	m_Duplicates(a_Song->duplicates())
{
	// Initialize the ChangeSets:
	for (const auto & song: m_Duplicates)
	{
		auto & cs = m_ChangeSets[song.get()];
		cs.m_ManualAuthor = song->tagManual().m_Author;
		cs.m_ManualTitle  = song->tagManual().m_Title;
		cs.m_ManualGenre  = song->tagManual().m_Genre;
		cs.m_ManualMeasuresPerMinute = song->tagManual().m_MeasuresPerMinute;
		cs.m_Notes = song->notes();
	}

	// Initialize the UI:
	m_UI->setupUi(this);
	m_UI->tblDuplicates->setColumnCount(1);
	auto genres = Song::recognizedGenres();
	genres.insert(0, "");
	m_UI->cbManualGenre->addItems(genres);
	m_UI->cbManualGenre->setMaxVisibleItems(genres.count());
	fillDuplicates();
	selectSong(*m_Song);
}





DlgSongProperties::~DlgSongProperties()
{
	// Nothing explicit needed yet
}





void DlgSongProperties::fillDuplicates()
{
	m_UI->tblDuplicates->setRowCount(static_cast<int>(m_Duplicates.size()));
	int row = 0;
	for (const auto & song: m_Duplicates)
	{
		auto item = new QTableWidgetItem(song->fileName());
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(song.get()));
		m_UI->tblDuplicates->setItem(row, 0, item);
		row += 1;
	}
	m_UI->tblDuplicates->resizeColumnsToContents();
}





void DlgSongProperties::selectSong(const Song & a_Song)
{
	// Select the song in tblDuplicates:
	m_IsInternalChange = true;
	m_Song = songPtrFromRef(a_Song);
	assert(m_Song != nullptr);
	auto numRows = m_UI->tblDuplicates->rowCount();
	for (int row = 0; row < numRows; ++row)
	{
		auto itemSong = reinterpret_cast<const Song *>(m_UI->tblDuplicates->item(row, 0)->data(Qt::UserRole).toULongLong());
		if (itemSong == &a_Song)
		{
			m_UI->tblDuplicates->selectRow(row);
			break;
		}
	}

	m_UI->leFileName->setText(a_Song.fileName());
	// TODO: m_UI->leHash->setText(hexString(a_Song.hash().toByteArray()));
	auto length = a_Song.length();
	if (length.isValid())
	{
		auto numSeconds = static_cast<int>(length.toDouble() + 0.5);
		m_UI->leLength->setText(tr("%1:%2 (%n seconds)", "SongLength", numSeconds)
			.arg(QString::number(numSeconds / 60))
			.arg(QString::number(numSeconds % 60), 2, '0')
		);
	}
	const auto loc = QLocale::system();
	const auto & cs = m_ChangeSets[&a_Song];
	m_UI->leManualAuthor->setText(cs.m_ManualAuthor.toString());
	m_UI->leManualTitle->setText(cs.m_ManualTitle.toString());
	m_UI->cbManualGenre->setCurrentText(cs.m_ManualGenre.toString());
	if (cs.m_ManualMeasuresPerMinute.isValid())
	{
		m_UI->leManualMeasuresPerMinute->setText(loc.toString(cs.m_ManualMeasuresPerMinute.toDouble()));
	}
	m_UI->leId3Author->setText(a_Song.tagId3().m_Author.toString());
	m_UI->leId3Title->setText(a_Song.tagId3().m_Title.toString());
	m_UI->leId3Genre->setText(a_Song.tagId3().m_Genre.toString());
	if (a_Song.tagId3().m_MeasuresPerMinute.isValid())
	{
		m_UI->leId3MeasuresPerMinute->setText(loc.toString(a_Song.tagId3().m_MeasuresPerMinute.toDouble()));
	}
	m_UI->leFilenameAuthor->setText(a_Song.tagFileName().m_Author.toString());
	m_UI->leFilenameTitle->setText(a_Song.tagFileName().m_Title.toString());
	m_UI->leFilenameGenre->setText(a_Song.tagFileName().m_Genre.toString());
	if (a_Song.tagFileName().m_MeasuresPerMinute.isValid())
	{
		m_UI->leFilenameMeasuresPerMinute->setText(loc.toString(a_Song.tagFileName().m_MeasuresPerMinute.toDouble()));
	}
	m_UI->pteNotes->setPlainText(cs.m_Notes.toString());
	m_IsInternalChange = false;
}





SongPtr DlgSongProperties::songPtrFromRef(const Song & a_Song)
{
	for (const auto & song: m_Duplicates)
	{
		if (song.get() == &a_Song)
		{
			return song;
		}
	}
	return nullptr;
}

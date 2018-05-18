#include "DlgSongs.h"
#include <QFileDialog>
#include <QDebug>
#include <QProcessEnvironment>
#include <QMessageBox>
#include <QComboBox>
#include "ui_DlgSongs.h"
#include "Database.h"
#include "MetadataScanner.h"
#include "AVPP.h"
#include "Stopwatch.h"
#include "HashCalculator.h"





/** The number of ticks (periodic UI updates) to wait between the user changing the search text
and applying it to the filter. This is to avoid slowdowns while typing the search text. */
static const int TICKS_UNTIL_SET_SEARCH_TEXT = 3;





////////////////////////////////////////////////////////////////////////////////
// DlgSongs:

DlgSongs::DlgSongs(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	HashCalculator & a_Hasher,
	std::unique_ptr<QSortFilterProxyModel> && a_FilterModel,
	bool a_ShowManipulators,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_HashCalculator(a_Hasher),
	m_UI(new Ui::DlgSongs),
	m_FilterModel(std::move(a_FilterModel)),
	m_SongModel(a_DB),
	m_SongModelFilter(m_SongModel),
	m_IsLibraryRescanShown(true),
	m_LastLibraryRescanTotal(0),
	m_LastLibraryRescanQueue(-1),
	m_TicksUntilSetSearchText(0)
{
	m_UI->setupUi(this);
	if (m_FilterModel == nullptr)
	{
		m_FilterModel.reset(new QSortFilterProxyModel);
	}
	if (!a_ShowManipulators)
	{
		m_UI->btnAddFolder->hide();
		m_UI->btnRemove->hide();
		m_UI->btnAddToPlaylist->hide();
	}
	m_FilterModel->setSourceModel(&m_SongModelFilter);
	m_UI->tblSongs->setModel(m_FilterModel.get());
	m_UI->tblSongs->setItemDelegate(new SongModelEditorDelegate(this));

	// Connect the signals:
	connect(m_UI->btnAddFile,        &QPushButton::clicked,    this, &DlgSongs::chooseAddFile);
	connect(m_UI->btnAddFolder,      &QPushButton::clicked,    this, &DlgSongs::chooseAddFolder);
	connect(m_UI->btnRemove,         &QPushButton::clicked,    this, &DlgSongs::removeSelected);
	connect(m_UI->btnClose,          &QPushButton::clicked,    this, &DlgSongs::close);
	connect(m_UI->btnAddToPlaylist,  &QPushButton::clicked,    this, &DlgSongs::addSelectedToPlaylist);
	connect(m_UI->btnRescanMetadata, &QPushButton::clicked,    this, &DlgSongs::rescanMetadata);
	connect(&m_SongModel,            &SongModel::songEdited,   this, &DlgSongs::modelSongEdited);
	connect(&m_SongModel,            &SongModel::rowsInserted, this, &DlgSongs::updateSongStats);
	connect(&m_DB,                   &Database::songFileAdded, this, &DlgSongs::updateSongStats);
	connect(&m_DB,                   &Database::songRemoved,   this, &DlgSongs::updateSongStats);
	connect(&m_PeriodicUiUpdate,     &QTimer::timeout,         this, &DlgSongs::periodicUiUpdate);

	initFilterSearch();

	// Resize the table columns to fit the song data:
	#if 0
	{
		STOPWATCH("Resize DlgSongs columns");
		m_UI->tblSongs->resizeColumnsToContents();
	}
	#endif

	// Make the dialog have Maximize button on Windows:
	setWindowFlags(Qt::Window);

	updateSongStats();

	m_PeriodicUiUpdate.start(100);
}





DlgSongs::~DlgSongs()
{
	// Nothing explicit needed, but the destructor needs to be defined in the CPP file due to m_UI.
}





void DlgSongs::addFiles(const QStringList & a_FileNames)
{
	// Duplicates are skippen inside m_DB, no need to handle them here
	std::vector<std::pair<QString, qulonglong>> songs;
	for (const auto & fnam: a_FileNames)
	{
		QFileInfo fi(fnam);
		if (!fi.exists())
		{
			continue;
		}
		if (!AVPP::isExtensionSupported(fi.suffix()))
		{
			continue;
		}
		songs.emplace_back(fnam, static_cast<qulonglong>(fi.size()));
	}
	if (songs.empty())
	{
		return;
	}
	qDebug() << ": Adding " << songs.size() << " song files";
	m_DB.addSongFiles(songs);
}





void DlgSongs::addFolder(const QString & a_Path)
{
	QDir dir(a_Path + "/");
	std::vector<std::pair<QString, qulonglong>> songs;
	for (const auto & item: dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
	{
		if (item.isDir())
		{
			addFolder(item.absoluteFilePath());
			continue;
		}
		if (!item.isFile())
		{
			continue;
		}
		if (!AVPP::isExtensionSupported(item.suffix()))
		{
			continue;
		}
		songs.emplace_back(item.absoluteFilePath(), static_cast<qulonglong>(item.size()));
	}
	if (songs.empty())
	{
		return;
	}
	qDebug() << ": Adding " << songs.size() << " songs from folder " << a_Path;
	m_DB.addSongFiles(songs);
}





void DlgSongs::updateSongStats()
{
	auto numFiltered = static_cast<size_t>(m_FilterModel->rowCount());
	auto numTotal = m_DB.songs().size();
	if (numFiltered == numTotal)
	{
		m_UI->lblStats->setText(tr("Total songs: %1").arg(numTotal));
	}
	else
	{
		m_UI->lblStats->setText(tr("Total songs: %1 (filtered out of %2)").arg(numFiltered).arg(numTotal));
	}
}





void DlgSongs::initFilterSearch()
{
	m_UI->cbFilter->addItem(tr("All songs"),                            SongModelFilter::fltNone);
	m_UI->cbFilter->addItem(tr("Songs without ID3 tag"),                SongModelFilter::fltNoId3);
	m_UI->cbFilter->addItem(tr("Songs with no genre"),                  SongModelFilter::fltNoGenre);
	m_UI->cbFilter->addItem(tr("Songs with no tempo"),                  SongModelFilter::fltNoMeasuresPerMinute);
	m_UI->cbFilter->addItem(tr("Songs with warnings"),                  SongModelFilter::fltWarnings);
	m_UI->cbFilter->addItem(tr("Songs not matching any template item"), SongModelFilter::fltNoTemplateFilterMatch);

	// Bind signals / slots:
	// Cannot bind overloaded signal via fn ptr in older Qt, need to use SIGNAL() / SLOT()
	connect(m_UI->cbFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChosen(int)));
	connect(m_UI->leSearch, &QLineEdit::textEdited,           this, &DlgSongs::searchTextEdited);

	m_SongModelFilter.setFavoriteTemplateItems(m_DB.getFavoriteTemplateItems());
}





SongPtr DlgSongs::songFromIndex(const QModelIndex & a_Index)
{
	return m_SongModel.songFromIndex(
		m_SongModelFilter.mapToSource(
			m_FilterModel->mapToSource(a_Index)
		)
	);
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
	addFolder(dir);
}





void DlgSongs::removeSelected()
{
	// Collect the songs to remove:
	std::vector<SongPtr> songs;
	foreach(const auto & idx, m_UI->tblSongs->selectionModel()->selectedRows())
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
		tr("Are you sure you want to remove the selected songs? This operation cannot be undone!"),
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Remove from the DB:
	for (const auto & song: songs)
	{
		m_DB.delSong(*song);
	}
}





void DlgSongs::addSelectedToPlaylist()
{
	foreach(const auto & idx, m_UI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		emit addSongToPlaylist(song);
	}
}





void DlgSongs::rescanMetadata()
{
	foreach(const auto & idx, m_UI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		if (song->hash().isValid())
		{
			m_MetadataScanner.queueScanSong(song);
		}
	}
}





void DlgSongs::modelSongEdited(SongPtr a_Song)
{
	m_DB.saveSong(a_Song);
}





void DlgSongs::periodicUiUpdate()
{
	// Update the LibraryRescan UI:
	auto queueLength = m_HashCalculator.queueLength() * 2 + m_MetadataScanner.queueLength();
	if (m_LastLibraryRescanQueue != queueLength)
	{
		m_LastLibraryRescanQueue = queueLength;
		if (queueLength == 0)
		{
			if (m_IsLibraryRescanShown)
			{
				m_UI->wLibraryRescan->hide();
				m_IsLibraryRescanShown = false;
			}
		}
		else
		{
			auto numSongs = static_cast<int>(m_DB.songs().size() * 2);
			if (numSongs != m_LastLibraryRescanTotal)
			{
				m_UI->pbLibraryRescan->setMaximum(numSongs);
				m_LastLibraryRescanTotal = numSongs;
			}
			m_UI->pbLibraryRescan->setValue(std::max(numSongs - queueLength, 0));
			m_UI->pbLibraryRescan->update();  // For some reason setting the value is not enough to redraw
			if (!m_IsLibraryRescanShown)
			{
				m_UI->wLibraryRescan->show();
				m_IsLibraryRescanShown = true;
			}
		}
	}

	if (m_TicksUntilSetSearchText > 0)
	{
		m_TicksUntilSetSearchText -= 1;
		if (m_TicksUntilSetSearchText == 0)
		{
			m_SongModelFilter.setSearchString(m_NewSearchText);
		}
	}
}





void DlgSongs::filterChosen(int a_Index)
{
	auto filter = static_cast<SongModelFilter::EFilter>(m_UI->cbFilter->itemData(a_Index).toInt());
	m_SongModelFilter.setFilter(filter);
}





void DlgSongs::searchTextEdited(const QString & a_NewText)
{
	m_NewSearchText = a_NewText;
	m_TicksUntilSetSearchText = TICKS_UNTIL_SET_SEARCH_TEXT;
}

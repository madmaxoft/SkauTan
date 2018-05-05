#include "DlgSongs.h"
#include <QFileDialog>
#include <QDebug>
#include <QProcessEnvironment>
#include <QMessageBox>
#include "ui_DlgSongs.h"
#include "Database.h"
#include "MetadataScanner.h"
#include "AVPP.h"





////////////////////////////////////////////////////////////////////////////////
// DlgSongs:

DlgSongs::DlgSongs(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	std::unique_ptr<QSortFilterProxyModel> && a_FilterModel,
	bool a_ShowManipulators,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_UI(new Ui::DlgSongs),
	m_FilterModel(std::move(a_FilterModel)),
	m_SongModel(a_DB)
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
	m_FilterModel->setSourceModel(&m_SongModel);
	m_UI->tblSongs->setModel(m_FilterModel.get());
	m_UI->tblSongs->setItemDelegate(new SongModelEditorDelegate(this));

	// Connect the signals:
	connect(m_UI->btnAddFile,        &QPushButton::clicked,  this, &DlgSongs::chooseAddFile);
	connect(m_UI->btnAddFolder,      &QPushButton::clicked,  this, &DlgSongs::chooseAddFolder);
	connect(m_UI->btnRemove,         &QPushButton::clicked,  this, &DlgSongs::removeSelected);
	connect(m_UI->btnClose,          &QPushButton::clicked,  this, &DlgSongs::close);
	connect(m_UI->btnAddToPlaylist,  &QPushButton::clicked,  this, &DlgSongs::addSelectedToPlaylist);
	connect(m_UI->btnRescanMetadata, &QPushButton::clicked,  this, &DlgSongs::rescanMetadata);
	connect(&m_SongModel,            &SongModel::songEdited, this, &DlgSongs::modelSongEdited);

	m_UI->tblSongs->resizeColumnsToContents();

	// Make the dialog have Maximize button on Windows:
	setWindowFlags(Qt::Window);
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
	qDebug() << __FUNCTION__ << ": Adding " << songs.size() << " song files";
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
	qDebug() << __FUNCTION__ << ": Adding " << songs.size() << " songs from folder " << a_Path;
	m_DB.addSongFiles(songs);
}





void DlgSongs::chooseAddFile()
{
	auto files = QFileDialog::getOpenFileNames(
		this,
		tr("SkauTan - Choose files to add"),
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
		tr("SkauTan - Choose folder to add"),
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
		songs.push_back(m_SongModel.songFromIndex(idx));
	}
	if (songs.empty())
	{
		return;
	}

	// Ask for confirmation:
	if (QMessageBox::question(
		this,
		tr("SkauTan - Remove songs?"),
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
		auto song = m_SongModel.songFromIndex(idx);
		emit addSongToPlaylist(song);
	}
}





void DlgSongs::rescanMetadata()
{
	foreach(const auto & idx, m_UI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = m_SongModel.songFromIndex(idx);
		m_MetadataScanner.queueScanSong(song);
	}
}





void DlgSongs::modelSongEdited(SongPtr a_Song)
{
	m_DB.saveSong(a_Song);
}

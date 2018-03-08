#include "DlgSongs.h"
#include <QFileDialog>
#include <QDebug>
#include <QProcessEnvironment>
#include "ui_DlgSongs.h"
#include "Database.h"





////////////////////////////////////////////////////////////////////////////////
// DlgSongs:

DlgSongs::DlgSongs(
	Database & a_DB,
	std::unique_ptr<QSortFilterProxyModel> && a_FilterModel,
	bool a_ShowManipulators,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_DB(a_DB),
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
		m_UI->btnAddToPlaylist->hide();
	}
	m_FilterModel->setSourceModel(&m_SongModel);
	m_UI->tblSongs->setModel(m_FilterModel.get());
	m_UI->tblSongs->setItemDelegate(new SongModelEditorDelegate(this));

	// Connect the signals:
	connect(m_UI->btnAddFolder,     &QPushButton::clicked,  this, &DlgSongs::chooseAddFolder);
	connect(m_UI->btnClose,         &QPushButton::clicked,  this, &DlgSongs::closeByButton);
	connect(m_UI->btnAddToPlaylist, &QPushButton::clicked,  this, &DlgSongs::addSelectedToPlaylist);
	connect(&m_SongModel,           &SongModel::songEdited, this, &DlgSongs::modelSongEdited);

	#if 0
	// Set the column widths to reasonable defaults:
	QFontMetrics fm(m_UI->tblSongs->horizontalHeader()->font());
	m_UI->tblSongs->setColumnWidth(SongModel::colRating,            fm.width("WRatingW"));
	m_UI->tblSongs->setColumnWidth(SongModel::colGenre,             fm.width("WGenreW"));
	m_UI->tblSongs->setColumnWidth(SongModel::colLength,            fm.width("W000:00:00W"));
	m_UI->tblSongs->setColumnWidth(SongModel::colMeasuresPerMinute, fm.width("WMPMW"));
	auto defaultWid = m_UI->tblSongs->columnWidth(SongModel::colFileName);
	m_UI->tblSongs->setColumnWidth(SongModel::colAuthor,   defaultWid * 2);
	m_UI->tblSongs->setColumnWidth(SongModel::colTitle,    defaultWid * 2);
	m_UI->tblSongs->setColumnWidth(SongModel::colFileName, defaultWid * 3);
	#endif
	m_UI->tblSongs->resizeColumnsToContents();

	// Make the dialog have Maximize button on Windows:
	setWindowFlags(Qt::Window);
}





DlgSongs::~DlgSongs()
{
	// Nothing explicit needed, but the destructor needs to be defined in the CPP file due to m_UI.
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
			qDebug() << __FUNCTION__ << ": Skipping object " << item.absoluteFilePath();
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





void DlgSongs::chooseAddFolder(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	auto dir = QFileDialog::getExistingDirectory(
		this,
		tr("Choose folder to add"),
		QProcessEnvironment::systemEnvironment().value("SKAUTAN_MUSIC_PATH", "")
	);
	if (dir.isEmpty())
	{
		return;
	}
	addFolder(dir);
}





void DlgSongs::closeByButton(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	close();
}





void DlgSongs::addSelectedToPlaylist(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	foreach(const auto & idx, m_UI->tblSongs->selectionModel()->selectedRows())
	{
		auto song = m_SongModel.songFromIndex(idx);
		emit addSongToPlaylist(song);
	}
}





void DlgSongs::modelSongEdited(Song * a_Song)
{
	m_DB.saveSong(*a_Song);
}

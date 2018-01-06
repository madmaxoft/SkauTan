#include "DlgSongs.h"
#include <QFileDialog>
#include <QDebug>
#include <QProcessEnvironment>
#include "ui_DlgSongs.h"
#include "SongDatabase.h"





////////////////////////////////////////////////////////////////////////////////
// DlgSongs:

DlgSongs::DlgSongs(SongDatabase & a_SongDB, QWidget * a_Parent):
	Super(a_Parent),
	m_SongDB(a_SongDB),
	m_UI(new Ui::DlgSongs),
	m_SongModel(a_SongDB)
{
	m_UI->setupUi(this);
	m_UI->tblSongs->setModel(&m_SongModel);

	// Connect the signals:
	connect(m_UI->btnAddFolder, &QPushButton::clicked, this, &DlgSongs::chooseAddFolder);
	connect(m_UI->btnClose,     &QPushButton::clicked, this, &DlgSongs::closeByButton);
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
	m_SongDB.addSongFiles(songs);
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





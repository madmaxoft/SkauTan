#include "PlayerWindow.h"
#include "assert.h"
#include <QDebug>
#include "ui_PlayerWindow.h"
#include "SongDatabase.h"
#include "DlgSongs.h"
#include "PlaylistItemSong.h"





PlayerWindow::PlayerWindow(QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::PlayerWindow),
	m_DB(new SongDatabase),
	m_Playlist(new Playlist)
{
	assert(m_Playlist != nullptr);
	m_PlaylistModel.reset(new PlaylistItemModel(m_Playlist));

	m_DB->open("SkauTan.sqlite");
	m_UI->setupUi(this);
	m_UI->tblPlaylist->setModel(m_PlaylistModel.get());

	// Connect the signals:
	connect(m_UI->btnSongs, &QPushButton::clicked, this, &PlayerWindow::showSongs);

	// Set up the header sections:
	QFontMetrics fm(m_UI->tblPlaylist->horizontalHeader()->font());
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colLength,            fm.width("W000:00:00W"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colGenre,             fm.width("WGenreW"));
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colMeasuresPerMinute, fm.width("WMPMW"));
	auto defaultWid = m_UI->tblPlaylist->columnWidth(PlaylistItemModel::colDisplayName);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colAuthor,      defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colTitle,       defaultWid * 2);
	m_UI->tblPlaylist->setColumnWidth(PlaylistItemModel::colDisplayName, defaultWid * 3);
}





PlayerWindow::~PlayerWindow()
{
	// Nothing explicit needed, but the destructor still needs to be defined in the CPP file due to m_UI
}





void PlayerWindow::showSongs(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgSongs dlg(*m_DB, this);
	connect(&dlg, &DlgSongs::addSongToPlaylist, this, &PlayerWindow::addSong);
	dlg.exec();
}





void PlayerWindow::addSong(std::shared_ptr<Song> a_Song)
{
	addPlaylistItem(std::make_shared<PlaylistItemSong>(a_Song));
}





void PlayerWindow::addPlaylistItem(std::shared_ptr<IPlaylistItem> a_Item)
{
	m_Playlist->addItem(a_Item);
}

#include "PlayerWindow.h"
#include "ui_PlayerWindow.h"
#include "SongDatabase.h"
#include "DlgSongs.h"





PlayerWindow::PlayerWindow(QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::PlayerWindow),
	m_DB(new SongDatabase)
{
	m_DB->open("SkauTan.sqlite");
	m_UI->setupUi(this);

	// Connect the signals:
	connect(m_UI->btnSongs, &QPushButton::clicked, this, &PlayerWindow::showSongs);
}





PlayerWindow::~PlayerWindow()
{
	// Nothing explicit needed, but the destructor still needs to be defined in the CPP file due to m_UI
}





void PlayerWindow::showSongs(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	DlgSongs dlg(*m_DB);
	dlg.exec();
}






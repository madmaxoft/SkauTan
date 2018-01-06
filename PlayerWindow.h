#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H





#include <QMainWindow>
#include <memory>





// fwd:
class SongDatabase;
namespace Ui
{
	class PlayerWindow;
}





/** Main window of the application.
Provides playlist,playback control and entrypoints to other dialogs. */
class PlayerWindow:
	public QMainWindow
{
	Q_OBJECT
	using Super = QMainWindow;


public:
	explicit PlayerWindow(QWidget * a_Parent = nullptr);
	~PlayerWindow();


private:

	std::unique_ptr<Ui::PlayerWindow> m_UI;

	std::unique_ptr<SongDatabase> m_DB;


private slots:

	/** Shows the Songs dialog.
	Signature must match QPushButton::clicked(). */
	void showSongs(bool a_IsChecked);
};





#endif // PLAYERWINDOW_H

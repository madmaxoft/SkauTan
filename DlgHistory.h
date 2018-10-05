#ifndef DLGHISTORY_H
#define DLGHISTORY_H





#include <memory>
#include <QDialog>
#include "Song.h"





// fwd:
class ComponentCollection;
class QMenu;
namespace Ui
{
	class DlgHistory;
}





class DlgHistory:\
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgHistory(ComponentCollection & a_Components, QWidget * a_Parent = nullptr);

	virtual ~DlgHistory() override;


signals:

	/** Emitted when the user wants to append a song to the playlist. */
	void appendSongToPlaylist(SongPtr a_Song);

	/** Emitted when the user wants to insert a song into the playlist after the current song. */
	void insertSongToPlaylist(SongPtr a_Song);


private:

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The Qt-managed UI.  */
	std::unique_ptr<Ui::DlgHistory> m_UI;

	/** The context menu for the history view. */
	std::unique_ptr<QMenu> m_ContextMenu;


	/** Returns the song represented in the specified table cell.
	Returns nullptr if no such song. */
	SongPtr songFromIndex(const QModelIndex & a_Index);


private slots:

	/** Shows the context menu for tblHistory. */
	void showHistoryContextMenu(const QPoint & a_Pos);

	/** Emits the appendToPlaylist() signal for each selected song. */
	void appendToPlaylist();

	/** Emits the insertIntoPlaylist() signal for each selected song. */
	void insertIntoPlaylist();

	/** Displays the song properties dialog for the currently highlighted song. */
	void showProperties();

	/** Displays file save dlg, then saves the (selected) history into the file. */
	void exportToFile();
};





#endif // DLGHISTORY_H

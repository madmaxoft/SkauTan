#ifndef DLGHISTORY_H
#define DLGHISTORY_H





#include <memory>
#include <QDialog>
#include <QTimer>
#include "../../Song.hpp"





// fwd:
class ComponentCollection;
class QMenu;
class QAbstractItemModel;
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

	/** The model adapter that performs the filtering.
	The actual class used is HistoryModelFilter, declared within the CPP file; it is a descendant
	of QAbstractItemModel so it can be up-cast when needed. */
	QAbstractItemModel * m_ModelFilter;

	/** The new search text to be set into m_SongModelFilter in periodic UI update.
	The text isn't set immediately to avoid slowdowns while still typing the string. */
	QString m_NewSearchText;

	/** Number of ticks until m_NewSearchText is set into the filter in the periodic UI update.
	The text isn't set immediately to avoid slowdowns while still typing the string,
	this counter goes from a fixed value down to zero on each periodic UI update and only when reaching zero
	is the search text applied. */
	int m_TicksUntilSetSearchText;

	/** Timer for updating the UI periodically with background-generated information. */
	QTimer m_PeriodicUiUpdate;


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

	/** Called periodically to update the UI. */
	void periodicUiUpdate();

	/** Called when the user edits the search text.
	Schedules an update in the m_FilterModel. */
	void searchTextEdited(const QString & a_NewText);
};





#endif // DLGHISTORY_H

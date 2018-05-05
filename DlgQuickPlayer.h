#ifndef DLGQUICKPLAYER_H
#define DLGQUICKPLAYER_H





#include <memory>
#include <QDialog>
#include "Template.h"





// fwd:
class Database;
class Player;
class QListWidgetItem;
class QTimer;
namespace Ui
{
	class DlgQuickPlayer;
}





class DlgQuickPlayer:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgQuickPlayer(Database & a_DB, Player & a_Player);

	virtual ~DlgQuickPlayer() override;


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgQuickPlayer> m_UI;

	/** The DB from which songs and template items are taken. */
	Database & m_DB;

	/** The player that is used for playback and playlist management. */
	Player & m_Player;

	/** Set to true if the position update is coming from the player internals (rather than user input). */
	bool m_IsInternalPositionUpdate;

	/** The timer that periodically updates the UI. */
	std::unique_ptr<QTimer> m_UpdateUITimer;


signals:

	/** Emitted when the user selects a template item to play.
	The receiver should enqueue the item in their playlist and start playing it. */
	void addAndPlayItem(Template::Item * a_Item);

	/** Emitted when the user clicks the Pause button.
	The receiver should pause the playback. */
	void pause();


protected slots:

	/** The user clicked an item in the list widget.
	Emits addAndPlayItem(), which enqueues and starts playing the item. */
	void playClickedItem(QListWidgetItem * a_Item);

	/** The user clicked the Pause button.
	Emits pause(), which the owner should bind to their "pause" action. */
	void pauseClicked();

	/** The playlist's current item has changed.
	Updates the track info in the UI. */
	void playlistCurrentItemChanged();

	/** Updates the playback position UI.
	Called regularly from a timer.
	Called even when playback not active! */
	void updateTimePos();
};





#endif // DLGQUICKPLAYER_H

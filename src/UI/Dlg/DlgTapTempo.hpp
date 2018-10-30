#ifndef DLGTAPTEMPO_H
#define DLGTAPTEMPO_H





#include <memory>
#include <QDialog>
#include <QElapsedTimer>
#include "../../Song.hpp"





// fwd:
class ComponentCollection;
class Player;
namespace Ui
{
	class DlgTapTempo;
}





class DlgTapTempo:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgTapTempo(
		ComponentCollection & a_Components,
		SongPtr a_Song,
		QWidget * a_Parent = nullptr
	);

	DlgTapTempo(const DlgTapTempo &) = delete;
	DlgTapTempo(DlgTapTempo &&) = delete;

	~DlgTapTempo();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgTapTempo> m_UI;

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The song to process. */
	SongPtr m_Song;

	/** The timer used for measuring delays between taps. */
	QElapsedTimer m_Timer;

	/** The player through which the song is being played. */
	std::unique_ptr<Player> m_Player;

	/** The intervals between user's taps. */
	std::vector<qint64> m_TimePoints;


	/** Clears the history of the user's taps.
	The next tap restarts the detection from scratch.
	Updates the UI to reflect no timepoints. */
	void clearTimePoints();

	/** Adds the timepoint as the next tap value.
	Updates the UI with history and current detection results. */
	void addTimePoint(qint64 a_MSecElapsed);

	/** Returns the MPM as calculated from m_TimePoints. */
	double detectMPM();

private slots:

	/** Emitted when the user taps the button.
	Stores the time since the last tap, updates the stats. */
	void buttonPressed();

	/** Saves the MPM to the song and closes the dialog. */
	void saveAndClose();
};





#endif // DLGTAPTEMPO_H

#ifndef DLGTEMPODETECT_H
#define DLGTEMPODETECT_H





#include <memory>
#include <vector>
#include <QDialog>
#include "../../Song.hpp"
#include "../../SongTempoDetector.hpp"





// fwd:
class QLabel;
class ComponentCollection;
namespace Ui
{
	class DlgTempoDetect;
}





class DlgTempoDetect:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgTempoDetect(ComponentCollection & a_Components, SongPtr a_Song, QWidget * a_Parent = nullptr);

	~DlgTempoDetect();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgTempoDetect> m_UI;

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The song being analyzed. */
	SongPtr m_Song;

	/** The history of detections on this song. */
	std::vector<std::shared_ptr<TempoDetector::Result>> m_History;

	/** Set to true while the code updates the UI, to avoid recalculations.
	If false, the UI changes come from the user. */
	bool m_IsInternalChange;

	/** The current delay, as used in delayedDetectTempo's timer.
	Updated every 50 msec, when it reaches 1, the tempo detection is started; doesn't go below 0. */
	int m_TempoDetectDelay;

	/** The tempo detector instance on which to run the detections. */
	std::unique_ptr<SongTempoDetector> m_Detector;


	/** Fills in the values for options into the UI selectors. */
	void initOptionsUi();

	/** Updates the UI to match the specified options. */
	void selectOptions(const SongTempoDetector::Options & a_Options);

	/** Reads the UI settings and returns an Options object initialized by those settings. */
	SongTempoDetector::Options readOptionsFromUi();

	/** Updates the specified row in the History view. */
	void updateHistoryRow(int a_Row);


private slots:

	/** Starts a timer, upon timeout reads the UI values and starts detecting the tempo.
	Calling this again before the timer elapses restarts the timer from the beginning.
	Used for user-editable values that may require several calls before the entire value is input. */
	void delayedDetectTempo();

	/** Reads the UI values and starts detecting the tempo.
	If the options set in the UI are already in m_History, uses the results from there instead. */
	void detectTempo();

	/** Emitted by TempoDetector after it scans the song.
	Adds the result to m_History and updates the UI. */
	void songScanned(SongPtr a_Song, TempoDetector::ResultPtr a_Result);

	/** Asks the user for a filename, then saves the debug audio data with beats to the file. */
	void saveDebugBeats();

	/** Asks the user for a filename, then saves the debug audio data with levels to the file. */
	void saveDebugLevels();
};





#endif // DLGTEMPODETECT_H

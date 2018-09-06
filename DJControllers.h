#ifndef DJCONTROLLERS_H
#define DJCONTROLLERS_H





#include <QThread>
#include <QTimer>
#include "ComponentCollection.h"
#include "Controller/AbstractController.h"





// fwd:
class MidiEnumerator;





/** A component that provides access to DJ controllers. */
class DJControllers:
	public QThread,
	public ComponentCollection::Component<ComponentCollection::ckDJControllers>
{
	using Super = QThread;
	Q_OBJECT


public:

	explicit DJControllers(QObject * a_Parent = nullptr);

	virtual ~DJControllers() override;


private:

	/** The timer used for periodically updating the port list and opening any new ports. */
	QTimer m_TimerUpdate;

	/** Enumerator of the MIDI-based controllers. */
	std::unique_ptr<MidiEnumerator> m_MidiEnumerator;

	/** Controllers currently connected and detected. */
	std::vector<std::shared_ptr<AbstractController>> m_Controllers;


	/** Sends a message to turn the specified LED on or off. */
	void setLed(int a_Led, bool a_LightUp);


	// QThread overrides:
	virtual void run() override;


signals:

	/** The user connected a controller that was successfully detected. */
	void controllerConnected(const QString & a_Name);

	/** The user disconnected the controller. */
	void controllerRemoved();

	/** The user pressed the Play / Pause button on the controller. */
	void playPause();

	/** The user changed the TempoCoeff on the controller.
	a_TempoCoeff ranges from 0.0 to 1.0. */
	void setTempoCoeff(qreal a_TempoCoeff);

	/** The user pressed the Pitch Plus button on the controller. */
	void pitchPlus();

	/** The user pressed the Pitch Minus button on the controller. */
	void pitchMinus();

	/** The user changed the Volume on the controller. */
	void setVolume(qreal a_Volume);

	/** The user rolled the Navigation wheel up on the controller. */
	void navigateUp();

	/** The user rolled the Navigation wheel down on the controller. */
	void navigateDown();


private slots:

	/** Called periodically by a timer.
	Checks that the currently open port is still active, if not, tries to detect controllers on all ports. */
	void periodicUpdate();

	/** Called by the enumerators whenever a new controller is detected. */
	void newControllerDetected(std::shared_ptr<AbstractController> a_Controller);

	/** Called by the enumerators whenever a controller is disconnected. */
	void controllerDisconnected(std::shared_ptr<AbstractController> a_Controller);

	/** Called when the user presses a key on a controller. */
	void controllerKeyPressed(int a_Key);

	/** Called when the user moves a slider on a controller. */
	void controllerSliderSet(int a_Slider, qreal a_Value);

	/** Called when the user moves a wheel on a controller. */
	void controllerWheelMoved(int a_Wheel, int a_NumSteps);


public slots:

	/** Sets the state of the Play / Pause button's LED. */
	void setLedPlay(bool a_LightUp);

	/** Sets the state of the Cure button's LED. */
	void setLedCue(bool a_LightUp);

	/** Sets the state of the Sync button's LED. */
	void setLedSync(bool a_LightUp);

	/** Sets the state of the Headphone button's LED. */
	void setLedPfl(bool a_LightUp);
};





#endif // DJCONTROLLERS_H

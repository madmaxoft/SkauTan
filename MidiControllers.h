#ifndef MIDICONTROLLERS_H
#define MIDICONTROLLERS_H





#include <QObject>
#include <QTimer>
#include <RtMidi.h>
#include "ComponentCollection.h"





/** A component that provides access to MIDI-based DJ controllers.
Only one controller at a time is connected and used.
A controller has both a MIDI IN and MIDI OUT port that it communicates on.
There's no explicit message from RtMidi when a new port is added to the system or disappears from the system,
we use a periodic timer to detect changes. */
class MidiControllers:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckMidiControllers>
{
	using Super = QObject;
	Q_OBJECT

public:

	explicit MidiControllers(QObject * a_Parent = nullptr);


private:

	/** The timer used for periodically updating the port list and opening any new ports. */
	QTimer m_TimerUpdate;

	/** The IN port of the current controller. */
	std::unique_ptr<RtMidiIn> m_PortIn;

	/** The OUT port of the current controller. */
	std::unique_ptr<RtMidiOut> m_PortOut;


	/** Tries to open each output port and detect a DJ controller on it, while keeping all inputs open for feedback.
	Updates m_PortIn and m_PortOut based on the detected controller.
	If a new controller is found, emits the proper signals. */
	void detectController();

	/** Sets the IN and OUT port of a newly detected controller.
	Emits the controllerConnected or connectedRemoved signal, as appropriate. */
	void setPortPair(
		std::unique_ptr<RtMidiIn> a_PortIn,
		std::unique_ptr<RtMidiOut> a_PortOut,
		const QString & a_PortName
	);

	/** Sends the specified message to the OUT port, if available.
	Ignored if OUT port not assigned. */
	void sendMessage(const std::vector<unsigned char> & a_Msg);

	/** Sends a message to turn the specified LED on or off. */
	void setLed(unsigned char a_Led, bool a_LightUp);

	/** Called by the MIDI subsystem whenever a MIDI message from m_PortIn is received. */
	void midiInCallback(double a_TimeStamp, std::vector<unsigned char> * a_Message);

	/** Called by the MIDI subsystem whenever a MIDI message from m_PortIn is received.
	Low level helper, translates the C=style interface into C++ object call. */
	static void midiInCallbackHelper(double a_TimeStamp, std::vector<unsigned char> * a_Message, void * a_UserData)
	{
		(reinterpret_cast<MidiControllers *>(a_UserData))->midiInCallback(a_TimeStamp, a_Message);
	}

signals:

	/** The user connected a controller that was successfully detected. */
	void controllerConnected(const QString & a_PortName);

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


public slots:

	/** Sets the state of the Play / Pause button's LED. */
	void setLedPlay(bool a_LightUp);

	/** Sets the state of the Cure button's LED. */
	void setLedCue(bool a_LightUp);

	/** Sets the state of the Sync button's LED. */
	void setLedSync(bool a_LightUp);

	/** Sets the state of the Headphone button's LED. */
	void setLedHeadphone(bool a_LightUp);
};





#endif // MIDICONTROLLERS_H

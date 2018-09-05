#ifndef MIDIENUMERATOR_H
#define MIDIENUMERATOR_H





#include "AbstractEnumerator.h"





class MidiPortIn;
class MidiPortOut;
using MidiPortInPtr = std::shared_ptr<MidiPortIn>;
using MidiPortOutPtr = std::shared_ptr<MidiPortOut>;





class MidiEnumerator:
	public AbstractEnumerator
{
	using Super = AbstractEnumerator;
	Q_OBJECT


public:
	MidiEnumerator(QObject * a_Parent);

	// AbstractEnumerator overrides:
	virtual void periodicUpdate() override;
	virtual std::vector<std::shared_ptr<AbstractController>> queryControllers() override;


public slots:

	/** Rescans all the ports in the system, runs detection on all of the ports. */
	void rescanPorts();


protected:

	/** All controllers currently present. */
	std::vector<AbstractControllerPtr> m_Controllers;

	/** Number of MIDI IN ports at last port scan.
	Used to crudely detect HW change by periodically comparing with current count. */
	uint m_LastNumPortsIn;

	/** Number of MIDI OUT ports at last port scan.
	Used to crudely detect HW change by periodically comparing with current count. */
	uint m_LastNumPortsOut;


	/** Detects controllers that use a single MIDI IN port.
	a_PortsIn is a vector of all currently openable MIDI IN ports on which to run the detection.
	The ports that are used by the controllers are removed from a_PortsIn. */
	std::vector<AbstractControllerPtr> detectSinglePortControllers(std::vector<MidiPortInPtr> & a_PortsIn);

	/** Detects controllers that use a pair of MIDI IN and OUT port.
	a_PortsIn is a vector of all currently openable MIDI IN ports on which to run the detection.
	The ports that are used by the controllers are removed from a_PortsIn. */
	std::vector<AbstractControllerPtr> detectDualPortControllers(std::vector<MidiPortInPtr> & a_PortsIn);

	/** If the specified message is a valid MMC response, returns a Controller based on the specified port pair.
	Returns nullptr if not a detectable response. */
	AbstractControllerPtr controllerFromMMCResonse(const QByteArray & a_Message,
		MidiPortInPtr a_PortIn,
		MidiPortOutPtr a_PortOut
	);

	/** Tries to match the port name to a list of known controller port names.
	If a match is found, creates and returns a controller; returns nullptr if no match. */
	AbstractControllerPtr controllerFromPortIn(MidiPortInPtr a_PortIn);

	/** Adds the specified controller to the internal list of controllers.
	Emits the newControllerDetected() signal and connects to the controller's unplugged() signal. */
	void addController(AbstractControllerPtr && a_Controller);
};





#endif // MIDIENUMERATOR_H

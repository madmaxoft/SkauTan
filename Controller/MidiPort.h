#ifndef MIDIPORT_H
#define MIDIPORT_H





#include <memory>
#include <QObject>





// fwd:
class RtMidiIn;
class RtMidiOut;





/** Represents a single generic MIDI port. */
class MidiPort:
	public QObject
{
	using Super = QObject;
	Q_OBJECT

public:

	explicit MidiPort(QObject * a_Parent = nullptr);

	unsigned portNumber() const { return m_PortNumber; }
	const std::string & portName() const { return m_PortName; }

	/** Returns the number of MIDI OUT ports available to the system. */
	static unsigned getNumOutPorts();

	/** Returns the number of MIDI IN ports available to the system. */
	static unsigned getNumInPorts();

protected:

	/** The port number (index) of the MIDI port.
	Only valid when the port is open. */
	unsigned m_PortNumber;

	/** The name of the MIDI port.
	Only valid when the port is open. */
	std::string m_PortName;


signals:

	/** Emitted when the port no longer responds. */
	void unplugged();

public slots:
};





/** MidiPort specialization that provides the MIDI IN functionality. */
class MidiPortIn:
	public MidiPort
{
	Q_OBJECT

public:

	/** Constructor needs to be explicitly declared, and defined in CPP file,
	otherwise compiler complains about missing type for the contained RtMidiIn. */
	MidiPortIn();

	/** Destructor needs to be explicitly declared, and defined in CPP file,
	otherwise compiler complains about missing type for the contained RtMidiIn. */
	~MidiPortIn();

	/** Opens the specified MIDI IN port.
	Returns true on success, false on failure.
	If the port is already open, closes it and reopens. */
	bool open(unsigned a_PortNumber);

	/** Returns true if the port is valid (open). */
	bool isValid() const { return (m_MidiIn != nullptr); }


signals:

	/** Emitted when a message is received from the port */
	void receivedMessage(double a_Timestamp, const QByteArray & a_Message);


protected:

	/** The underlying MIDI IN port.
	While the port is closed / invalid, is set to nullptr. */
	std::unique_ptr<RtMidiIn> m_MidiIn;


	/** The callback set on the underlying MIDI IN port. */
	static void midiPortCallback(
		double a_Timestamp,
		std::vector<unsigned char> * a_Message,
		void * a_UserData
	)
	{
		emit (static_cast<MidiPortIn *>(a_UserData))->receivedMessage(a_Timestamp, QByteArray(
			reinterpret_cast<const char *>(a_Message->data()), static_cast<int>(a_Message->size())
		));
	}
};





/** MidiPort specialization that provides a MIDI OUT functionality. */
class MidiPortOut:
	public MidiPort
{
	Q_OBJECT

public:

	/** Constructor needs to be explicitly declared, and defined in CPP file,
	otherwise compiler complains about missing type for the contained RtMidiOut. */
	MidiPortOut();

	/** Destructor needs to be explicitly declared, and defined in CPP file,
	otherwise compiler complains about missing type for the contained RtMidiOut. */
	~MidiPortOut();

	/** Opens the specified MIDI IN port.
	Returns true on success, false on failure.
	If the port is already open, closes it and reopens. */
	bool open(unsigned a_PortNumber);

	/** Returns true if the port is valid (open). */
	bool isValid() const { return (m_MidiOut != nullptr); }

	/** Sends the specified bytes as a MIDI OUT message through the port. */
	void sendMessage(const std::vector<unsigned char> & a_Message);


protected:

	/** The underlying MIDI OUT port. */
	std::unique_ptr<RtMidiOut> m_MidiOut;
};





using MidiPortInPtr  = std::shared_ptr<MidiPortIn>;
using MidiPortOutPtr = std::shared_ptr<MidiPortOut>;





#endif // MIDIPORT_H

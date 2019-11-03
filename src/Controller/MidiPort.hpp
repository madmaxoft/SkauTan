#pragma once

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

	explicit MidiPort(QObject * aParent = nullptr);

	unsigned portNumber() const { return mPortNumber; }
	const std::string & portName() const { return mPortName; }

	/** Returns the number of MIDI OUT ports available to the system. */
	static unsigned getNumOutPorts();

	/** Returns the number of MIDI IN ports available to the system. */
	static unsigned getNumInPorts();

protected:

	/** The port number (index) of the MIDI port.
	Only valid when the port is open. */
	unsigned mPortNumber;

	/** The name of the MIDI port.
	Only valid when the port is open. */
	std::string mPortName;


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
	bool open(unsigned aPortNumber);

	/** Returns true if the port is valid (open). */
	bool isValid() const { return (mMidiIn != nullptr); }


signals:

	/** Emitted when a message is received from the port */
	void receivedMessage(double aTimestamp, const QByteArray & aMessage);


protected:

	/** The underlying MIDI IN port.
	While the port is closed / invalid, is set to nullptr. */
	std::unique_ptr<RtMidiIn> mMidiIn;


	/** The callback set on the underlying MIDI IN port. */
	static void midiPortCallback(
		double aTimestamp,
		std::vector<unsigned char> * aMessage,
		void * aUserData
	)
	{
		emit (static_cast<MidiPortIn *>(aUserData))->receivedMessage(aTimestamp, QByteArray(
			reinterpret_cast<const char *>(aMessage->data()), static_cast<int>(aMessage->size())
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
	bool open(unsigned aPortNumber);

	/** Returns true if the port is valid (open). */
	bool isValid() const { return (mMidiOut != nullptr); }

	/** Sends the specified bytes as a MIDI OUT message through the port. */
	void sendMessage(const std::vector<unsigned char> & aMessage);


protected:

	/** The underlying MIDI OUT port. */
	std::unique_ptr<RtMidiOut> mMidiOut;
};





using MidiPortInPtr  = std::shared_ptr<MidiPortIn>;
using MidiPortOutPtr = std::shared_ptr<MidiPortOut>;

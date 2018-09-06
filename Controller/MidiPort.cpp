#include "MidiPort.h"
#include "../lib/RtMidi.h"
#include <QDebug>





static void midiPortErrorCallback(
	RtMidiError::Type a_Type,
	const std::string & a_ErrorText,
	void * a_UserData
)
{
	auto port = static_cast<MidiPort *>(a_UserData);
	qWarning() << "MIDI port error: port " << port->portName().c_str() << ", error type " << a_Type << ", msg " << a_ErrorText.c_str();
	emit port->unplugged();
};





////////////////////////////////////////////////////////////////////////////////
// MidiPort:

MidiPort::MidiPort(QObject * a_Parent):
	Super(a_Parent)
{

}





unsigned MidiPort::getNumOutPorts()
{
	RtMidiOut out;
	return out.getPortCount();
}





unsigned MidiPort::getNumInPorts()
{
	RtMidiIn in;
	return in.getPortCount();
}





////////////////////////////////////////////////////////////////////////////////
// MidiPortIn:

MidiPortIn::MidiPortIn()
{
	// Nothing explicit needed
	/*
	The constructor needs to be defined in CPP file, otherwise the compiler complains
	about missing type for the contained RtMidiIn.
	*/
}





MidiPortIn::~MidiPortIn()
{
	// Nothing explicit needed
	/*
	The destructor needs to be defined in CPP file, otherwise the compiler complains
	about missing type for the contained RtMidiIn.
	*/
}





bool MidiPortIn::open(unsigned a_PortNumber)
{
	if (m_MidiIn != nullptr)
	{
		emit unplugged();
		m_MidiIn.reset(nullptr);
	}

	auto portName = m_MidiIn->getPortName(a_PortNumber);
	m_MidiIn.reset(new RtMidiIn);
	try
	{
		m_MidiIn->openPort(a_PortNumber, portName);
		m_MidiIn->setCallback(&MidiPortIn::midiPortCallback, this);
		m_MidiIn->ignoreTypes(false, true, false);
		m_MidiIn->setErrorCallback(midiPortErrorCallback, static_cast<MidiPort *>(this));
	}
	catch (const RtMidiError & a_Error)
	{
		qWarning() << "Error while opening MIDI IN port "
			<< a_PortNumber << " (" << portName.c_str() << "): "
			<< a_Error.what();
		m_MidiIn.reset(nullptr);
		return false;
	}
	m_PortNumber = a_PortNumber;
	m_PortName = portName;
	return true;
}





////////////////////////////////////////////////////////////////////////////////
// MidiPortOut:

MidiPortOut::MidiPortOut()
{
	// Nothing explicit needed
	/*
	The constructor needs to be defined in CPP file, otherwise the compiler complains
	about missing type for the contained RtMidiOut.
	*/
}





MidiPortOut::~MidiPortOut()
{
	// Nothing explicit needed
	/*
	The destructor needs to be defined in CPP file, otherwise the compiler complains
	about missing type for the contained RtMidiOut.
	*/
}





bool MidiPortOut::open(unsigned a_PortNumber)
{
	if (m_MidiOut != nullptr)
	{
		emit unplugged();
		m_MidiOut.reset(nullptr);
	}

	m_MidiOut.reset(new RtMidiOut);
	auto portName = m_MidiOut->getPortName(a_PortNumber);
	try
	{
		m_MidiOut->openPort(a_PortNumber, portName);
		m_MidiOut->setErrorCallback(midiPortErrorCallback, static_cast<MidiPort *>(this));
	}
	catch (const RtMidiError & a_Error)
	{
		qWarning() << "Error while opening MIDI OUT port "
			<< a_PortNumber << " (" << portName.c_str() << "): "
			<< a_Error.what();
		m_MidiOut.reset(nullptr);
		return false;
	}
	m_PortNumber = a_PortNumber;
	m_PortName = portName;
	return true;
}





void MidiPortOut::sendMessage(const std::vector<unsigned char> & a_Message)
{
	try
	{
		m_MidiOut->sendMessage(&a_Message);
	}
	catch (const RtMidiError & a_Error)
	{
		qDebug() << "MIDI OUT error (port " << m_PortName.c_str() << "): " << a_Error.what();
		emit unplugged();
	}
}

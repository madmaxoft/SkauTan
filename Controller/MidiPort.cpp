#include "MidiPort.h"
#include <RtMidi.h>
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





////////////////////////////////////////////////////////////////////////////////
// MidiPortIn:

bool MidiPortIn::open(unsigned a_PortNumber, const std::string & a_PortName)
{
	if (m_MidiIn != nullptr)
	{
		emit unplugged();
		m_MidiIn.reset(nullptr);
	}

	m_MidiIn.reset(new RtMidiIn);
	try
	{
		m_MidiIn->openPort(a_PortNumber, a_PortName);
		m_MidiIn->setCallback(&MidiPortIn::midiPortCallback, this);
		m_MidiIn->ignoreTypes(false, true, false);
		m_MidiIn->setErrorCallback(midiPortErrorCallback, static_cast<MidiPort *>(this));
	}
	catch (const RtMidiError & a_Error)
	{
		qWarning() << "Error while opening MIDI IN port "
			<< a_PortNumber << " (" << a_PortName.c_str() << "): "
			<< a_Error.what();
		m_MidiIn.reset(nullptr);
		return false;
	}
	m_PortNumber = a_PortNumber;
	m_PortName = a_PortName;
	return true;
}





////////////////////////////////////////////////////////////////////////////////
// MidiPortOut:

bool MidiPortOut::open(unsigned a_PortNumber, const std::string & a_PortName)
{
	if (m_MidiOut != nullptr)
	{
		emit unplugged();
		m_MidiOut.reset(nullptr);
	}

	m_MidiOut.reset(new RtMidiOut);
	try
	{
		m_MidiOut->openPort(a_PortNumber, a_PortName);
		m_MidiOut->setErrorCallback(midiPortErrorCallback, static_cast<MidiPort *>(this));
	}
	catch (const RtMidiError & a_Error)
	{
		qWarning() << "Error while opening MIDI OUT port "
			<< a_PortNumber << " (" << a_PortName.c_str() << "): "
			<< a_Error.what();
		m_MidiOut.reset(nullptr);
		return false;
	}
	m_PortNumber = a_PortNumber;
	m_PortName = a_PortName;
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

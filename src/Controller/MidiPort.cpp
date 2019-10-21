#include "MidiPort.hpp"
#include <RtMidi/RtMidi.h>
#include <QDebug>





static void midiPortErrorCallback(
	RtMidiError::Type aType,
	const std::string & aErrorText,
	void * aUserData
)
{
	auto port = static_cast<MidiPort *>(aUserData);
	qWarning() << "MIDI port error: port " << port->portName().c_str() << ", error type " << aType << ", msg " << aErrorText.c_str();
	emit port->unplugged();
};





////////////////////////////////////////////////////////////////////////////////
// MidiPort:

MidiPort::MidiPort(QObject * aParent):
	Super(aParent)
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





bool MidiPortIn::open(unsigned aPortNumber)
{
	if (mMidiIn != nullptr)
	{
		emit unplugged();
		mMidiIn.reset(nullptr);
	}

	mMidiIn.reset(new RtMidiIn);
	auto portName = mMidiIn->getPortName(aPortNumber);
	try
	{
		mMidiIn->openPort(aPortNumber, portName);
		mMidiIn->setCallback(&MidiPortIn::midiPortCallback, this);
		mMidiIn->ignoreTypes(false, true, false);
		mMidiIn->setErrorCallback(midiPortErrorCallback, static_cast<MidiPort *>(this));
	}
	catch (const RtMidiError & aError)
	{
		qWarning() << "Error while opening MIDI IN port "
			<< aPortNumber << " (" << portName.c_str() << "): "
			<< aError.what();
		mMidiIn.reset(nullptr);
		return false;
	}
	mPortNumber = aPortNumber;
	mPortName = portName;
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





bool MidiPortOut::open(unsigned aPortNumber)
{
	if (mMidiOut != nullptr)
	{
		emit unplugged();
		mMidiOut.reset(nullptr);
	}

	mMidiOut.reset(new RtMidiOut);
	auto portName = mMidiOut->getPortName(aPortNumber);
	try
	{
		mMidiOut->openPort(aPortNumber, portName);
		mMidiOut->setErrorCallback(midiPortErrorCallback, static_cast<MidiPort *>(this));
	}
	catch (const RtMidiError & aError)
	{
		qWarning() << "Error while opening MIDI OUT port "
			<< aPortNumber << " (" << portName.c_str() << "): "
			<< aError.what();
		mMidiOut.reset(nullptr);
		return false;
	}
	mPortNumber = aPortNumber;
	mPortName = portName;
	return true;
}





void MidiPortOut::sendMessage(const std::vector<unsigned char> & aMessage)
{
	try
	{
		mMidiOut->sendMessage(&aMessage);
	}
	catch (const RtMidiError & aError)
	{
		qDebug() << "MIDI OUT error (port " << mPortName.c_str() << "): " << aError.what();
		emit unplugged();
	}
}

#include "MidiController.hpp"
#include <QDebug>
#include "../Utils.hpp"  // For utils::toHex()





MidiController::MidiController(MidiPortInPtr a_PortIn):
	m_PortIn(a_PortIn)
{
	connect(a_PortIn.get(), &MidiPort::unplugged, this, &MidiController::unplugged);
}





MidiController::MidiController(
	MidiPortInPtr a_PortIn,
	MidiPortOutPtr a_PortOut
):
	m_PortIn(a_PortIn),
	m_PortOut(a_PortOut)
{
	connect(a_PortIn.get(),  &MidiPort::unplugged, this, &MidiController::unplugged);
	if (a_PortOut != nullptr)
	{
		connect(a_PortOut.get(), &MidiPort::unplugged, this, &MidiController::unplugged);
	}
}





bool MidiController::ping()
{
	// TODO
	return true;
}





void MidiController::portUnplugged()
{
	emit unplugged();
}





////////////////////////////////////////////////////////////////////////////////
// SimpleMidiController:

SimpleMidiController::SimpleMidiController(
	MidiPortInPtr a_MidiPortIn,
	MidiPortOutPtr a_MidiPortOut,
	const SimpleMidiController::KeyMap & a_KeyMap,
	const SimpleMidiController::LedMap & a_LedMap,
	const SimpleMidiController::SliderMap & a_SliderMap,
	const SimpleMidiController::WheelMap & a_WheelMap,
	const QString & a_ControllerName
):
	Super(a_MidiPortIn, a_MidiPortOut),
	m_KeyMap(a_KeyMap),
	m_LedMap(a_LedMap),
	m_SliderMap(a_SliderMap),
	m_WheelMap(a_WheelMap),
	m_ControllerName(a_ControllerName)
{
	connect(m_PortIn.get(), &MidiPortIn::receivedMessage, this, &SimpleMidiController::processMidiInMessage);
}





void SimpleMidiController::setLed(int a_Led, bool a_On)
{
	if (m_PortOut == nullptr)
	{
		return;
	}
	auto led = m_LedMap.find(a_Led);
	if (led == m_LedMap.end())
	{
		return;
	}
	unsigned char msgType = a_On ? 0x90 : 0x80;
	m_PortOut->sendMessage({msgType, led->second, 0x40});
}





void SimpleMidiController::processMidiInMessage(double a_TimeStamp, const QByteArray & a_Message)
{
	Q_UNUSED(a_TimeStamp);

	if (a_Message.isEmpty())
	{
		return;
	}

	auto msgBytes = reinterpret_cast<const unsigned char *>(a_Message.constData());
	switch (msgBytes[0] & 0xf0)
	{
		case 0x80:  // Note OFF
		case 0x90:  // Note ON
		{
			if (a_Message.size() != 3)
			{
				qDebug() << "Invalid NoteOnOff MIDI message size: " << Utils::toHex(a_Message);
				return;
			}
			if (
				((msgBytes[0] & 0xf0) == 0x80) ||  // Note OFF
				(msgBytes[2] == 0)                 // Note ON with zero velocity => Note OFF
			)
			{
				// Do we want a "keyUp" event?
				return;
			}
			else
			{
				auto key = m_KeyMap.find(msgBytes[1]);
				if (key != m_KeyMap.end())
				{
					emit keyPressed(key->second);
					return;
				}
			}
			// qDebug() << "Unhandled Note: " << Utils::toHex(a_Message);
			break;
		}

		case 0xb0:  // Control change
		{
			if (a_Message.size() != 3)
			{
				qDebug() << "Invalid ControlChange MIDI message size: " << Utils::toHex(a_Message);
				return;
			}
			auto slider = m_SliderMap.find(msgBytes[1]);
			if (slider != m_SliderMap.end())
			{
				auto value = static_cast<double>(msgBytes[2]) / 127;
				emit sliderSet(slider->second, value);
				return;
			}
			auto wheel = m_WheelMap.find(msgBytes[1]);
			if (wheel != m_WheelMap.end())
			{
				auto numSteps = (msgBytes[2] < 0x40) ? msgBytes[2] : (msgBytes[2] - 128);
				emit wheelMoved(wheel->second, numSteps);
				return;
			}
			// qDebug() << "Unhandled ControlChange: " << Utils::toHex(a_Message);
			break;
		}

		default:
		{
			break;
		}
	}
}






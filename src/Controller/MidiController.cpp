#include "MidiController.hpp"
#include <QDebug>
#include "../Utils.hpp"  // For utils::toHex()





MidiController::MidiController(MidiPortInPtr aPortIn):
	mPortIn(aPortIn)
{
	connect(aPortIn.get(), &MidiPort::unplugged, this, &MidiController::unplugged);
}





MidiController::MidiController(
	MidiPortInPtr aPortIn,
	MidiPortOutPtr aPortOut
):
	mPortIn(aPortIn),
	mPortOut(aPortOut)
{
	connect(aPortIn.get(),  &MidiPort::unplugged, this, &MidiController::unplugged);
	if (aPortOut != nullptr)
	{
		connect(aPortOut.get(), &MidiPort::unplugged, this, &MidiController::unplugged);
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
	MidiPortInPtr aMidiPortIn,
	MidiPortOutPtr aMidiPortOut,
	const SimpleMidiController::KeyMap & aKeyMap,
	const SimpleMidiController::LedMap & aLedMap,
	const SimpleMidiController::SliderMap & aSliderMap,
	const SimpleMidiController::WheelMap & aWheelMap,
	const QString & aControllerName
):
	Super(aMidiPortIn, aMidiPortOut),
	mKeyMap(aKeyMap),
	mLedMap(aLedMap),
	mSliderMap(aSliderMap),
	mWheelMap(aWheelMap),
	mControllerName(aControllerName)
{
	connect(mPortIn.get(), &MidiPortIn::receivedMessage, this, &SimpleMidiController::processMidiInMessage);
}





void SimpleMidiController::setLed(int aLed, bool aOn)
{
	if (mPortOut == nullptr)
	{
		return;
	}
	auto led = mLedMap.find(aLed);
	if (led == mLedMap.end())
	{
		return;
	}
	unsigned char msgType = aOn ? 0x90 : 0x80;
	mPortOut->sendMessage({msgType, led->second, 0x40});
}





void SimpleMidiController::processMidiInMessage(double aTimeStamp, const QByteArray & aMessage)
{
	Q_UNUSED(aTimeStamp);

	if (aMessage.isEmpty())
	{
		return;
	}

	auto msgBytes = reinterpret_cast<const unsigned char *>(aMessage.constData());
	switch (msgBytes[0] & 0xf0)
	{
		case 0x80:  // Note OFF
		case 0x90:  // Note ON
		{
			if (aMessage.size() != 3)
			{
				qDebug() << "Invalid NoteOnOff MIDI message size: " << Utils::toHex(aMessage);
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
				auto key = mKeyMap.find(msgBytes[1]);
				if (key != mKeyMap.end())
				{
					emit keyPressed(key->second);
					return;
				}
			}
			// qDebug() << "Unhandled Note: " << Utils::toHex(aMessage);
			break;
		}

		case 0xb0:  // Control change
		{
			if (aMessage.size() != 3)
			{
				qDebug() << "Invalid ControlChange MIDI message size: " << Utils::toHex(aMessage);
				return;
			}
			auto slider = mSliderMap.find(msgBytes[1]);
			if (slider != mSliderMap.end())
			{
				auto value = static_cast<double>(msgBytes[2]) / 127;
				emit sliderSet(slider->second, value);
				return;
			}
			auto wheel = mWheelMap.find(msgBytes[1]);
			if (wheel != mWheelMap.end())
			{
				auto numSteps = (msgBytes[2] < 0x40) ? msgBytes[2] : (msgBytes[2] - 128);
				emit wheelMoved(wheel->second, numSteps);
				return;
			}
			// qDebug() << "Unhandled ControlChange: " << Utils::toHex(aMessage);
			break;
		}

		default:
		{
			break;
		}
	}
}






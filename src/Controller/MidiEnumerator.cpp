#include "MidiEnumerator.hpp"
#include <set>
#include <QDebug>
#include <QSemaphore>
#include <QRegularExpression>
#include "MidiController.hpp"





static const int PING_WAIT_TIMEOUT = 100;  // 100 msec timeout between MMC query and response for detection





static const struct SimpleControllerDef
{
	/** The user-visible name of the controller. */
	QString controllerName;

	/** Regexp that must be matched by the IN port name in order to consider this controller. */
	QRegularExpression portInMatch;

	/** Regexp that must be matched by the OUT port name in order to consider this controller. */
	QRegularExpression portOutMatch;

	/** The MMC device query response header.
	If this matches the beginning of a received MIDI message, the controller is considered detected.
	If the device has no IN port, leave this empty. */
	std::vector<unsigned char> responseMMC;

	/** The mapping of MIDI Note messages to controller keys. */
	SimpleMidiController::KeyMap keyMap;

	/** The mapping of controller LEDs to MIDI OUT note messages. */
	SimpleMidiController::LedMap ledMap;

	SimpleMidiController::SliderMap sliderMap;

	SimpleMidiController::WheelMap wheelMap;
} g_SimpleControllerDefs[] =
{
	{
		// The DummyMIDIPort is provided by LoopMIDI ( http://www.tobias-erichsen.de/software/loopmidi.html )
		// It is used solely for testing port arrival and departure
		QString("DummyMIDIPort"),
		QRegularExpression("DummyMIDIPort"),
		QRegularExpression(),
		{},  // No MMC response
		{},  // No keyMap
		{},  // No ledMap
		{},  // No sliderMap
		{},  // No wheelMap
	},  // DummyMIDIPort

	{
		// The MK-361 USB MIDI keyboard is the only HW currently available for testing
		// Used for testing, despite not being a DJ controller as such:
		QString("MK-361 USB MIDI keyboard"),
		QRegularExpression("MK\\-361 USB MIDI.*"),
		QRegularExpression(),
		{},  // No MMC response
		{
			{ 0x18, AbstractController::skPlayPause1},  // Lowest key on the keyboard
			// TODO: More testing keys
		},  // keyMap
		{
			// No LEDs - no MIDI OUT
		},  // ledMap
		{
			{ 0x07, AbstractController::ssMasterVolume },  // Volume slider
			{ 0x06, AbstractController::ssPitch1 },        // Modulation slider
		},  // sliderMap
		{
			{ 0x40, AbstractController::swBrowse },  // Sustain pedal -> browse down
		},  // wheelMap
	},  // MK-361

	{
		QString("Numark DJ2Go"),
		QRegularExpression(),
		QRegularExpression(),
		{
			0xf0, 0x7e, 0x00,  // Non-realtime SysEx, channel 0
			0x06, 0x02,        // MMC response
			0x00, 0x01, 0x3f,  // Manufacturer
			0x27               // Model number
		},
		{
			{ 0x33, AbstractController::skCue1 },
			{ 0x3b, AbstractController::skPlayPause1 },
			{ 0x40, AbstractController::skSync1 },
			{ 0x65, AbstractController::skPfl1 },
			{ 0x4b, AbstractController::skLoad1 },
			{ 0x43, AbstractController::skPitchPlus1 },
			{ 0x44, AbstractController::skPitchMinus1 },

			{ 0x3c, AbstractController::skCue2 },
			{ 0x42, AbstractController::skPlayPause2 },
			{ 0x47, AbstractController::skSync2 },
			{ 0x66, AbstractController::skPfl2 },
			{ 0x34, AbstractController::skLoad2 },
			{ 0x45, AbstractController::skPitchPlus2 },
			{ 0x46, AbstractController::skPitchMinus2 },

			{ 0x59, AbstractController::skBack1 },
			{ 0x5a, AbstractController::skEnter1 },
		},  // keyMap
		{
			{ 0x33, AbstractController::slKeyCue1},
			{ 0x3b, AbstractController::slKeyPlayPause1},
			{ 0x40, AbstractController::slKeySync1},
			{ 0x65, AbstractController::slKeyPfl1},
			{ 0x3c, AbstractController::slKeyCue2},
			{ 0x42, AbstractController::slKeyPlayPause2},
			{ 0x47, AbstractController::slKeySync2},
			{ 0x66, AbstractController::slKeyPfl2},
		},  // ledMap
		{
			{ 0x17, AbstractController::ssMasterVolume},
			{ 0x08, AbstractController::ssVolume1},
			{ 0x09, AbstractController::ssVolume2},
			{ 0x0a, AbstractController::ssCrossfade1},
			{ 0x0d, AbstractController::ssPitch1},
			{ 0x0e, AbstractController::ssPitch2},
		},  // sliderMap
		{
			{ 0x1a, AbstractController::swBrowse},
			{ 0x19, AbstractController::swJog1},
			{ 0x18, AbstractController::swJog2},
		},  // wheelMap
	},  // Numark DJ2Go
};





MidiEnumerator::MidiEnumerator(QObject * aParent):
	Super(aParent),
	mLastNumPortsIn(0),
	mLastNumPortsOut(0)
{
}





void MidiEnumerator::periodicUpdate()
{
	if (
		(mLastNumPortsIn  != MidiPort::getNumInPorts()) ||
		(mLastNumPortsOut != MidiPort::getNumOutPorts())
	)
	{
		qDebug() << "MIDI Port counts differ, rescanning...";
		rescanPorts();
	}
}





std::vector<std::shared_ptr<AbstractController> > MidiEnumerator::queryControllers()
{
	return mControllers;
}





void MidiEnumerator::rescanPorts()
{
	#if 0
	// DEBUG: List all ports:
	{
		RtMidiIn dummyIn;
		auto numPorts = dummyIn.getPortCount();
		qDebug() << "Number of MIDI IN ports: " << numPorts;
		for (unsigned i = 0; i < numPorts; ++i)
		{
			qDebug() << "MIDI IN: " << dummyIn.getPortName(i).c_str();
		}
		RtMidiOut dummyOut;
		numPorts = dummyOut.getPortCount();
		qDebug() << "Number of MIDI OUT ports: " << numPorts;
		for (unsigned i = 0; i < numPorts; ++i)
		{
			qDebug() << "MIDI OUT: " << dummyOut.getPortName(i).c_str();
		}
	}
	#endif

	// Open all IN ports:
	std::vector<MidiPortInPtr> portsIn;
	auto numPorts = MidiPort::getNumInPorts();
	for (unsigned i = 0; i < numPorts; ++i)
	{
		auto port = std::make_unique<MidiPortIn>();
		if (port->open(i))
		{
			portsIn.push_back(std::move(port));
		}
	}

	// Detect controllers:
	for (auto & c: detectSinglePortControllers(portsIn))
	{
		addController(std::move(c));
	}
	for (auto & c: detectDualPortControllers(portsIn))
	{
		addController(std::move(c));
	}

	// Update the last port counts:
	mLastNumPortsIn = numPorts;
	mLastNumPortsOut = MidiPort::getNumOutPorts();
}





std::vector<AbstractControllerPtr> MidiEnumerator::detectSinglePortControllers(std::vector<MidiPortInPtr> & aPortsIn)
{
	// Detect controllers:
	std::vector<AbstractControllerPtr> res;
	std::set<MidiPortInPtr> usedPorts;
	for (auto & port: aPortsIn)
	{
		auto controller = controllerFromPortIn(port);
		if (controller != nullptr)
		{
			qDebug() << "Detected a single-port controller:";
			qDebug() << "  name: " << controller->name() << ", IN: " << port->portName().c_str();
			res.push_back(controller);
			usedPorts.insert(port);
		}

	}

	// Remove usedPorts from aPortsIn:
	aPortsIn.erase(std::remove_if(aPortsIn.begin(), aPortsIn.end(),
		[&](const MidiPortInPtr & aTestedPort)
		{
			return (usedPorts.find(aTestedPort) != usedPorts.end());
		}
	), aPortsIn.end());

	return res;
}





std::vector<AbstractControllerPtr> MidiEnumerator::detectDualPortControllers(std::vector<MidiPortInPtr> & aPortsIn)
{
	// Connect each IN port to a signal that selects the port on a received reply:
	std::vector<AbstractControllerPtr> res;
	std::set<MidiPortInPtr> usedPorts;  // IN ports that have successfully detected a controller
	MidiPortOutPtr portOut;  // The currently queried OUT port
	QSemaphore evt(0);  // The event for signalling we found a matching IN port
	std::vector<QMetaObject::Connection> connections;  // The Qt signal-slot connections to be removed at end
	for (auto & port: aPortsIn)
	{
		connections.push_back(connect(port.get(), &MidiPortIn::receivedMessage,
			[&res, &usedPorts, port, &portOut, this, &evt](double aTimestamp, const QByteArray & aMessage)
			{
				Q_UNUSED(aTimestamp);
				auto controller = controllerFromMMCResonse(aMessage, port, portOut);
				if (controller == nullptr)
				{
					return;
				}
				qDebug() << "Detected a dual-port controller:";
				qDebug() << "  name: " << controller->name()
					<< ", IN: " << port->portName().c_str()
					<< ", OUT: " << portOut->portName().c_str();
				res.push_back(controller);
				usedPorts.insert(port);
				evt.release(1);
			}
		));
	}

	// Send a MMC query through each OUT port (assign to portOut), wait for responses:
	// Cycle through each MIDI OUT port, try sending the MMC device enquiry msg:
	auto numPorts = MidiPort::getNumOutPorts();
	static const std::vector<unsigned char> msg{0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7};  // SysEx - MMC device query
	for (unsigned i = 0; i < numPorts; ++i)
	{
		portOut.reset(new MidiPortOut);
		if (!portOut->open(i))
		{
			continue;
		}
		portOut->sendMessage(msg);
		evt.tryAcquire(1, PING_WAIT_TIMEOUT);
	}

	for (auto & c: connections)
	{
		disconnect(c);
	}

	// Remove usedPorts from aPortsIn:
	aPortsIn.erase(std::remove_if(aPortsIn.begin(), aPortsIn.end(),
		[&](const MidiPortInPtr & aTestedPort)
		{
			return (usedPorts.find(aTestedPort) != usedPorts.end());
		}
	), aPortsIn.end());

	return res;
}





AbstractControllerPtr MidiEnumerator::controllerFromMMCResonse(
	const QByteArray & aMessage,
	MidiPortInPtr aPortIn,
	MidiPortOutPtr aPortOut
)
{
	if (aMessage.size() < 3)
	{
		// Invalid / too small to be a wanted response
		return nullptr;
	}

	// Detect by the response to the MMC device query:
	auto nameIn = QString::fromStdString(aPortIn->portName());
	auto nameOut = QString::fromStdString(aPortOut->portName());
	for (const auto & c: g_SimpleControllerDefs)
	{
		if (!c.portInMatch.match(nameIn).hasMatch())
		{
			continue;
		}
		if (!c.portOutMatch.match(nameOut).hasMatch())
		{
			continue;
		}
		auto numToCompare = std::min(static_cast<size_t>(aMessage.size()), c.responseMMC.size());
		if (memcmp(aMessage.data(), c.responseMMC.data(), numToCompare) != 0)
		{
			continue;
		}
		return std::make_shared<SimpleMidiController>(
			aPortIn, aPortOut,
			c.keyMap,
			c.ledMap,
			c.sliderMap,
			c.wheelMap,
			c.controllerName
		);
	}
	return nullptr;
}





AbstractControllerPtr MidiEnumerator::controllerFromPortIn(MidiPortInPtr aPortIn)
{
	auto name = QString::fromStdString(aPortIn->portName());

	for (const auto & d: g_SimpleControllerDefs)
	{
		if (!d.responseMMC.empty())
		{
			// This is a dual-port controller definition, skip
			continue;
		}
		if (!d.portInMatch.match(name).hasMatch())
		{
			continue;
		}
		return std::make_shared<SimpleMidiController>(
			aPortIn, nullptr,
			d.keyMap, d.ledMap, d.sliderMap, d.wheelMap, d.controllerName
		);
	}

	return nullptr;
}





void MidiEnumerator::addController(AbstractControllerPtr && aController)
{
	// Add to internal list:
	mControllers.push_back(aController);

	// Emit the signal:
	emit newControllerDetected(aController);

	// Connect to the controller's unplugged() signal:
	connect(aController.get(), &MidiController::unplugged,
		[this, aController]()
		{
			for (auto itr = mControllers.begin(), end = mControllers.end(); itr != end; ++itr)
			{
				if (*itr == aController)
				{
					mControllers.erase(itr);
					emit controllerDisconnected(*itr);
					break;
				}
			}
		}
	);
}

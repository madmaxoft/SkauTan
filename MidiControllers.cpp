#include "MidiControllers.h"
#include <QSemaphore>
#include <QDebug>
#include "Utils.h"





/** Number of milliseconds to wait between sending the PING and receiving the PONG from the device. */
static const int PING_WAIT_TIMEOUT = 100;





/** Helper class used while detecting ports.
Provides callback for the MIDI IN that handles the MMC device enquiry msg. */
class PortDetector
{
public:
	/** The MIDI IN port that is listening with this callback. */
	std::unique_ptr<RtMidiIn> m_PortIn;

	/** The user-visible name of m_PortIn. */
	QString m_PortName;

	/** Semaphore to signalize when a valid reply is received. */
	QSemaphore & m_Event;

	/** Set to true if a valid reply was received. */
	bool m_HasReplied;

	unsigned m_PortNumber;



	PortDetector(QSemaphore & a_Event):
		m_Event(a_Event),
		m_HasReplied(false)
	{
	}


	/** Opens the specified port and starts listening for incoming messages. */
	void start(unsigned a_PortNum)
	{
		m_PortIn = std::make_unique<RtMidiIn>();
		m_PortName = QString::fromStdString(m_PortIn->getPortName(a_PortNum));
		m_PortIn->openPort(a_PortNum);
		m_PortIn->setCallback(&callback, this);
		m_PortIn->ignoreTypes(false, false, false);
		m_PortNumber = a_PortNum;
	}


	void detectionCallback(double a_TimeStamp, std::vector<unsigned char> * a_Message)
	{
		Q_UNUSED(a_TimeStamp);

		if ((a_Message == nullptr) || (a_Message->size() < 10))
		{
			// Invalid / too small to be a wanted response
			return;
		}
		const auto & msg = *a_Message;
		static const unsigned char header[] =
		{
			0xf0, 0x7e, 0x00,  // Non-realtime SysEx, channel 0
			0x06, 0x02,        // MMC response
			0x00, 0x01, 0x3f,  // Manufacturer
			0x27               // Model number
		};
		if (memcmp(msg.data(), header, sizeof(header)) != 0)
		{
			// Not a DJ2Go MMC response
			return;
		}

		qDebug() << "Got a MIDI response from a DJ2Go MIDI controller: " << Utils::toHex(QByteArray(
			reinterpret_cast<const char *>(msg.data()),
			static_cast<int>(msg.size())
		));

		m_HasReplied = true;
		m_Event.release(1);
	}


	static void callback(double a_TimeStamp, std::vector<unsigned char> * a_Message, void * a_UserData)
	{
		(reinterpret_cast<PortDetector *>(a_UserData))->detectionCallback(a_TimeStamp, a_Message);
	}
};





////////////////////////////////////////////////////////////////////////////////
// MidiControllers:

MidiControllers::MidiControllers(QObject * a_Parent):
	Super(a_Parent)
{
	connect(&m_TimerUpdate, &QTimer::timeout, this, &MidiControllers::periodicUpdate);
	m_TimerUpdate.start(1000);
}





void MidiControllers::detectController()
{
	// Open all MIDI IN ports with detection callbacks on them:
	QSemaphore event(0);
	std::vector<std::unique_ptr<PortDetector>> portsIn;
	RtMidiIn dummy;
	auto numPorts = dummy.getPortCount();
	for (unsigned i = 0; i < numPorts; ++i)
	{
		auto cb = std::make_unique<PortDetector>(event);
		cb->start(i);
		portsIn.push_back(std::move(cb));
	}

	// Cycle through each MIDI OUT port, try sending the MMC device enquiry msg:
	auto midiOut = std::make_unique<RtMidiOut>();
	numPorts = midiOut->getPortCount();
	for (unsigned i = 0; i < numPorts; ++i)
	{
		midiOut->openPort(i);
		std::vector<unsigned char> msg{0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7};  // SysEx - device query
		midiOut->sendMessage(&msg);
		if (event.tryAcquire(1, PING_WAIT_TIMEOUT))
		{
			// Found a match. Search the IN port detectors for which one it was:
			for (auto & inPort: portsIn)
			{
				if (inPort->m_HasReplied)
				{
					inPort->m_PortIn->cancelCallback();
					setPortPair(std::move(inPort->m_PortIn), std::move(midiOut), inPort->m_PortName);
					return;
				}
			}
		}
		midiOut->closePort();
	}
}





void MidiControllers::setPortPair(
	std::unique_ptr<RtMidiIn> a_PortIn,
	std::unique_ptr<RtMidiOut> a_PortOut,
	const QString & a_PortName
)
{
	if (m_PortIn != nullptr)
	{
		emit controllerRemoved();
	}

	m_PortIn = std::move(a_PortIn);
	m_PortOut = std::move(a_PortOut);
	if (m_PortIn != nullptr)
	{
		m_PortIn->setCallback(midiInCallbackHelper, this);
	}

	// Turn off all LEDs:
	setLedCue(false);
	setLedHeadphone(false);
	setLedPlay(false);
	setLedSync(false);

	if (m_PortIn != nullptr)
	{
		qDebug() << "Controller detected: port " << a_PortName;
		emit controllerConnected(a_PortName);
	}
}





void MidiControllers::sendMessage(const std::vector<unsigned char> & a_Msg)
{
	if (m_PortOut == nullptr)
	{
		return;
	}
	m_PortOut->sendMessage(const_cast<std::vector<unsigned char> *>(&a_Msg));  // The cast is due to older RtMidi versions on Linux
}





void MidiControllers::setLed(unsigned char a_Led, bool a_LightUp)
{
	unsigned char ctl = a_LightUp ? 0x90 : 0x80;
	sendMessage({ctl, a_Led, 0x7f});
}





void MidiControllers::midiInCallback(double a_TimeStamp, std::vector<unsigned char> * a_Message)
{
	Q_UNUSED(a_TimeStamp);
	Q_UNUSED(a_Message);

	if ((a_Message == nullptr) || (a_Message->size() < 3))
	{
		return;
	}
	const auto & msg = *a_Message;
	switch (msg[0])
	{
		case 0x90:
		{
			// Button down
			switch (msg[1])
			{
				case 0x3b: emit playPause(); return;
			}
			return;
		}

		case 0xb0:
		{
			// Absolute / relative controls
			switch (msg[1])
			{
				case 0x08: emit setVolume(static_cast<qreal>(msg[2]) / 127); return;
				case 0x0d: emit setTempoCoeff(static_cast<qreal>(msg[2]) / 127); return;
				case 0x1a:
				{
					if (msg[2] < 0x40)
					{
						emit navigateUp();
					}
					else
					{
						emit navigateDown();
					}
					return;
				}
			}
			break;
		}
	}
}





void MidiControllers::periodicUpdate()
{
	if (m_PortIn == nullptr)
	{
		detectController();
	}
	else
	{
		// pingController();
	}
}





void MidiControllers::setLedPlay(bool a_LightUp)
{
	setLed(0x3b, a_LightUp);
	// setLed(0x42, a_LightUp);
}





void MidiControllers::setLedCue(bool a_LightUp)
{
	setLed(0x33, a_LightUp);
	// setLed(0x3c, a_LightUp);
}





void MidiControllers::setLedSync(bool a_LightUp)
{
	setLed(0x40, a_LightUp);
	// setLed(0x47, a_LightUp);
}





void MidiControllers::setLedHeadphone(bool a_LightUp)
{
	setLed(0x65, a_LightUp);
	// setLed(0x66, a_LightUp);
}

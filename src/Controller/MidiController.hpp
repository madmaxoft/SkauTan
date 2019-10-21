#ifndef MIDICONTROLLER_H
#define MIDICONTROLLER_H





#include "AbstractController.hpp"
#include "MidiPort.hpp"





/** A DJ controller that uses MIDI interface to communicate with the computer.
Binds a MIDI IN and optionally a MIDI OUT to a controller. */
class MidiController:
	public AbstractController
{
	using Super = AbstractController;
	Q_OBJECT

public:

	/** Special type of controller, has no OUT port. */
	MidiController(MidiPortInPtr aPortIn);

	/** Regular controller, has both an OUT and an IN port. */
	MidiController(MidiPortInPtr aPortIn, MidiPortOutPtr aPortOut);


	// AbstractController overrides:
	virtual bool ping() override;

protected:

	/** The IN port used by the controller. */
	MidiPortInPtr mPortIn;

	/** The OUT port used by the controller.
	May be nullptr if the controller has no OUT port. */
	MidiPortOutPtr mPortOut;


private slots:

	/** Emitted by the MIDI port when it is unplugged. */
	void portUnplugged();
};





/** A controller that follows a simple design:
- keys send a NoteOn MIDI message
- LEDs are controlled using outgoing NoteOn / NoteOff MIDI messages
- sliders and wheels send a ControllerChange message.
Each instance takes a mapping in the constructor that translates between specific MIDI messages and actions. */
class SimpleMidiController:
	public MidiController
{
	using Super = MidiController;
	Q_OBJECT

public:
	/** Maps MIDI Note -> Key */
	using KeyMap = std::map<unsigned char, int>;

	/** Maps LED -> MIDI Note. */
	using LedMap = std::map<int, unsigned char>;

	/** Maps MIDI controller -> Slider. */
	using SliderMap = std::map<unsigned char, int>;

	/** Maps MIDI controller -> Wheel. */
	using WheelMap = std::map<unsigned char, int>;


	/** Constructs a controller that uses the specified maps for its operation. */
	SimpleMidiController(
		MidiPortInPtr aMidiPortIn,
		MidiPortOutPtr aMidiPortOut,  // optional, nullptr if unused
		const KeyMap & aKeyMap,
		const LedMap & aLedMap,
		const SliderMap & aSliderMap,
		const WheelMap & aWheelMap,
		const QString & aControllerName
	);


	// AbstractController overrides:
	virtual void setLed(int aLed, bool aOn) override;
	virtual QString name() const override { return mControllerName; }

protected:

	KeyMap mKeyMap;
	LedMap mLedMap;
	SliderMap mSliderMap;
	WheelMap mWheelMap;
	const QString mControllerName;


protected slots:

	/** A message has been received from MIDI IN, process it through the maps into a logical event. */
	void processMidiInMessage(double aTimeStamp, const QByteArray & aMessage);
};





#endif // MIDICONTROLLER_H

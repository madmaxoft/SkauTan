#ifndef ABSTRACTCONTROLLER_H
#define ABSTRACTCONTROLLER_H





#include <memory>
#include <QObject>





/** Represents a single controller connected to the system and detected by the enumerators. */
class AbstractController:
	public QObject
{
	using Super = QObject;
	Q_OBJECT


public:

	/** The keys that are standardized, assumably present on most controllers.
	The postfix numbering indicates the deck, goes from left to right, top to bottom (like English is written). */
	enum StandardKey
	{
		skPlay1,
		skPause1,
		skPlayPause1,
		skCue1,
		skSync1,
		skPfl1,
		skPitchPlus1,
		skPitchMinus1,
		skLoad1,
		skEnter1,
		skBack1,
		skGenericA1,
		skGenericB1,
		skGenericC1,
		skGenericD1,

		skPlay2,
		skPause2,
		skPlayPause2,
		skCue2,
		skSync2,
		skPfl2,
		skPitchPlus2,
		skPitchMinus2,
		skLoad2,
		skEnter2,
		skBack2,
		skGenericA2,
		skGenericB2,
		skGenericC2,
		skGenericD2,

		skCustomStart = 1024,  ///< Start of custom per-controller keys
	};

	/** The LEDs that are standardized, assumably present on most controllers.
	The postfix numbering indicates the deck, goes from left to right, top to bottom (like English is written).
	"slKey" prefix indicates that the LED is lighting up a key on the controller. */
	enum StandardLed
	{
		slKeyPlay1,
		slKeyPause1,
		slKeyPlayPause1,
		slKeyCue1,
		slKeySync1,
		slKeyPfl1,
		slKeyGenericA1,
		slKeyGenericB1,
		slKeyGenericC1,
		slKeyGenericD1,
		slBeat1,
		slGenericA1,
		slGenericB1,
		slGenericC1,
		slGenericD1,

		slKeyPlay2,
		slKeyPause2,
		slKeyPlayPause2,
		slKeyCue2,
		slKeySync2,
		slKeyPfl2,
		slKeyGenericA2,
		slKeyGenericB2,
		slKeyGenericC2,
		slKeyGenericD2,
		slBeat2,
		slGenericA2,
		slGenericB2,
		slGenericC2,
		slGenericD2,

		slCustom = 1024,  ///< Start of custom per-controller LEDs
	};

	/** The sliders that are standardized, assumably present on most controllers.
	A slider always moves between its min and max value (absolute); compare to a wheel.
	The postfix numbering indicates the deck, goes from left to right, top to bottom (like English is written). */
	enum StandardSlider
	{
		ssMasterVolume,
		ssMicVolume,

		ssPitch1,
		ssVolume1,
		ssCrossfade1,  ///< Crossfade between deck 1 and 2

		ssPitch2,
		ssVolume2,
	};

	/** The ("jog") wheels that are standardized, assumably present on most controllers.
	A wheel can move indefinitely in either direction, it registers "number of steps taken in a direction" (relative);
	compare to a slider.
	The postfix numbering indicates the deck, goes from left to right, top to bottom (like English is written). */
	enum StandardWheel
	{
		swBrowse,
		swJog1,
		swJog2,
	};


	// Force a virtual destructor in descentants
	virtual ~AbstractController() {}

	/** Sends a ping to the controller
	Returns true if the controller is still present,false if it has been disconnected. */
	virtual bool ping() = 0;

	/** Sets the specified LED light to the specified state.
	aLed may be a StandardLed constant, or a controller-specific LED number. */
	virtual void setLed(int aLed, bool aOn) = 0;

	/** Returns the user-visible controller name. */
	virtual QString name() const = 0;


signals:

	/** The user pressed a key on the controller.
	aKey specifies the controller key, either a StandardKey value or a controller-specific key number. */
	void keyPressed(int aKey);

	/** The user moved a slider to the specified value.
	aSlider identifies the slider, either a StandardSlider value or a controller-specific slider number.
	aValue is normalized into the range 0 .. 1 */
	void sliderSet(int aSlider, qreal aValue);

	/** The user moved a (jog)wheel the specified number of steps.
	aWheel identifies the wheel, either a StandardWheel value or a controller-specific wheel number
	aNumSteps indicates the number of steps (controller chooses direction to be positive, the other negative). */
	void wheelMoved(int aWheel, int aNumSteps);

	/** The controller has been unplugged from the system. */
	void unplugged();
};

using AbstractControllerPtr = std::shared_ptr<AbstractController>;





#endif // ABSTRACTCONTROLLER_H

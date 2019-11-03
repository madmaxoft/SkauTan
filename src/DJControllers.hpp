#pragma once

#include <atomic>
#include <QThread>
#include <QTimer>
#include "ComponentCollection.hpp"
#include "Controller/AbstractController.hpp"





// fwd:
class MidiEnumerator;





/** A component that provides access to DJ controllers. */
class DJControllers:
	public QThread,
	public ComponentCollection::Component<ComponentCollection::ckDJControllers>
{
	using Super = QThread;
	Q_OBJECT


protected:

	/** Unregisters the specified key handler.
	For internal use, clients of this class will call this automatically via KeyHandlerReg's destructor. */
	void unregisterKeyHandler(quint64 aRegID);

	/** Unregisters the specified slider handler.
	For internal use, clients of this class will call this automatically via SliderHandlerReg's destructor. */
	void unregisterSliderHandler(quint64 aRegID);

	/** Unregisters the specified wheel handler.
	For internal use, clients of this class will call this automatically via WheelHandlerReg's destructor. */
	void unregisterWheelHandler(quint64 aRegID);


public:

	/** Wrapper for the handler function name and the object on which it should be invoked. */
	struct Handler
	{
		QObject * mDestinationObject;
		std::string mFunctionName;

		Handler(QObject * aDestinationObject, const char * aFunctionName):
			mDestinationObject(aDestinationObject),
			mFunctionName(aFunctionName)
		{
		}
	};


	/** RAII class for unregistering callbacks upon its destruction. */
	template <void (DJControllers::*UnregFn)(quint64)>
	class HandlerReg
	{
	public:

		/** Creates a new RAII wrapper tied to the specified registration ID. */
		HandlerReg(DJControllers & aParent, quint64 aRegID):
			mParent(aParent),
			mRegID(aRegID)
		{
		}

		/** Unregisters the wrapped handler. */
		~HandlerReg()
		{
			(mParent.*UnregFn)(mRegID);
		}


	protected:

		/** The DJControllers instance where the handler is registered. */
		DJControllers & mParent;

		/** The registration ID in mParent to remove upon destruction. */
		quint64 mRegID;
	};

	// Typedefs for easier usage:
	using KeyHandlerReg    = HandlerReg<&DJControllers::unregisterKeyHandler>;
	using SliderHandlerReg = HandlerReg<&DJControllers::unregisterSliderHandler>;
	using WheelHandlerReg  = HandlerReg<&DJControllers::unregisterWheelHandler>;
	using KeyHandlerRegPtr    = std::shared_ptr<KeyHandlerReg>;
	using SliderHandlerRegPtr = std::shared_ptr<SliderHandlerReg>;
	using WheelHandlerRegPtr  = std::shared_ptr<WheelHandlerReg>;


	/** The QObject's property used to store the context name. */
	static const char * CONTEXT_PROPERTY_NAME;


	/** Creates a new empty instance. */
	explicit DJControllers(QObject * aParent = nullptr);

	virtual ~DJControllers() override;

	/** Registers a handler for key events in the specified context.
	Returns a registration object that unregisters the callback when destroyed. */
	KeyHandlerRegPtr registerContextKeyHandler(
		const QString & aContext,
		QObject * aDestination,
		const char * aCallbackName
	);

	/** Registers a handler for slider events in the specified context. */
	SliderHandlerRegPtr registerContextSliderHandler(
		const QString & aContext,
		QObject * aDestination,
		const char * aCallbackName
	);

	/** Registers a handler for wheel events in the specified context. */
	WheelHandlerRegPtr registerContextWheelHandler(
		const QString & aContext,
		QObject * aDestination,
		const char * aCallbackName
	);


private:

	/** The timer used for periodically updating the port list and opening any new ports. */
	QTimer mTimerUpdate;

	/** Enumerator of the MIDI-based controllers. */
	std::unique_ptr<MidiEnumerator> mMidiEnumerator;

	/** Controllers currently connected and detected. */
	std::vector<std::shared_ptr<AbstractController>> mControllers;

	/** The RegID of the next key / slider / wheel registration. */
	std::atomic<quint64> mNextRegID;

	/** The registered key handlers. */
	std::vector<std::tuple<quint64, QString, Handler>> mKeyHandlers;

	/** The registered slider handlers. */
	std::vector<std::tuple<quint64, QString, Handler>> mSliderHandlers;

	/** The registered wheel handlers. */
	std::vector<std::tuple<quint64, QString, Handler>> mWheelHandlers;


	/** Sends a message to turn the specified LED on or off. */
	void setLed(int aLed, bool aLightUp);

	/** Returns the current context, based on the app's focused window and its parents. */
	QString findCurrentContext();


	// QThread overrides:
	virtual void run() override;


signals:

	/** The user connected a controller that was successfully detected. */
	void controllerConnected(const QString & aName);

	/** The user disconnected the controller. */
	void controllerRemoved();


private slots:

	/** Called periodically by a timer.
	Checks that the currently open port is still active, if not, tries to detect controllers on all ports. */
	void periodicUpdate();

	/** Called by the enumerators whenever a new controller is detected. */
	void newControllerDetected(std::shared_ptr<AbstractController> aController);

	/** Called by the enumerators whenever a controller is disconnected. */
	void controllerDisconnected(std::shared_ptr<AbstractController> aController);

	/** Called when the user presses a key on a controller. */
	void controllerKeyPressed(int aKey);

	/** Called when the user moves a slider on a controller. */
	void controllerSliderSet(int aSlider, qreal aValue);

	/** Called when the user moves a wheel on a controller. */
	void controllerWheelMoved(int aWheel, int aNumSteps);


public slots:

	/** Sets the state of the Play / Pause button's LED. */
	void setLedPlay(bool aLightUp);

	/** Sets the state of the Cure button's LED. */
	void setLedCue(bool aLightUp);

	/** Sets the state of the Sync button's LED. */
	void setLedSync(bool aLightUp);

	/** Sets the state of the Headphone button's LED. */
	void setLedPfl(bool aLightUp);
};

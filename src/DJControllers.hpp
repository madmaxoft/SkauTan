#ifndef DJCONTROLLERS_H
#define DJCONTROLLERS_H





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
	void unregisterKeyHandler(quint64 a_RegID);

	/** Unregisters the specified slider handler.
	For internal use, clients of this class will call this automatically via SliderHandlerReg's destructor. */
	void unregisterSliderHandler(quint64 a_RegID);

	/** Unregisters the specified wheel handler.
	For internal use, clients of this class will call this automatically via WheelHandlerReg's destructor. */
	void unregisterWheelHandler(quint64 a_RegID);


public:

	using KeyHandler = std::function<void(int a_Key)>;
	using SliderHandler = std::function<void(int a_Slider, qreal a_Value)>;
	using WheelHandler = std::function<void(int a_Wheel, int a_NumSteps)>;


	/** RAII class for unregistering callbacks upon its destruction. */
	template <void (DJControllers::*UnregFn)(quint64)>
	class HandlerReg
	{
	public:

		/** Creates a new RAII wrapper tied to the specified registration ID. */
		HandlerReg(DJControllers & a_Parent, quint64 a_RegID):
			m_Parent(a_Parent),
			m_RegID(a_RegID)
		{
		}

		/** Unregisters the wrapped handler. */
		~HandlerReg()
		{
			(m_Parent.*UnregFn)(m_RegID);
		}


	protected:

		/** The DJControllers instance where the handler is registered. */
		DJControllers & m_Parent;

		/** The registration ID in m_Parent to remove upon destruction. */
		quint64 m_RegID;
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
	explicit DJControllers(QObject * a_Parent = nullptr);

	virtual ~DJControllers() override;

	/** Registers a handler for key events in the specified context.
	Returns a registration object that unregisters the callback when destroyed. */
	KeyHandlerRegPtr registerContextKeyHandler(
		const QString & a_Context,
		KeyHandler a_Callback
	);

	/** Registers a handler for slider events in the specified context. */
	SliderHandlerRegPtr registerContextSliderHandler(
		const QString & a_Context,
		SliderHandler a_Callback
	);

	/** Registers a handler for wheel events in the specified context. */
	WheelHandlerRegPtr registerContextWheelHandler(
		const QString & a_Context,
		WheelHandler a_Callback
	);


private:

	/** The timer used for periodically updating the port list and opening any new ports. */
	QTimer m_TimerUpdate;

	/** Enumerator of the MIDI-based controllers. */
	std::unique_ptr<MidiEnumerator> m_MidiEnumerator;

	/** Controllers currently connected and detected. */
	std::vector<std::shared_ptr<AbstractController>> m_Controllers;

	/** The RegID of the next key / slider / wheel registration. */
	std::atomic<quint64> m_NextRegID;

	/** The registered key handlers. */
	std::vector<std::tuple<quint64, QString, KeyHandler>> m_KeyHandlers;

	/** The registered slider handlers. */
	std::vector<std::tuple<quint64, QString, SliderHandler>> m_SliderHandlers;

	/** The registered wheel handlers. */
	std::vector<std::tuple<quint64, QString, WheelHandler>> m_WheelHandlers;


	/** Sends a message to turn the specified LED on or off. */
	void setLed(int a_Led, bool a_LightUp);

	/** Returns the current context, based on the app's focused window and its parents. */
	QString findCurrentContext();


	// QThread overrides:
	virtual void run() override;


signals:

	/** The user connected a controller that was successfully detected. */
	void controllerConnected(const QString & a_Name);

	/** The user disconnected the controller. */
	void controllerRemoved();


private slots:

	/** Called periodically by a timer.
	Checks that the currently open port is still active, if not, tries to detect controllers on all ports. */
	void periodicUpdate();

	/** Called by the enumerators whenever a new controller is detected. */
	void newControllerDetected(std::shared_ptr<AbstractController> a_Controller);

	/** Called by the enumerators whenever a controller is disconnected. */
	void controllerDisconnected(std::shared_ptr<AbstractController> a_Controller);

	/** Called when the user presses a key on a controller. */
	void controllerKeyPressed(int a_Key);

	/** Called when the user moves a slider on a controller. */
	void controllerSliderSet(int a_Slider, qreal a_Value);

	/** Called when the user moves a wheel on a controller. */
	void controllerWheelMoved(int a_Wheel, int a_NumSteps);


public slots:

	/** Sets the state of the Play / Pause button's LED. */
	void setLedPlay(bool a_LightUp);

	/** Sets the state of the Cure button's LED. */
	void setLedCue(bool a_LightUp);

	/** Sets the state of the Sync button's LED. */
	void setLedSync(bool a_LightUp);

	/** Sets the state of the Headphone button's LED. */
	void setLedPfl(bool a_LightUp);
};





#endif // DJCONTROLLERS_H

#pragma once

#include <memory>
#include <vector>
#include <QObject>





// fwd:
class AbstractController;
using AbstractControllerPtr = std::shared_ptr<AbstractController>;





/** Represents the machinery to detect controllers using similar API.
Descendants may be implemented using RtMidi, HID or others. */
class AbstractEnumerator:
	public QObject
{
	using Super = QObject;
	Q_OBJECT


public:

	AbstractEnumerator(QObject * aParent): Super(aParent) {}

	// Force a virtual destructor:
	virtual ~AbstractEnumerator() {}

	/** Called periodically by the DJControllers class in a background thread.
	Used to poll for new devices etc. */
	virtual void periodicUpdate() = 0;

	/** Returns all the currently detected controllers. */
	virtual std::vector<AbstractControllerPtr> queryControllers() = 0;


signals:

	/** Emitted when a new controller is detected. */
	void newControllerDetected(std::shared_ptr<AbstractController> aController);

	/** Emitted when a controller is disconnected from the system. */
	void controllerDisconnected(std::shared_ptr<AbstractController> aController);


public slots:
};

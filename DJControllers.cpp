#include "DJControllers.h"
#include <QSemaphore>
#include <QDebug>
#include "Utils.h"
#include "Controller/MidiEnumerator.h"





////////////////////////////////////////////////////////////////////////////////
// DJControllers:

DJControllers::DJControllers(QObject * a_Parent):
	Super(a_Parent)
{
	QThread::setObjectName("DJControllers");
	moveToThread(this);
	QThread::start();

	connect(&m_TimerUpdate, &QTimer::timeout, this, &DJControllers::periodicUpdate);
	m_TimerUpdate.start(1000);
}





DJControllers::~DJControllers()
{
	QThread::quit();
	if (!QThread::wait(1000))
	{
		qWarning() << "Failed to stop the DJControllers thread, force-terminating.";
	}
}





void DJControllers::setLed(int a_Led, bool a_LightUp)
{
	for (auto & c: m_Controllers)
	{
		c->setLed(a_Led, a_LightUp);
	}
}





void DJControllers::run()
{
	// Initialize the enumerators:
	m_MidiEnumerator.reset(new MidiEnumerator(this));
	connect(m_MidiEnumerator.get(), &MidiEnumerator::newControllerDetected,  this, &DJControllers::newControllerDetected);
	connect(m_MidiEnumerator.get(), &MidiEnumerator::controllerDisconnected, this, &DJControllers::controllerDisconnected);

	// Scan for controllers:
	m_MidiEnumerator->rescanPorts();
	// QUEUED: QMetaObject::invokeMethod(m_MidiEnumerator.get(), "rescanPorts", Qt::QueuedConnection);

	// Run the event loop:
	exec();

	// Delete the enumerators:
	m_MidiEnumerator.reset();
}





void DJControllers::periodicUpdate()
{
	m_MidiEnumerator->periodicUpdate();
}





void DJControllers::newControllerDetected(std::shared_ptr<AbstractController> a_Controller)
{
	qDebug() << "New controller detected: " << a_Controller->name();
	m_Controllers.push_back(a_Controller);
	connect(a_Controller.get(), &AbstractController::keyPressed, this, &DJControllers::controllerKeyPressed);
	connect(a_Controller.get(), &AbstractController::sliderSet,  this, &DJControllers::controllerSliderSet);
	connect(a_Controller.get(), &AbstractController::wheelMoved, this, &DJControllers::controllerWheelMoved);
	emit controllerConnected(a_Controller->name());
}





void DJControllers::controllerDisconnected(std::shared_ptr<AbstractController> a_Controller)
{
	qDebug() << "Controller was disconnected: " << a_Controller->name();
	m_Controllers.erase(std::remove_if(m_Controllers.begin(), m_Controllers.end(),
		[&a_Controller](std::shared_ptr<AbstractController> a_ContainedController)
		{
			return (a_ContainedController == a_Controller);
		}
	));
	a_Controller->disconnect(this);
}





void DJControllers::controllerKeyPressed(int a_Key)
{
	switch (a_Key)
	{
		case AbstractController::skPlayPause1:
		{
			emit playPause();
			return;
		}
		case AbstractController::skPitchPlus1:
		{
			emit pitchPlus();
			return;
		}
		case AbstractController::skPitchMinus1:
		{
			emit pitchMinus();
			return;
		}
		// TODO: Other keys
	}
	qDebug() << "Unhandled controller key: " << a_Key;
}





void DJControllers::controllerSliderSet(int a_Slider, qreal a_Value)
{
	switch (a_Slider)
	{
		case AbstractController::ssMasterVolume:
		{
			emit setVolume(a_Value);
			return;
		}
		case AbstractController::ssPitch1:
		{
			emit setTempoCoeff(a_Value);
			return;
		}
		// TODO: Other sliders
	}
	qDebug() << "Unhandled controller slider: " << a_Slider;
}





void DJControllers::controllerWheelMoved(int a_Wheel, int a_NumSteps)
{
	switch (a_Wheel)
	{
		case AbstractController::swBrowse:
		{
			if (a_NumSteps < 0)
			{
				emit navigateUp();
			}
			else
			{
				emit navigateDown();
			}
			return;
		}
		// TODO: Other wheels
	}
	qDebug() << "Unhandled controller wheel: " << a_Wheel;
}





void DJControllers::setLedPlay(bool a_LightUp)
{
	setLed(AbstractController::slKeyPlayPause1, a_LightUp);
}





void DJControllers::setLedCue(bool a_LightUp)
{
	setLed(AbstractController::slKeyCue1, a_LightUp);
}





void DJControllers::setLedSync(bool a_LightUp)
{
	setLed(AbstractController::slKeySync1, a_LightUp);
}





void DJControllers::setLedPfl(bool a_LightUp)
{
	setLed(AbstractController::slKeyPfl1, a_LightUp);
}

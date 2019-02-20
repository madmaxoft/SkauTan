#include "DJControllers.hpp"
#include <QSemaphore>
#include <QDebug>
#include <QGuiApplication>
#include <QWidget>
#include "Utils.hpp"
#include "Controller/MidiEnumerator.hpp"





////////////////////////////////////////////////////////////////////////////////
// DJControllers:

const char * DJControllers::CONTEXT_PROPERTY_NAME = "SkauTan_DJControllers_Context";





DJControllers::DJControllers(QObject * a_Parent):
	Super(a_Parent),
	m_NextRegID(0)
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





void DJControllers::unregisterKeyHandler(quint64 a_RegID)
{
	for (auto itr = m_KeyHandlers.begin(), end = m_KeyHandlers.end(); itr != end; ++itr)
	{
		if (std::get<0>(*itr) == a_RegID)
		{
			m_KeyHandlers.erase(itr);
			return;
		}
	}
}





void DJControllers::unregisterSliderHandler(quint64 a_RegID)
{
	for (auto itr = m_SliderHandlers.begin(), end = m_SliderHandlers.end(); itr != end; ++itr)
	{
		if (std::get<0>(*itr) == a_RegID)
		{
			m_SliderHandlers.erase(itr);
			return;
		}
	}
}





void DJControllers::unregisterWheelHandler(quint64 a_RegID)
{
	for (auto itr = m_WheelHandlers.begin(), end = m_WheelHandlers.end(); itr != end; ++itr)
	{
		if (std::get<0>(*itr) == a_RegID)
		{
			m_WheelHandlers.erase(itr);
			return;
		}
	}
}





DJControllers::KeyHandlerRegPtr DJControllers::registerContextKeyHandler(
	const QString & a_Context,
	QObject * a_DestinationObject,
	const char * a_CallbackName
)
{
	auto regID = m_NextRegID++;
	m_KeyHandlers.push_back(
		std::make_tuple(
			regID, a_Context,
			Handler(a_DestinationObject, a_CallbackName)
		)
	);
	return std::make_shared<KeyHandlerReg>(*this, regID);
}





DJControllers::SliderHandlerRegPtr DJControllers::registerContextSliderHandler(
	const QString & a_Context,
	QObject * a_DestinationObject,
	const char * a_CallbackName
)
{
	auto regID = m_NextRegID++;
	m_SliderHandlers.push_back(
		std::make_tuple(
			regID, a_Context,
			Handler(a_DestinationObject, a_CallbackName)
		)
	);
	return std::make_shared<SliderHandlerReg>(*this, regID);
}





DJControllers::WheelHandlerRegPtr DJControllers::registerContextWheelHandler(
	const QString & a_Context,
	QObject * a_DestinationObject,
	const char * a_CallbackName
)
{
	auto regID = m_NextRegID++;
	m_WheelHandlers.push_back(
		std::make_tuple(
			regID, a_Context,
			Handler(a_DestinationObject, a_CallbackName)
		)
	);
	return std::make_shared<WheelHandlerReg>(*this, regID);
}





void DJControllers::setLed(int a_Led, bool a_LightUp)
{
	for (auto & c: m_Controllers)
	{
		c->setLed(a_Led, a_LightUp);
	}
}





QString DJControllers::findCurrentContext()
{
	auto focused = qApp->focusObject();
	QString wholeContext;
	while (focused != nullptr)
	{
		auto context = focused->property(CONTEXT_PROPERTY_NAME);
		if (context.isValid() && !context.toString().isEmpty())
		{
			wholeContext = "." + context.toString();
		}
		auto w = dynamic_cast<QWidget *>(focused);
		if (w != nullptr)
		{
			focused = w->parentWidget();
		}
		else
		{
			focused = focused->parent();
		}
	}
	if (wholeContext.isEmpty())
	{
		return QString();
	}
	wholeContext.remove(0, 1);  // Remove the initial dot
	return wholeContext;
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
	// Notify all handlers whose context matches the current one:
	auto context = findCurrentContext();
	for (const auto & handler: m_KeyHandlers)
	{
		if (std::get<1>(handler) == context)
		{
			auto & reg = std::get<2>(handler);
			QMetaObject::invokeMethod(
				reg.m_DestinationObject,
				reg.m_FunctionName.c_str(),
				Qt::QueuedConnection,
				Q_ARG(int, a_Key)
			);
		}
	}
}





void DJControllers::controllerSliderSet(int a_Slider, qreal a_Value)
{
	// Notify all handlers whose context matches the current one:
	auto context = findCurrentContext();
	for (const auto & handler: m_SliderHandlers)
	{
		if (std::get<1>(handler) == context)
		{
			auto & reg = std::get<2>(handler);
			QMetaObject::invokeMethod(
				reg.m_DestinationObject,
				reg.m_FunctionName.c_str(),
				Qt::QueuedConnection,
				Q_ARG(int, a_Slider),
				Q_ARG(qreal, a_Value)
			);
		}
	}
}





void DJControllers::controllerWheelMoved(int a_Wheel, int a_NumSteps)
{
	// Notify all handlers whose context matches the current one:
	auto context = findCurrentContext();
	for (const auto & handler: m_WheelHandlers)
	{
		if (std::get<1>(handler) == context)
		{
			auto & reg = std::get<2>(handler);
			QMetaObject::invokeMethod(
				reg.m_DestinationObject,
				reg.m_FunctionName.c_str(),
				Qt::QueuedConnection,
				Q_ARG(int, a_Wheel),
				Q_ARG(int, a_NumSteps)
			);
		}
	}
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

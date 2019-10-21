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





DJControllers::DJControllers(QObject * aParent):
	Super(aParent),
	mNextRegID(0)
{
	QThread::setObjectName("DJControllers");
	moveToThread(this);
	QThread::start();

	connect(&mTimerUpdate, &QTimer::timeout, this, &DJControllers::periodicUpdate);
	mTimerUpdate.start(1000);
}





DJControllers::~DJControllers()
{
	QThread::quit();
	if (!QThread::wait(1000))
	{
		qWarning() << "Failed to stop the DJControllers thread, force-terminating.";
	}
}





void DJControllers::unregisterKeyHandler(quint64 aRegID)
{
	for (auto itr = mKeyHandlers.begin(), end = mKeyHandlers.end(); itr != end; ++itr)
	{
		if (std::get<0>(*itr) == aRegID)
		{
			mKeyHandlers.erase(itr);
			return;
		}
	}
}





void DJControllers::unregisterSliderHandler(quint64 aRegID)
{
	for (auto itr = mSliderHandlers.begin(), end = mSliderHandlers.end(); itr != end; ++itr)
	{
		if (std::get<0>(*itr) == aRegID)
		{
			mSliderHandlers.erase(itr);
			return;
		}
	}
}





void DJControllers::unregisterWheelHandler(quint64 aRegID)
{
	for (auto itr = mWheelHandlers.begin(), end = mWheelHandlers.end(); itr != end; ++itr)
	{
		if (std::get<0>(*itr) == aRegID)
		{
			mWheelHandlers.erase(itr);
			return;
		}
	}
}





DJControllers::KeyHandlerRegPtr DJControllers::registerContextKeyHandler(
	const QString & aContext,
	QObject * aDestinationObject,
	const char * aCallbackName
)
{
	auto regID = mNextRegID++;
	mKeyHandlers.push_back(
		std::make_tuple(
			regID, aContext,
			Handler(aDestinationObject, aCallbackName)
		)
	);
	return std::make_shared<KeyHandlerReg>(*this, regID);
}





DJControllers::SliderHandlerRegPtr DJControllers::registerContextSliderHandler(
	const QString & aContext,
	QObject * aDestinationObject,
	const char * aCallbackName
)
{
	auto regID = mNextRegID++;
	mSliderHandlers.push_back(
		std::make_tuple(
			regID, aContext,
			Handler(aDestinationObject, aCallbackName)
		)
	);
	return std::make_shared<SliderHandlerReg>(*this, regID);
}





DJControllers::WheelHandlerRegPtr DJControllers::registerContextWheelHandler(
	const QString & aContext,
	QObject * aDestinationObject,
	const char * aCallbackName
)
{
	auto regID = mNextRegID++;
	mWheelHandlers.push_back(
		std::make_tuple(
			regID, aContext,
			Handler(aDestinationObject, aCallbackName)
		)
	);
	return std::make_shared<WheelHandlerReg>(*this, regID);
}





void DJControllers::setLed(int aLed, bool aLightUp)
{
	for (auto & c: mControllers)
	{
		c->setLed(aLed, aLightUp);
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
	mMidiEnumerator.reset(new MidiEnumerator(this));
	connect(mMidiEnumerator.get(), &MidiEnumerator::newControllerDetected,  this, &DJControllers::newControllerDetected);
	connect(mMidiEnumerator.get(), &MidiEnumerator::controllerDisconnected, this, &DJControllers::controllerDisconnected);

	// Scan for controllers:
	mMidiEnumerator->rescanPorts();
	// QUEUED: QMetaObject::invokeMethod(mMidiEnumerator.get(), "rescanPorts", Qt::QueuedConnection);

	// Run the event loop:
	exec();

	// Delete the enumerators:
	mMidiEnumerator.reset();
}





void DJControllers::periodicUpdate()
{
	mMidiEnumerator->periodicUpdate();
}





void DJControllers::newControllerDetected(std::shared_ptr<AbstractController> aController)
{
	qDebug() << "New controller detected: " << aController->name();
	mControllers.push_back(aController);
	connect(aController.get(), &AbstractController::keyPressed, this, &DJControllers::controllerKeyPressed);
	connect(aController.get(), &AbstractController::sliderSet,  this, &DJControllers::controllerSliderSet);
	connect(aController.get(), &AbstractController::wheelMoved, this, &DJControllers::controllerWheelMoved);
	emit controllerConnected(aController->name());
}





void DJControllers::controllerDisconnected(std::shared_ptr<AbstractController> aController)
{
	qDebug() << "Controller was disconnected: " << aController->name();
	mControllers.erase(std::remove_if(mControllers.begin(), mControllers.end(),
		[&aController](std::shared_ptr<AbstractController> aContainedController)
		{
			return (aContainedController == aController);
		}
	));
	aController->disconnect(this);
}





void DJControllers::controllerKeyPressed(int aKey)
{
	// Notify all handlers whose context matches the current one:
	auto context = findCurrentContext();
	for (const auto & handler: mKeyHandlers)
	{
		if (std::get<1>(handler) == context)
		{
			auto & reg = std::get<2>(handler);
			QMetaObject::invokeMethod(
				reg.mDestinationObject,
				reg.mFunctionName.c_str(),
				Qt::QueuedConnection,
				Q_ARG(int, aKey)
			);
		}
	}
}





void DJControllers::controllerSliderSet(int aSlider, qreal aValue)
{
	// Notify all handlers whose context matches the current one:
	auto context = findCurrentContext();
	for (const auto & handler: mSliderHandlers)
	{
		if (std::get<1>(handler) == context)
		{
			auto & reg = std::get<2>(handler);
			QMetaObject::invokeMethod(
				reg.mDestinationObject,
				reg.mFunctionName.c_str(),
				Qt::QueuedConnection,
				Q_ARG(int, aSlider),
				Q_ARG(qreal, aValue)
			);
		}
	}
}





void DJControllers::controllerWheelMoved(int aWheel, int aNumSteps)
{
	// Notify all handlers whose context matches the current one:
	auto context = findCurrentContext();
	for (const auto & handler: mWheelHandlers)
	{
		if (std::get<1>(handler) == context)
		{
			auto & reg = std::get<2>(handler);
			QMetaObject::invokeMethod(
				reg.mDestinationObject,
				reg.mFunctionName.c_str(),
				Qt::QueuedConnection,
				Q_ARG(int, aWheel),
				Q_ARG(int, aNumSteps)
			);
		}
	}
}





void DJControllers::setLedPlay(bool aLightUp)
{
	setLed(AbstractController::slKeyPlayPause1, aLightUp);
}





void DJControllers::setLedCue(bool aLightUp)
{
	setLed(AbstractController::slKeyCue1, aLightUp);
}





void DJControllers::setLedSync(bool aLightUp)
{
	setLed(AbstractController::slKeySync1, aLightUp);
}





void DJControllers::setLedPfl(bool aLightUp)
{
	setLed(AbstractController::slKeyPfl1, aLightUp);
}

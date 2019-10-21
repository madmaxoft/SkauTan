#include "DebugLogger.hpp"
#include <QDebug>





/** The previous message handler. */
static QtMessageHandler g_OldHandler;





DebugLogger::DebugLogger()
{
	g_OldHandler = qInstallMessageHandler(messageHandler);
}





DebugLogger & DebugLogger::get()
{
	static DebugLogger inst;
	return inst;
}





std::vector<DebugLogger::Message> DebugLogger::lastMessages() const
{
	QMutexLocker lock(&mMtx);
	std::vector<Message> res;
	size_t max = sizeof(mMessages) / sizeof(mMessages[0]);
	res.reserve(max);
	for (size_t i = 0; i < max; ++i)
	{
		size_t idx = (i + mNextMessageIdx) % max;
		if (mMessages[idx].mDateTime.isValid())
		{
			res.push_back(mMessages[idx]);
		}
	}
	return res;
}





void DebugLogger::messageHandler(
	QtMsgType aType,
	const QMessageLogContext & aContext,
	const QString & aMessage
)
{
	DebugLogger::get().addMessage(aType, aContext, aMessage);
	g_OldHandler(aType, aContext, aMessage);
}





void DebugLogger::addMessage(
	QtMsgType aType,
	const QMessageLogContext & aContext,
	const QString & aMessage
)
{
	QMutexLocker lock(&mMtx);
	mMessages[mNextMessageIdx] = Message(aType, aContext, aMessage);
	mNextMessageIdx = (mNextMessageIdx + 1) % (sizeof(mMessages) / sizeof(mMessages[0]));
}

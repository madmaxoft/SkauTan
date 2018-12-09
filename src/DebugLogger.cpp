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
	QMutexLocker lock(&m_Mtx);
	std::vector<Message> res;
	size_t max = sizeof(m_Messages) / sizeof(m_Messages[0]);
	res.reserve(max);
	for (size_t i = 0; i < max; ++i)
	{
		size_t idx = (i + m_NextMessageIdx) % max;
		if (m_Messages[idx].m_DateTime.isValid())
		{
			res.push_back(m_Messages[idx]);
		}
	}
	return res;
}





void DebugLogger::messageHandler(
	QtMsgType a_Type,
	const QMessageLogContext & a_Context,
	const QString & a_Message
)
{
	DebugLogger::get().addMessage(a_Type, a_Context, a_Message);
	g_OldHandler(a_Type, a_Context, a_Message);
}





void DebugLogger::addMessage(
	QtMsgType a_Type,
	const QMessageLogContext & a_Context,
	const QString & a_Message
)
{
	QMutexLocker lock(&m_Mtx);
	m_Messages[m_NextMessageIdx] = Message(a_Type, a_Context, a_Message);
	m_NextMessageIdx = (m_NextMessageIdx + 1) % (sizeof(m_Messages) / sizeof(m_Messages[0]));
}

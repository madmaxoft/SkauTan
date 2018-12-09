#ifndef DEBUGLOGGER_HPP
#define DEBUGLOGGER_HPP





#include <QObject>
#include <QMutex>
#include <QDateTime>





class DebugLogger:
	public QObject
{
	using Super = QObject;

	Q_OBJECT


public:

	/** Container for a single debug message. */
	struct Message
	{
		QDateTime m_DateTime;
		QtMsgType m_Type;
		QString m_FileName;
		QString m_Function;
		int m_LineNum;
		QString m_Message;


		/** Default constructor, used for array initialization.
		The m_DateTime is not assigned -> it will not be included in lastMessages() output. */
		Message() = default;


		/** Full constructor, used for copying. */
		Message(
			QtMsgType a_Type,
			const QMessageLogContext & a_Context,
			const QString & a_Message
		):
			m_DateTime(QDateTime::currentDateTimeUtc()),
			m_Type(a_Type),
			m_FileName(QString::fromUtf8(a_Context.file)),
			m_Function(QString::fromUtf8(a_Context.function)),
			m_LineNum(a_Context.line),
			m_Message(a_Message)
		{
		}
	};


	/** Returns the only instance of this object. */
	static DebugLogger & get();

	/** Returns the (valid) messages stored in the logger. */
	std::vector<Message> lastMessages() const;


signals:


public slots:


protected:

	/** The maximum count of messages remembered. */
	static const size_t MAX_MESSAGES = 1024;

	/** The last messages sent to the logger.
	Protected by m_Mtx against multithreaded access.
	Works as a circular buffer, with m_NextMessageIdx pointing to the index where the next message will be written. */
	Message m_Messages[MAX_MESSAGES];

	/** Index into m_Messages where the next message will be written.
	Protected by m_Mtx against multithreaded access. */
	size_t m_NextMessageIdx;

	/** The mutex protecting m_Messages and m_NextMessageIdx from multithreaded access. */
	mutable QMutex m_Mtx;


	/** Constructor not accessible to the outside -> singleton. */
	DebugLogger();

	/** The override message handler that stores messages in a DebugLogger. */
	static void messageHandler(
		QtMsgType a_Type,
		const QMessageLogContext & a_Context,
		const QString & a_Message
	);

	/** Adds the specified message to m_Messages[]. */
	void addMessage(
		QtMsgType a_Type,
		const QMessageLogContext & a_Context,
		const QString & a_Message
	);
};





#endif // DEBUGLOGGER_HPP

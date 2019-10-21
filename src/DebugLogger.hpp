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
		QDateTime mDateTime;
		QtMsgType mType;
		QString mFileName;
		QString mFunction;
		int mLineNum;
		QString mMessage;


		/** Default constructor, used for array initialization.
		The mDateTime is not assigned -> it will not be included in lastMessages() output. */
		Message() = default;


		/** Full constructor, used for copying. */
		Message(
			QtMsgType aType,
			const QMessageLogContext & aContext,
			const QString & aMessage
		):
			mDateTime(QDateTime::currentDateTimeUtc()),
			mType(aType),
			mFileName(QString::fromUtf8(aContext.file)),
			mFunction(QString::fromUtf8(aContext.function)),
			mLineNum(aContext.line),
			mMessage(aMessage)
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
	Protected by mMtx against multithreaded access.
	Works as a circular buffer, with mNextMessageIdx pointing to the index where the next message will be written. */
	Message mMessages[MAX_MESSAGES];

	/** Index into mMessages where the next message will be written.
	Protected by mMtx against multithreaded access. */
	size_t mNextMessageIdx;

	/** The mutex protecting mMessages and mNextMessageIdx from multithreaded access. */
	mutable QMutex mMtx;


	/** Constructor not accessible to the outside -> singleton. */
	DebugLogger();

	/** The override message handler that stores messages in a DebugLogger. */
	static void messageHandler(
		QtMsgType aType,
		const QMessageLogContext & aContext,
		const QString & aMessage
	);

	/** Adds the specified message to mMessages[]. */
	void addMessage(
		QtMsgType aType,
		const QMessageLogContext & aContext,
		const QString & aMessage
	);
};





#endif // DEBUGLOGGER_HPP

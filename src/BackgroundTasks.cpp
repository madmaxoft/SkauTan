#include "BackgroundTasks.hpp"
#include <QDebug>





////////////////////////////////////////////////////////////////////////////////
// BackgroundTasks:

Q_DECLARE_METATYPE(BackgroundTasks::TaskPtr);

BackgroundTasks::BackgroundTasks()
{
	qRegisterMetaType<BackgroundTasks::TaskPtr>();
	for (int i = QThread::idealThreadCount(); i > 0; --i)
	{
		auto executor = std::make_unique<Executor>(*this);
		auto executorPtr = executor.get();
		executor->setObjectName(QString("BackgroundTasks::Executor::%1").arg(i));
		m_Executors.push_back(std::move(executor));
		executorPtr->start();
	}
}





BackgroundTasks::~BackgroundTasks()
{
	// Tell all executors to terminate:
	qDebug() << "Terminating all executors...";
	m_ShouldTerminate = true;
	m_WaitForTasks.wakeAll();

	// Wait for all executors to terminate:
	qDebug() << "Waiting for all executors...";
	for (auto & e: m_Executors)
	{
		e->wait();
	}

	// Abort all tasks left over in the queue:
	qDebug() << "Aborting non-executed tasks.";
	for (auto & t: m_Tasks)
	{
		t->abort();
		emit taskAborted(t);
	}
}





BackgroundTasks & BackgroundTasks::get()
{
	static BackgroundTasks instance;
	return instance;
}





void BackgroundTasks::addTask(TaskPtr a_Task, bool a_Prioritize)
{
	{
		QMutexLocker lock(&m_Mtx);
		if (a_Prioritize)
		{
			m_Tasks.push_front(a_Task);
		}
		else
		{
			m_Tasks.push_back(a_Task);
		}
	}
	emit taskAdded(a_Task);
	m_WaitForTasks.wakeOne();
}





void BackgroundTasks::enqueue(
	const QString & a_TaskName,
	std::function<void ()> a_Task,
	bool a_Prioritize,
	std::function<void ()> a_OnAbort
)
{
	/** Adapter between a Task class and two functions. */
	class FunctionTask: public BackgroundTasks::Task
	{
		using Super = BackgroundTasks::Task;

	public:

		FunctionTask(const QString & a_FnName, std::function<void ()> a_FnTask, std::function<void ()> a_FnOnAbort):
			Super(a_FnName),
			m_Task(a_FnTask),
			m_OnAbort(a_FnOnAbort)
		{
		}

		virtual void execute() override
		{
			m_Task();
		}

		virtual void abort() override
		{
			m_OnAbort();
			Super::abort();
		}

	protected:
		std::function<void ()> m_Task;
		std::function<void ()> m_OnAbort;
	};

	// Enqueue the task adapter:
	BackgroundTasks::get().addTask(std::make_shared<FunctionTask>(a_TaskName, a_Task, a_OnAbort), a_Prioritize);
}





const std::list<BackgroundTasks::TaskPtr> BackgroundTasks::tasks() const
{
	QMutexLocker lock(&m_Mtx);
	std::list<TaskPtr> res(m_Tasks);
	return res;
}





BackgroundTasks::TaskPtr BackgroundTasks::getNextTask()
{
	QMutexLocker lock(&m_Mtx);
	if (m_ShouldTerminate)
	{
		return nullptr;
	}
	while (m_Tasks.empty())
	{
		if (m_ShouldTerminate)
		{
			return nullptr;
		}
		m_WaitForTasks.wait(&m_Mtx);
	}
	auto task = m_Tasks.front();
	m_Tasks.pop_front();
	return task;
}





void BackgroundTasks::emitTaskFinished(BackgroundTasks::TaskPtr a_Task)
{
	emit taskFinished(a_Task);
}





////////////////////////////////////////////////////////////////////////////////
// BackgroundTasks::Executor:

BackgroundTasks::Executor::Executor(BackgroundTasks & a_Parent):
	m_Parent(a_Parent)
{
}




void BackgroundTasks::Executor::run()
{
	auto task = m_Parent.getNextTask();
	while (task != nullptr)
	{
		task->execute();
		QMetaObject::invokeMethod(
			&m_Parent,
			"emitTaskFinished",
			Qt::QueuedConnection,
			Q_ARG(BackgroundTasks::TaskPtr, task)
		);
		task = m_Parent.getNextTask();
	}
}





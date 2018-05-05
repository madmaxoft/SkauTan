#include "BackgroundTasks.h"





////////////////////////////////////////////////////////////////////////////////
// BackgroundTasks:

BackgroundTasks::BackgroundTasks()
{
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
	m_ShouldTerminate = true;
	for (auto & e: m_Executors)
	{
		e->terminate();
	}

	// Wait for all executors to terminate:
	for (auto & e: m_Executors)
	{
		e->wait();
	}

	// Abort all tasks left over in the queue:
	for (auto & t: m_Tasks)
	{
		t->abort();
	}
}





BackgroundTasks & BackgroundTasks::get()
{
	static BackgroundTasks instance;
	return instance;
}





void BackgroundTasks::addTask(TaskPtr a_Task)
{
	QMutexLocker lock(&m_Mtx);
	m_Tasks.push_back(std::move(a_Task));
	m_WaitForTasks.wakeOne();
}





void BackgroundTasks::enqueue(std::function<void ()> a_Task, std::function<void ()> a_OnAbort)
{
	/** Adapter between a Task class and two functions. */
	class FunctionTask: public BackgroundTasks::Task
	{
		using Super = BackgroundTasks::Task;

	public:

		FunctionTask(std::function<void ()> a_FnTask, std::function<void ()> a_FnOnAbort):
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
	BackgroundTasks::get().addTask(std::make_shared<FunctionTask>(a_Task, a_OnAbort));
}





BackgroundTasks::TaskPtr BackgroundTasks::getNextTask()
{
	QMutexLocker lock(&m_Mtx);
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
		task = m_Parent.getNextTask();
	}
}





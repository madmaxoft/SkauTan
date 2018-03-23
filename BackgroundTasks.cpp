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
	m_WaitForTasks.notify_one();
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





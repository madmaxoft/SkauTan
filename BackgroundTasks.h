#ifndef BACKGROUNDTASKS_H
#define BACKGROUNDTASKS_H






#include <memory>
#include <vector>
#include <list>
#include <atomic>
#include <functional>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>





/** Provides the back-end for processing things in the background.
To add a background task, create a new subclass of BackgroundTasks::Task, implements its execute() method
and add an instance of it through addTask(). The BackgroundTasks instance will take care of scheduling
the task when ready.
Alternatively, use the BackgroundTasks::enqueue() function to put a function into the queue. */
class BackgroundTasks
{
public:

	/** Base class for code that needs to be executed in the background.
	Users should subclass and provide the execute() method implementation. */
	class Task
	{
	public:
		Task() : m_ShouldTerminate(false) {}

		// Force a virtual destructor
		virtual ~Task() {}

		/** Runs the actual code for this task.
		Called from within an Executor in a background thread. */
		virtual void execute() = 0;

		/** Called by the Executor if the entire BackgroundTasks instance is being destroyed.
		The task should terminate as soon as possible.
		The default implementation sets a flag that can be checked periodically. */
		virtual void abort() { m_ShouldTerminate = true; }

	protected:

		/** Flag that is set when the task should terminate as soon as possible. */
		std::atomic<bool> m_ShouldTerminate;
	};

	using TaskPtr = std::shared_ptr<Task>;


	/** Returns the singleton instance. */
	static BackgroundTasks & get();

	/** Adds the specified task to the internal list of tasks to run.
	The task is then run when an executor becomes free. */
	void addTask(TaskPtr a_Task);

	/** Adds a new task to the queue that executes the specified function.
	a_OnAbort is called if the task is to be aborted (even before it starts). */
	static void enqueue(std::function<void()> a_Task, std::function<void()> a_OnAbort = [](){});


protected:

	/** Represents a single thread that can execute tasks sequentially. */
	class Executor: public QThread
	{
	public:
		/** Creates a new executor that executes tasks of the specified BackgroundTasks. */
		Executor(BackgroundTasks & a_Parent);

		void run();

	protected:
		BackgroundTasks & m_Parent;
	};

	using ExecutorUPtr = std::unique_ptr<Executor>;



	/** The mutex protecting m_Tasks against multithreaded access. */
	QMutex m_Mtx;

	QWaitCondition m_WaitForTasks;

	/** The queue of tasks to be executed.
	Protected against multithreaded access by m_Mtx. */
	std::list<TaskPtr> m_Tasks;

	/** The threads that can execute tasks. */
	std::vector<ExecutorUPtr> m_Executors;

	/** Flag that is set when the entire background processing should terminate as soon as possible. */
	std::atomic<bool> m_ShouldTerminate;



	BackgroundTasks();

	~BackgroundTasks();

	/** Returns the next task from m_Tasks to execute.
	Waits for a task to become available (or the instance shutdown).
	Returns nullptr if instance is shutting down before a task is ready. */
	TaskPtr getNextTask();
};





#endif // BACKGROUNDTASKS_H

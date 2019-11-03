#pragma once

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
class BackgroundTasks:
	public QObject
{
	using Super = QObject;

	Q_OBJECT


public:

	/** Base class for code that needs to be executed in the background.
	Users should subclass and provide the execute() method implementation. */
	class Task;
	using TaskPtr = std::shared_ptr<Task>;


	/** Returns the singleton instance. */
	static BackgroundTasks & get();

	/** Adds the specified task to the internal list of tasks to run.
	If aPrioritize is true, adds the task to the front of the queue (executed asap),
	otherwise adds to the end of the queue (executed last).
	The task is then run when an executor becomes free. */
	void addTask(TaskPtr aTask, bool aPrioritize = false);

	/** Adds a new task to the queue that executes the specified function.
	If aPrioritize is true, adds the task to the front of the queue (executed asap),
	otherwise adds to the end of the queue (executed last).
	aOnAbort is called if the task is to be aborted (even before it starts). */
	static void enqueue(
		const QString & aTaskName,
		std::function<void()> aTask,
		bool aPrioritize = false,
		std::function<void()> aOnAbort = [](){}
	);

	const std::list<TaskPtr> tasks() const;

	/** Aborts all tasks that haven't started yet, and waits for all current tasks to finish.
	This is to be called before program shutdown so that tasks terminate in a defined way. */
	void stopAll();


protected:

	/** Represents a single thread that can execute tasks sequentially. */
	class Executor: public QThread
	{
	public:
		/** Creates a new executor that executes tasks of the specified BackgroundTasks. */
		Executor(BackgroundTasks & aParent);

		void run();

	protected:
		BackgroundTasks & mParent;
	};

	using ExecutorUPtr = std::unique_ptr<Executor>;



	/** The mutex protecting mTasks against multithreaded access. */
	mutable QMutex mMtx;

	QWaitCondition mWaitForTasks;

	/** The queue of tasks to be executed.
	Protected against multithreaded access by mMtx. */
	std::list<TaskPtr> mTasks;

	/** The threads that can execute tasks. */
	std::vector<ExecutorUPtr> mExecutors;

	/** Flag that is set when the entire background processing should terminate as soon as possible. */
	std::atomic<bool> mShouldTerminate;



	BackgroundTasks();

	~BackgroundTasks();

	/** Returns the next task from mTasks to execute.
	Waits for a task to become available (or the instance shutdown).
	Returns nullptr if instance is shutting down before a task is ready. */
	TaskPtr getNextTask();


signals:

	/** Emitted after the task is added. */
	void taskAdded(BackgroundTasks::TaskPtr aTask);

	/** Emitted after the task is finished and removed from the task list. */
	void taskFinished(BackgroundTasks::TaskPtr aTask);

	/** Emitted when a task is aborted. */
	void taskAborted(BackgroundTasks::TaskPtr aTask);


private slots:

	/** Received from the executors' threads, invokes the taskFinished() signal for the specified task. */
	void emitTaskFinished(BackgroundTasks::TaskPtr aTask);
};





class BackgroundTasks::Task:
	public QObject
{
	Q_OBJECT

public:

	Task(const QString & aName) : mName(aName), mShouldTerminate(false) {}

	// Force a virtual destructor
	virtual ~Task() {}

	/** Runs the actual code for this task.
	Called from within an Executor in a background thread. */
	virtual void execute() = 0;

	/** Called by the Executor if the entire BackgroundTasks instance is being destroyed.
	The task should terminate as soon as possible.
	The default implementation sets a flag that can be checked periodically. */
	virtual void abort() { mShouldTerminate = true; }

	/** Returns the user-visible task name. */
	const QString & name() const { return mName; }

protected:

	/** The name of the task, as shown to the user. */
	const QString mName;

	/** Flag that is set when the task should terminate as soon as possible. */
	std::atomic<bool> mShouldTerminate;
};

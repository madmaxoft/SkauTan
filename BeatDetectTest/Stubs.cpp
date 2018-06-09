#include "../BackgroundTasks.h"





void BackgroundTasks::enqueue(const QString & a_Name, std::function<void(void)> a_TaskFn, std::function<void(void)> a_AbortFn)
{
	Q_UNUSED(a_Name);
	Q_UNUSED(a_TaskFn);
	Q_UNUSED(a_AbortFn);
}

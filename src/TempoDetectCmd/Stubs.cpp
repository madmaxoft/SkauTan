#include "../BackgroundTasks.hpp"





void BackgroundTasks::enqueue(
	const QString & a_Name,
	std::function<void(void)> a_TaskFn,
	bool a_Prioritize,
	std::function<void(void)> a_AbortFn
)
{
	Q_UNUSED(a_Name);
	Q_UNUSED(a_TaskFn);
	Q_UNUSED(a_Prioritize);
	Q_UNUSED(a_AbortFn);
}

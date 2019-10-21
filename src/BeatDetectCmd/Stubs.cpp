#include "../BackgroundTasks.hpp"





void BackgroundTasks::enqueue(
	const QString & aName,
	std::function<void(void)> aTaskFn,
	bool aPrioritize,
	std::function<void(void)> aAbortFn
)
{
	Q_UNUSED(aName);
	Q_UNUSED(aTaskFn);
	Q_UNUSED(aPrioritize);
	Q_UNUSED(aAbortFn);
}

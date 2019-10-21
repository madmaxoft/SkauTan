#include "DlgBackgroundTaskList.hpp"
#include "ui_DlgBackgroundTaskList.h"
#include <cassert>
#include <QDebug>
#include "../../Stopwatch.hpp"
#include "../../Settings.hpp"





DlgBackgroundTaskList::DlgBackgroundTaskList(QWidget * aParent) :
	Super(aParent),
	mUI(new Ui::DlgBackgroundTaskList)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgBackgroundTaskList", *this);
	auto & backgroundTasks = BackgroundTasks::get();

	// Connect the signals:
	connect(&backgroundTasks, &BackgroundTasks::taskAdded,    this, &DlgBackgroundTaskList::addTask);
	connect(&backgroundTasks, &BackgroundTasks::taskFinished, this, &DlgBackgroundTaskList::delTask);
	connect(&backgroundTasks, &BackgroundTasks::taskAborted,  this, &DlgBackgroundTaskList::delTask);
	connect(mUI->btnClose,   &QPushButton::clicked,          this, &QDialog::close);
	connect(&mUpdateTimer,   &QTimer::timeout,               this, &DlgBackgroundTaskList::periodicUpdateUi);

	// Insert the current tasks:
	for (const auto & task: backgroundTasks.tasks())
	{
		addTask(task);
	}
	updateCountLabel();

	// Start the UI updater, update UI every half second:
	mUpdateTimer.start(500);
}





DlgBackgroundTaskList::~DlgBackgroundTaskList()
{
	Settings::saveWindowPos("DlgBackgroundTaskList", *this);
}





void DlgBackgroundTaskList::updateCountLabel()
{
	mUI->lblTasks->setText(tr("Background tasks: %1").arg(mUI->lwTasks->count()));
}





void DlgBackgroundTaskList::addTask(BackgroundTasks::TaskPtr aTask)
{
	// Postpone action into the UI update
	assert(aTask != nullptr);
	mTasksToAdd.push_back(std::move(aTask));
}





void DlgBackgroundTaskList::delTask(BackgroundTasks::TaskPtr aTask)
{
	// Postpone action into the UI update
	assert(aTask != nullptr);
	mTasksToRemove.push_back(std::move(aTask));
}





void DlgBackgroundTaskList::periodicUpdateUi()
{
	// Add new tasks:
	for (const auto & task: mTasksToAdd)
	{
		auto item = new QListWidgetItem(task->name());
		mTaskItemMap[task] = item;
		mUI->lwTasks->addItem(item);
	}
	mTasksToAdd.clear();

	// Remove finished tasks:
	for (const auto & task: mTasksToRemove)
	{
		auto itr = mTaskItemMap.find(task);
		if (itr != mTaskItemMap.end())
		{
			auto item = itr->second;
			mTaskItemMap.erase(itr);
			delete item;  // Removes the item from the widget
		}
	}
	mTasksToRemove.clear();

	updateCountLabel();
}

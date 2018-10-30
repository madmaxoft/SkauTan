#include "DlgBackgroundTaskList.hpp"
#include "ui_DlgBackgroundTaskList.h"
#include <cassert>
#include <QDebug>
#include "../../Stopwatch.hpp"
#include "../../Settings.hpp"





DlgBackgroundTaskList::DlgBackgroundTaskList(QWidget * a_Parent) :
	Super(a_Parent),
	m_UI(new Ui::DlgBackgroundTaskList)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgBackgroundTaskList", *this);
	auto & backgroundTasks = BackgroundTasks::get();

	// Connect the signals:
	connect(&backgroundTasks, &BackgroundTasks::taskAdded,    this, &DlgBackgroundTaskList::addTask);
	connect(&backgroundTasks, &BackgroundTasks::taskFinished, this, &DlgBackgroundTaskList::delTask);
	connect(&backgroundTasks, &BackgroundTasks::taskAborted,  this, &DlgBackgroundTaskList::delTask);
	connect(m_UI->btnClose,   &QPushButton::clicked,          this, &QDialog::close);
	connect(&m_UpdateTimer,   &QTimer::timeout,               this, &DlgBackgroundTaskList::periodicUpdateUi);

	// Insert the current tasks:
	for (const auto & task: backgroundTasks.tasks())
	{
		addTask(task);
	}
	updateCountLabel();

	// Start the UI updater, update UI every half second:
	m_UpdateTimer.start(500);
}





DlgBackgroundTaskList::~DlgBackgroundTaskList()
{
	Settings::saveWindowPos("DlgBackgroundTaskList", *this);
}





void DlgBackgroundTaskList::updateCountLabel()
{
	m_UI->lblTasks->setText(tr("Background tasks: %1").arg(m_UI->lwTasks->count()));
}





void DlgBackgroundTaskList::addTask(BackgroundTasks::TaskPtr a_Task)
{
	// Postpone action into the UI update
	assert(a_Task != nullptr);
	m_TasksToAdd.push_back(std::move(a_Task));
}





void DlgBackgroundTaskList::delTask(BackgroundTasks::TaskPtr a_Task)
{
	// Postpone action into the UI update
	assert(a_Task != nullptr);
	m_TasksToRemove.push_back(std::move(a_Task));
}





void DlgBackgroundTaskList::periodicUpdateUi()
{
	// Add new tasks:
	for (const auto & task: m_TasksToAdd)
	{
		auto item = new QListWidgetItem(task->name());
		m_TaskItemMap[task] = item;
		m_UI->lwTasks->addItem(item);
	}
	m_TasksToAdd.clear();

	// Remove finished tasks:
	for (const auto & task: m_TasksToRemove)
	{
		auto itr = m_TaskItemMap.find(task);
		if (itr != m_TaskItemMap.end())
		{
			auto item = itr->second;
			m_TaskItemMap.erase(itr);
			delete item;  // Removes the item from the widget
		}
	}
	m_TasksToRemove.clear();

	updateCountLabel();
}

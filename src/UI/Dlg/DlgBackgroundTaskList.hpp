#ifndef DLGBACKGROUNDTASKLIST_H
#define DLGBACKGROUNDTASKLIST_H





#include <memory>
#include <QDialog>
#include <QTimer>
#include "../../BackgroundTasks.hpp"





// fwd:
class QListWidgetItem;
namespace Ui
{
	class DlgBackgroundTaskList;
}





class DlgBackgroundTaskList:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT


public:

	explicit DlgBackgroundTaskList(QWidget * a_Parent = nullptr);
	virtual ~DlgBackgroundTaskList() override;


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgBackgroundTaskList> m_UI;

	/** Maps tasks to items for faster lookup. */
	std::map<BackgroundTasks::TaskPtr, QListWidgetItem *> m_TaskItemMap;

	/** Tasks that should be added to the UI on the next periodic update. */
	std::vector<BackgroundTasks::TaskPtr> m_TasksToAdd;

	/** Tasks that should be removed from the UI on the next periodic update. */
	std::vector<BackgroundTasks::TaskPtr> m_TasksToRemove;

	/** The timer used for periodic UI updates. */
	QTimer m_UpdateTimer;


	/** Updates m_UI->lblTasks with the current count of the tasks. */
	void updateCountLabel();


private slots:

	/** Emitted by BackgroundTasks after a task is added to the queue.
	Queues adding a new task to the UI. */
	void addTask(BackgroundTasks::TaskPtr a_Task);

	/** Emitted by BackgroundTasks after a task is finished or aborted.
	Queues removing the task from the UI. */
	void delTask(BackgroundTasks::TaskPtr a_Task);

	/** Called periodically to update the UI (task list and count). */
	void periodicUpdateUi();
};





#endif // DLGBACKGROUNDTASKLIST_H

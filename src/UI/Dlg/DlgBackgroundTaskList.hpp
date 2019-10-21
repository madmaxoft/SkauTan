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

	explicit DlgBackgroundTaskList(QWidget * aParent = nullptr);
	virtual ~DlgBackgroundTaskList() override;


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgBackgroundTaskList> mUI;

	/** Maps tasks to items for faster lookup. */
	std::map<BackgroundTasks::TaskPtr, QListWidgetItem *> mTaskItemMap;

	/** Tasks that should be added to the UI on the next periodic update. */
	std::vector<BackgroundTasks::TaskPtr> mTasksToAdd;

	/** Tasks that should be removed from the UI on the next periodic update. */
	std::vector<BackgroundTasks::TaskPtr> mTasksToRemove;

	/** The timer used for periodic UI updates. */
	QTimer mUpdateTimer;


	/** Updates mUI->lblTasks with the current count of the tasks. */
	void updateCountLabel();


private slots:

	/** Emitted by BackgroundTasks after a task is added to the queue.
	Queues adding a new task to the UI. */
	void addTask(BackgroundTasks::TaskPtr aTask);

	/** Emitted by BackgroundTasks after a task is finished or aborted.
	Queues removing the task from the UI. */
	void delTask(BackgroundTasks::TaskPtr aTask);

	/** Called periodically to update the UI (task list and count). */
	void periodicUpdateUi();
};





#endif // DLGBACKGROUNDTASKLIST_H

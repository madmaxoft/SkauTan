#include "PlayerWindow.h"
#include <QApplication>
#include "BackgroundTasks.h"









int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	BackgroundTasks::get();
	PlayerWindow w;
	w.showMaximized();

	return a.exec();
}

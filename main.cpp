#include "PlayerWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	PlayerWindow w;
	w.showMaximized();

	return a.exec();
}

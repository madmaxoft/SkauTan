#include "DlgDebugLog.hpp"
#include "ui_DlgDebugLog.h"
#include "../../Settings.hpp"
#include "../../DebugLogger.hpp"





DlgDebugLog::DlgDebugLog(QWidget * aParent):
	Super(aParent),
	mUI(new Ui::DlgDebugLog)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgDebugLog", *this);
	Settings::loadHeaderView("DlgDebugLog", "twMessages", *(mUI->twMessages->horizontalHeader()));

	// Connect the signals:
	connect(mUI->btnClose, &QPushButton::clicked, this, &QDialog::close);

	// Insert debug messages:
	const auto & msgs = DebugLogger::get().lastMessages();
	for (const auto & msg: msgs)
	{
		addMessage(msg.mDateTime, msg.mType, msg.mFileName, msg.mFunction, msg.mLineNum, msg.mMessage);
	}

	// Make the dialog have Maximize button on Windows:
	setWindowFlags(Qt::Window);
}





DlgDebugLog::~DlgDebugLog()
{
	Settings::saveHeaderView("DlgDebugLog", "twMessages", *(mUI->twMessages->horizontalHeader()));
	Settings::saveWindowPos("DlgDebugLog", *this);
}





void DlgDebugLog::addMessage(const QDateTime & aDateTime, const QtMsgType aType, const QString & aFileName, const QString & aFunction, int aLineNum, const QString & aMessage)
{
	auto row = mUI->twMessages->rowCount();
	mUI->twMessages->setRowCount(row + 1);

	QString typeString;
	switch (aType)
	{
		case QtDebugMsg:    typeString = "D"; break;
		case QtInfoMsg:     typeString = "I"; break;
		case QtWarningMsg:  typeString = "W"; break;
		case QtCriticalMsg: typeString = "C"; break;
		case QtFatalMsg:    typeString = "F"; break;
	}
	QString lineNumber = (aLineNum > 0) ? QString::number(aLineNum) : QString();
	mUI->twMessages->setItem(row, 0, new QTableWidgetItem(aDateTime.toString("yyyy-dd-MM hh:mm:ss")));
	mUI->twMessages->setItem(row, 1, new QTableWidgetItem(typeString));
	mUI->twMessages->setItem(row, 2, new QTableWidgetItem(aFileName));
	mUI->twMessages->setItem(row, 3, new QTableWidgetItem(aFunction));
	mUI->twMessages->setItem(row, 4, new QTableWidgetItem(QString::number(aLineNum)));
	mUI->twMessages->setItem(row, 5, new QTableWidgetItem(aMessage));
}

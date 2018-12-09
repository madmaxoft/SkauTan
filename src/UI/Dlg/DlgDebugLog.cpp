#include "DlgDebugLog.hpp"
#include "ui_DlgDebugLog.h"
#include "../../Settings.hpp"
#include "../../DebugLogger.hpp"





DlgDebugLog::DlgDebugLog(QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::DlgDebugLog)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgDebugLog", *this);
	Settings::loadHeaderView("DlgDebugLog", "twMessages", *(m_UI->twMessages->horizontalHeader()));

	// Connect the signals:
	connect(m_UI->btnClose, &QPushButton::clicked, this, &QDialog::close);

	// Insert debug messages:
	const auto & msgs = DebugLogger::get().lastMessages();
	for (const auto & msg: msgs)
	{
		addMessage(msg.m_DateTime, msg.m_Type, msg.m_FileName, msg.m_Function, msg.m_LineNum, msg.m_Message);
	}

	// Make the dialog have Maximize button on Windows:
	setWindowFlags(Qt::Window);
}





DlgDebugLog::~DlgDebugLog()
{
	Settings::saveHeaderView("DlgDebugLog", "twMessages", *(m_UI->twMessages->horizontalHeader()));
	Settings::saveWindowPos("DlgDebugLog", *this);
}





void DlgDebugLog::addMessage(const QDateTime & a_DateTime, const QtMsgType a_Type, const QString & a_FileName, const QString & a_Function, int a_LineNum, const QString & a_Message)
{
	auto row = m_UI->twMessages->rowCount();
	m_UI->twMessages->setRowCount(row + 1);

	QString typeString;
	switch (a_Type)
	{
		case QtDebugMsg:    typeString = "D"; break;
		case QtInfoMsg:     typeString = "I"; break;
		case QtWarningMsg:  typeString = "W"; break;
		case QtCriticalMsg: typeString = "C"; break;
		case QtFatalMsg:    typeString = "F"; break;
	}
	QString lineNumber = (a_LineNum > 0) ? QString::number(a_LineNum) : QString();
	m_UI->twMessages->setItem(row, 0, new QTableWidgetItem(a_DateTime.toString("yyyy-dd-MM hh:mm:ss")));
	m_UI->twMessages->setItem(row, 1, new QTableWidgetItem(typeString));
	m_UI->twMessages->setItem(row, 2, new QTableWidgetItem(a_FileName));
	m_UI->twMessages->setItem(row, 3, new QTableWidgetItem(a_Function));
	m_UI->twMessages->setItem(row, 4, new QTableWidgetItem(QString::number(a_LineNum)));
	m_UI->twMessages->setItem(row, 5, new QTableWidgetItem(a_Message));
}

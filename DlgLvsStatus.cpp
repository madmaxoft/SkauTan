#include "DlgLvsStatus.h"
#include "ui_DlgVoteServer.h"
#include <QNetworkInterface>
#include <QDesktopServices>
#include <QUrl>
#include "Settings.h"
#include "LocalVoteServer.h"





DlgLvsStatus::DlgLvsStatus(ComponentCollection & a_Components, QWidget * a_Parent):
	Super(a_Parent),
	m_UI(new Ui::DlgVoteServer),
	m_Components(a_Components)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgVoteServer", *this);

	// Fill in the addresses:
	auto lvs = m_Components.get<LocalVoteServer>();
	auto serverPort = lvs->port();
	auto interfaces = QNetworkInterface::allInterfaces();
	auto tw = m_UI->twAddresses;
	QStringList res;
	int row = 0;
	for (const auto &intf: interfaces)
	{
		if (
			((intf.flags() & QNetworkInterface::IsUp) != 0) &&
			((intf.flags() & QNetworkInterface::IsRunning) != 0) &&
			((intf.flags() & QNetworkInterface::IsLoopBack) == 0)
		)
		{
			for (const auto & addr: intf.addressEntries())
			{
				QUrl url;
				url.setScheme("http");
				auto ip = addr.ip();
				ip.setScopeId({});  // QUrl cannot process scopeId
				auto ipstr = ip.toString();
				if (ip.protocol() == QAbstractSocket::IPv6Protocol)
				{
					ipstr = "[" + ipstr + "]";
				}
				url.setHost(ipstr);
				url.setPort(serverPort);
				tw->setRowCount(row + 1);
				tw->setItem(row, 0, new QTableWidgetItem(url.toString()));
				tw->setItem(row, 1, new QTableWidgetItem(intf.humanReadableName()));
				row += 1;
			}
		}
	}
	tw->resizeColumnsToContents();

	// Connect signals and slots:
	connect(m_UI->btnClose, &QPushButton::pressed,        this, &QDialog::close);
	connect(tw,             &QTableWidget::doubleClicked, this, &DlgLvsStatus::cellDblClicked);
}





DlgLvsStatus::~DlgLvsStatus()
{
	Settings::saveWindowPos("DlgVoteServer", *this);
}





void DlgLvsStatus::cellDblClicked(const QModelIndex & a_Index)
{
	if (a_Index.column() == 0)
	{
		auto cellData = a_Index.data().toString();
		if (cellData.contains("http://"))
		{
			auto url = QUrl(cellData);
			QDesktopServices::openUrl(url);
		}
	}
}

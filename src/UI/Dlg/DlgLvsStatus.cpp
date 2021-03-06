#include "DlgLvsStatus.hpp"
#include "ui_DlgLvsStatus.h"
#include <QNetworkInterface>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QrCodeGen/QrCode.hpp>
#include "../../Settings.hpp"
#include "../../LocalVoteServer.hpp"





static QPixmap renderQrCode(const QString & aText)
{
	auto qr = qrcodegen::QrCode::encodeText(aText.toUtf8().constData(), qrcodegen::QrCode::Ecc::HIGH);
	auto size = qr.getSize();
	QImage img(size, size, QImage::Format_Mono);
	for (int x = 0; x < size; ++x)
	{
		for (int y = 0; y < size; ++y)
		{
			img.setPixel(x, y, qr.getModule(x, y) ? 0 : 1);
		}
	}
	return QPixmap::fromImage(img).scaled(size * 4, size * 4);
}





////////////////////////////////////////////////////////////////////////////////
// DlgLvsStatus:

DlgLvsStatus::DlgLvsStatus(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mUI(new Ui::DlgLvsStatus),
	mComponents(aComponents)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgVoteServer", *this);
	updateVoteCount();

	// Fill in the addresses:
	auto lvs = mComponents.get<LocalVoteServer>();
	auto serverPort = lvs->port();
	auto interfaces = QNetworkInterface::allInterfaces();
	auto tw = mUI->twAddresses;
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
	connect(mUI->btnClose, &QPushButton::pressed,                    this, &QDialog::close);
	connect(tw,             &QTableWidget::doubleClicked,             this, &DlgLvsStatus::cellDblClicked);
	connect(tw,             &QTableWidget::currentCellChanged,        this, &DlgLvsStatus::displayQrCode);
	connect(lvs.get(),      &LocalVoteServer::addVoteRhythmClarity,   this, &DlgLvsStatus::updateVoteCount);
	connect(lvs.get(),      &LocalVoteServer::addVoteGenreTypicality, this, &DlgLvsStatus::updateVoteCount);
	connect(lvs.get(),      &LocalVoteServer::addVotePopularity,      this, &DlgLvsStatus::updateVoteCount);
}





DlgLvsStatus::~DlgLvsStatus()
{
	Settings::saveWindowPos("DlgVoteServer", *this);
}





void DlgLvsStatus::cellDblClicked(const QModelIndex & aIndex)
{
	if (aIndex.column() == 0)
	{
		auto cellData = aIndex.data().toString();
		if (cellData.contains("http://"))
		{
			auto url = QUrl(cellData);
			QDesktopServices::openUrl(url);
		}
	}
}





void DlgLvsStatus::displayQrCode()
{
	QString url;
	auto curRow = mUI->twAddresses->currentRow();
	if (curRow >= 0)
	{
		auto item = mUI->twAddresses->item(curRow, 0);
		if (item != nullptr)
		{
			url = item->data(Qt::DisplayRole).toString();
		}
	}
	if (url.isEmpty())
	{
		mUI->lblQrCode->setPixmap(QPixmap());
	}
	else
	{
		mUI->lblQrCode->setPixmap(renderQrCode(url));
	}
}





void DlgLvsStatus::updateVoteCount()
{
	auto lvs = mComponents.get<LocalVoteServer>();
	mUI->lblVoteCount->setText(tr("Number of votes: %1").arg(lvs->numVotes()));
}

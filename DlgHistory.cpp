#include "DlgHistory.h"
#include "ui_DlgHistory.h"
#include <QSqlQueryModel>
#include <QSqlError>
#include <QDebug>
#include "Database.h"
#include "Settings.h"





DlgHistory::DlgHistory(Database & a_DB, QWidget * a_Parent) :
	Super(a_Parent),
	m_UI(new Ui::DlgHistory)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgHistory", *this);
	auto model = new QSqlQueryModel(this);
	model->setQuery(a_DB.playbackHistorySqlQuery());
	if (model->lastError().type() != QSqlError::NoError)
	{
		qWarning() << ": LastError: " << model->lastError();
	}
	/*
	model->setHeaderData(0, Qt::Horizontal, tr("Last played"));
	model->setHeaderData(1, Qt::Horizontal, tr("Genre"));
	model->setHeaderData(2, Qt::Horizontal, tr("Author"));
	model->setHeaderData(3, Qt::Horizontal, tr("Title"));
	model->setHeaderData(4, Qt::Horizontal, tr("FileName"));
	*/
	m_UI->tblHistory->setModel(model);

	// Stretch the columns to fit in the headers / data:
	QFontMetrics fmHdr(m_UI->tblHistory->horizontalHeader()->font());
	QFontMetrics fmItems(m_UI->tblHistory->font());
	m_UI->tblHistory->setColumnWidth(0, fmItems.width("W2088-08-08T00:00:00.000W"));
	m_UI->tblHistory->setColumnWidth(1, fmHdr.width("WGenreW"));
	Settings::loadHeaderView("DlgHistory", "tblHistory", *m_UI->tblHistory->horizontalHeader());

	connect(m_UI->btnClose, &QPushButton::clicked, this, &DlgHistory::closeClicked);

	setWindowFlags(Qt::Window);
}





DlgHistory::~DlgHistory()
{
	Settings::saveHeaderView("DlgHistory", "tblHistory", *m_UI->tblHistory->horizontalHeader());
	Settings::saveWindowPos("DlgHistory", *this);
}





void DlgHistory::closeClicked(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	close();
}

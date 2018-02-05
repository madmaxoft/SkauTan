#include "DlgHistory.h"
#include "ui_DlgHistory.h"
#include <QSqlQueryModel>
#include <QSqlError>
#include <QDebug>
#include "SongDatabase.h"





DlgHistory::DlgHistory(SongDatabase & a_SongDB, QWidget * a_Parent) :
	Super(a_Parent),
	m_UI(new Ui::DlgHistory)
{
	m_UI->setupUi(this);
	auto model = new QSqlQueryModel(this);
	model->setQuery("SELECT "
		"PlaybackHistory.Timestamp, "
		"Songs.Genre, "
		"Songs.Author, "
		"Songs.Title, "
		"Songs.FileName "
		"FROM PlaybackHistory LEFT JOIN Songs ON Songs.RowID = PlaybackHistory.SongID",
		a_SongDB.database()
	);
	if (model->lastError().type() != QSqlError::NoError)
	{
		qWarning() << __FUNCTION__ << ": LastError: " << model->lastError();
	}
	model->setHeaderData(0, Qt::Horizontal, tr("Last played"));
	model->setHeaderData(1, Qt::Horizontal, tr("Genre"));
	model->setHeaderData(2, Qt::Horizontal, tr("Author"));
	model->setHeaderData(3, Qt::Horizontal, tr("Title"));
	model->setHeaderData(4, Qt::Horizontal, tr("FileName"));
	m_UI->tblHistory->setModel(model);

	// Stretch the columns to fit in the headers / data:
	QFontMetrics fmHdr(m_UI->tblHistory->horizontalHeader()->font());
	QFontMetrics fmItems(m_UI->tblHistory->font());
	m_UI->tblHistory->setColumnWidth(0, fmItems.width("W2088-08-08T00:00:00.000W"));
	m_UI->tblHistory->setColumnWidth(1, fmHdr.width("WGenreW"));

	connect(m_UI->btnClose, &QPushButton::clicked, this, &DlgHistory::closeClicked);

	setWindowFlags(Qt::Window);
}





DlgHistory::~DlgHistory()
{
	// Nothing explicit needed, but the destructor needs to be defined in the CPP file due to m_UI.
}





void DlgHistory::closeClicked(bool a_IsChecked)
{
	Q_UNUSED(a_IsChecked);

	close();
}

#include "DlgLibraryMaintenance.hpp"
#include "ui_DlgLibraryMaintenance.h"
#include <QMessageBox>
#include "../../Settings.hpp"
#include "../../BackgroundTasks.hpp"
#include "../../ComponentCollection.hpp"
#include "../../DB/Database.hpp"





DlgLibraryMaintenance::DlgLibraryMaintenance(ComponentCollection & a_Components, QWidget *a_Parent) :
	QDialog(a_Parent),
	m_Components(a_Components),
	m_UI(new Ui::DlgLibraryMaintenance)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgLibraryMaintenance", *this);

	// Connect the signals:
	connect(m_UI->btnClose,                   &QPushButton::pressed, this, &QDialog::close);
	connect(m_UI->btnRemoveInaccessibleSongs, &QPushButton::pressed, this, &DlgLibraryMaintenance::removeInaccessibleSongs);
}





DlgLibraryMaintenance::~DlgLibraryMaintenance()
{
	Settings::saveWindowPos("DlgLibraryMaintenance", *this);
}





void DlgLibraryMaintenance::inaccessibleSongsRemoved(quint32 a_NumRemoved)
{
	QMessageBox::information(
		this,
		tr("SkauTan: Inaccessible songs removed"),
		tr("Number of songs removed: %1").arg(a_NumRemoved)
	);
}





void DlgLibraryMaintenance::removeInaccessibleSongs()
{
	auto db = m_Components.get<Database>();
	BackgroundTasks::enqueue(tr("Remove inaccessible songs"),
		[db, this]()
		{
			quint32 numRemoved = db->removeInaccessibleSongs();
			QMetaObject::invokeMethod(this, "inaccessibleSongsRemoved", Q_ARG(quint32, numRemoved));
		}
	);
}

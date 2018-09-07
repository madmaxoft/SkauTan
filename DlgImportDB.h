#ifndef DLGIMPORTDB_H
#define DLGIMPORTDB_H




#include <memory>
#include <QDialog>
#include "DatabaseImport.h"





namespace Ui
{
	class DlgImportDB;
}





class DlgImportDB:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT

public:

	/** The filename of the DB to import. */
	QString m_FileName;

	/** The import options, as set by the user. */
	DatabaseImport::Options m_Options;


	explicit DlgImportDB(QWidget * a_Parent = nullptr);
	~DlgImportDB();


private:

	std::unique_ptr<Ui::DlgImportDB> m_UI;


private slots:

	/** Saves the options from the UI into m_Options. */
	void onFinished(int a_Result);

	/** The Browse button has been clicked, ask the user for the DB file. */
	void browseForDB();
};





#endif // DLGIMPORTDB_H

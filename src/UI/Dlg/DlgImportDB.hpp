#ifndef DLGIMPORTDB_H
#define DLGIMPORTDB_H




#include <memory>
#include <QDialog>
#include "../../DB/DatabaseImport.hpp"





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
	QString mFileName;

	/** The import options, as set by the user. */
	DatabaseImport::Options mOptions;


	explicit DlgImportDB(QWidget * aParent = nullptr);
	~DlgImportDB();


private:

	std::unique_ptr<Ui::DlgImportDB> mUI;


private slots:

	/** Saves the options from the UI into mOptions. */
	void onFinished(int aResult);

	/** The Browse button has been clicked, ask the user for the DB file. */
	void browseForDB();
};





#endif // DLGIMPORTDB_H

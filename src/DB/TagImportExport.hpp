#pragma once

#include <QString>
#include <QObject>





// fwd:
class Database;





/** Imports and exports tags for all SongSharedData in the DB. */
class TagImportExport:
	public QObject
{
	Q_OBJECT


public:

	/** Exports Primary tags from the DB into the specified file.
	Throws and Exception on error. */
	static void doExport(const Database & aDB, const QString & aFileName);

	/** Imports Manual tags from the specified file into the DB.
	Throws and Exception on error. */
	static void doImport(Database & aDB, const QString & aFileName);
};

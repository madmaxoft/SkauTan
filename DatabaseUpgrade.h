#ifndef DATABASEUPGRADE_H
#define DATABASEUPGRADE_H





#include <string>
#include <QSqlError>
#include <Exception.h>





// fwd:
class Database;
class QSqlDatabase;





class DatabaseUpgrade
{
public:

	/** An exception that is thrown on upgrade error if the SQL command fails. */
	class SqlError:
		public RuntimeError
	{
		using Super = RuntimeError;

	public:
		/** Creates a new instance, based on the SQL error reported for the command. */
		SqlError(const QSqlError & a_SqlError, const std::string & a_SqlCommand);
	};


	/** Upgrades the database to the latest known version.
	Throws SqlError if the upgrade fails.
	Tries its best to keep the DB in a usable state, even if the upgrade fails. */
	static void upgrade(Database & a_DB);

	/** Returns the highest version that the upgrade knows (current version). */
	static size_t currentVersion();


protected:


	/** The SQL database on which to perform the upgrade. */
	QSqlDatabase & m_DB;


	/** Creates a new instance of this object. */
	DatabaseUpgrade(Database & a_DB);

	/** Performs the whole upgrade on m_DB. */
	void execute();

	/** Returns the version stored in the DB.
	If the DB contains no versioning data, returns 0. */
	size_t getVersion();
};





#endif // DATABASEUPGRADE_H

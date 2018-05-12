#ifndef DATABASEUPGRADE_H
#define DATABASEUPGRADE_H





#include <stdexcept>
#include <string>
#include <QSqlError>





// fwd:
class Database;
class QSqlDatabase;





class DatabaseUpgrade
{
public:

	/** An exception that is thrown on upgrade error. */
	class Error:
		public std::runtime_error
	{
		using Super = std::runtime_error;

	public:
		Error(const char * a_What):
			Super(a_What)
		{
		}
	};


	/** An exception that is thrown on upgrade error if the SQL command fails. */
	class SqlError:
		public Error
	{
		using Super = Error;

	public:
		/** Creates a new instance, based on the SQL error reported for the command. */
		SqlError(const QSqlError & a_SqlError, const std::string & a_SqlCommand);

		/** The error reported by the SQL engine. */
		const QSqlError m_Error;

		/** The command that caused the error. */
		const std::string m_Command;
	};


	/** Upgrades the database to the latest known version.
	May throw Error if the upgrade fails.
	Tries its best to keep the DB in a usable state, even if the upgrade fails. */
	static void upgrade(Database & a_DB);


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

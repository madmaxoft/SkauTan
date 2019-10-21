#ifndef DATABASEBACKUP_H
#define DATABASEBACKUP_H





#include <QObject>
#include <QString>





/** A namespace-class for functions performing DB backup on various occasions. */
class DatabaseBackup:
	public QObject
{
	Q_OBJECT

public:
	/** If the DB hasn't been backed up today, makes a backup.
	Throws a RuntimeError if a backup is to be made, but it fails. */
	static void dailyBackupOnStartup(
		const QString & aDBFileName,
		const QString & aBackupFolder
	);


	/** Makes a backup of the DB before upgrading.
	aCurrentVersion is the current DB version (before upgrade).
	Throws a RuntimeError if the backup fails.
	Fails if the destination file already exists. */
	static void backupBeforeUpgrade(
		const QString & aDBFileName,
		size_t aCurrentVersion,
		const QString & aBackupFolder
	);
};





#endif // DATABASEBACKUP_H

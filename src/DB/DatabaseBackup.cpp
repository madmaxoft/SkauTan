#include "DatabaseBackup.hpp"
#include <QDate>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include "../Exception.hpp"





void DatabaseBackup::dailyBackupOnStartup(
	const QString & aDBFileName,
	const QString & aBackupFolder
)
{
	// If the DB file is not existent, there's nothing to back up:
	if (!QFile::exists(aDBFileName))
	{
		qDebug() << "Skipping daily backup, there's no DB file yet: " << aDBFileName;
		return;
	}

	auto now = QDate::currentDate();
	auto dstFileName = aBackupFolder + QString("%1/%1-%2-%3.sqlite")
		.arg(now.year())
		.arg(QString::number(now.month()), 2, '0')
		.arg(QString::number(now.day()), 2, '0');

	QFileInfo fi(dstFileName);
	if (fi.exists())
	{
		qDebug() << "Skipping daily backup, already made one today";
		return;
	}
	if(!fi.absoluteDir().mkpath(fi.absolutePath()))
	{
		throw RuntimeError(tr("Cannot create folder for daily backups: %1"), fi.absolutePath());
	}
	if (!QFile::copy(aDBFileName, dstFileName))
	{
		throw RuntimeError(tr("Cannot create a daily DB backup %1"), dstFileName);
	}
	qDebug() << "Daily backup created: " << dstFileName;
}





void DatabaseBackup::backupBeforeUpgrade(
	const QString & aDBFileName,
	size_t aCurrentVersion,
	const QString & aBackupFolder
)
{
	auto now = QDate::currentDate();
	auto dstFileName = aBackupFolder + QString("%1/%1-%2-%3-ver%4.sqlite")
		.arg(now.year())
		.arg(QString::number(now.month()), 2, '0')
		.arg(QString::number(now.day()), 2, '0')
		.arg(static_cast<qulonglong>(aCurrentVersion));

	QFileInfo fi(dstFileName);
	if (fi.exists())
	{
		throw Exception(tr("Cannot backup DB before upgrade, destination file %1 already exists."), dstFileName);
	}
	if(!fi.absoluteDir().mkpath(fi.absolutePath()))
	{
		throw RuntimeError(tr("Cannot create the folder for the pre-upgrade backup: %1"), fi.absolutePath());
	}
	if (!QFile::copy(aDBFileName, dstFileName))
	{
		throw RuntimeError(tr("Cannot create the pre-upgrade DB backup %1"), dstFileName);
	}
	qDebug() << "Pre-upgrade backup created: " << dstFileName;
}

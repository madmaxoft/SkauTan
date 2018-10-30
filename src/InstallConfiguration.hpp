#ifndef INSTALLCONFIGURATION_H
#define INSTALLCONFIGURATION_H





#include <QString>
#include "ComponentCollection.hpp"





/** Provides information that is specific to this installation of the program:
- writable data location */
class InstallConfiguration:
	public ComponentCollection::Component<ComponentCollection::ckInstallConfiguration>
{
public:

	/** Initializes the paths.
	Throws a RuntimeError if no suitable path was found. */
	InstallConfiguration();

	/** Returns the path where the specified file (with writable data) / folder should be stored. */
	QString dataLocation(const QString & a_FileName) const { return m_DataPath + a_FileName; }

	/** Returns the filename of the main DB. */
	QString dbFileName() const { return dataLocation("SkauTan.sqlite"); }

	/** Returns the path to the folder into which DB backups should be stored. */
	QString dbBackupsFolder() const { return dataLocation("backups/"); }


protected:

	/** The base path where SkauTan should store its data (writable).
	If nonempty, includes the trailing slash. */
	QString m_DataPath;


	/** Returns the folder to use for m_DataPath.
	To be called from the constructor.
	Throws a RuntimeError if no suitable path was found. */
	QString detectDataPath();

	/** Returns true if the specified path is suitable for data path.
	Checks that the path is writable.
	If the path doesn't exist, creates it. */
	bool isDataPathSuitable(const QString & a_Folder);
};





#endif // INSTALLCONFIGURATION_H

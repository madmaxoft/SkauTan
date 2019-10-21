#include "InstallConfiguration.hpp"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>





#ifdef _WIN32
	// Needed for NTFS permission checking, as documented in QFileInfo
	extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#endif  // _WIN32





InstallConfiguration::InstallConfiguration():
	mDataPath(detectDataPath())
{
	qDebug() << "Using data path " << mDataPath;
}





QString InstallConfiguration::detectDataPath()
{
	// If the current folder is writable, use that as the data location ("portable" version):
	if (isDataPathSuitable("."))
	{
		return "";
	}

	// Try the local app data folder:
	auto writableData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if (isDataPathSuitable(writableData))
	{
		if (writableData.right(1) != "/")
		{
			writableData += "/";
		}
		return writableData;
	}

	// No suitable location found
	throw RuntimeError("Cannot find a suitable location to store the data.");
}





bool InstallConfiguration::isDataPathSuitable(const QString & aFolder)
{
	QFileInfo folder(aFolder);
	if (!folder.exists())
	{
		QDir d;
		if (!d.mkpath(aFolder))
		{
			// Folder doesn't exist and cannot be created.
			return false;
		}
	}

	#ifdef _WIN32
		qt_ntfs_permission_lookup++;  // Turn NTFS permission checking on
	#endif

	auto res = folder.isWritable();

	#ifdef _WIN32
		qt_ntfs_permission_lookup--;  // Turn NTFS permission checking off
	#endif

	return res;
}

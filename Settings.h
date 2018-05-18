#ifndef HEADERCOLUMNSAVER_H
#define HEADERCOLUMNSAVER_H





// fwd:
class QHeaderView;
class QSettings;





#include <memory>
#include <QString>





/** Saves and restores various app settings to a common setting file. */
class Settings
{
public:

	/** Initializes the Settings subsystem. */
	static void init(const QString & a_IniFileName);

	/** Saves QHeaderView's columns' widths. */
	static void saveHeaderView(const char * a_WindowName, const char * a_HeaderViewName, const QHeaderView & a_HeaderView);

	/** Restores previously saved QHeaderView's columns' widths. */
	static void loadHeaderView(const char * a_WindowName, const char * a_HeaderViewName, QHeaderView & a_HeaderView);


protected:

	static std::unique_ptr<QSettings> m_Settings;
};





#endif // HEADERCOLUMNSAVER_H

#ifndef HEADERCOLUMNSAVER_H
#define HEADERCOLUMNSAVER_H





// fwd:
class QHeaderView;
class QSettings;
class QWidget;
class QSplitter;





#include <memory>
#include <QString>
#include <QVariant>





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

	/** Saves a top level window's position and size. */
	static void saveWindowPos(const char * a_WindowName, const QWidget & a_Window);

	/** Restores a previously saved top level window's position and size. */
	static void loadWindowPos(const char * a_WindowName, QWidget & a_Window);

	/** Saves the splitter section sizes. */
	static void saveSplitterSizes(const char * a_WindowName, const char * a_SplitterName, const QSplitter & a_Splitter);

	/** Saves the splitter section sizes. */
	static void saveSplitterSizes(const char * a_WindowName, const char * a_SplitterName, const QSplitter * a_Splitter);

	/** Restores previously saves splitter section sizes. */
	static void loadSplitterSizes(const char * a_WindowName, const char * a_SplitterName, QSplitter & a_Splitter);

	/** Restores previously saves splitter section sizes. */
	static void loadSplitterSizes(const char * a_WindowName, const char * a_SplitterName, QSplitter * a_Splitter);

	/** Saves a generic value. */
	static void saveValue(const char * a_WindowName, const char * a_ValueName, const QVariant & a_Value);

	/** Loads a previously saved generic value. */
	static QVariant loadValue(const char * a_WindowName, const char * a_ValueName, const QVariant & a_Default = QVariant());


protected:

	static std::unique_ptr<QSettings> m_Settings;
};





#endif // HEADERCOLUMNSAVER_H

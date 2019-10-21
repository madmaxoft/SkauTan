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
	static void init(const QString & aIniFileName);

	/** Saves QHeaderView's columns' widths. */
	static void saveHeaderView(const char * aWindowName, const char * aHeaderViewName, const QHeaderView & aHeaderView);

	/** Restores previously saved QHeaderView's columns' widths. */
	static void loadHeaderView(const char * aWindowName, const char * aHeaderViewName, QHeaderView & aHeaderView);

	/** Saves a top level window's position and size. */
	static void saveWindowPos(const char * aWindowName, const QWidget & aWindow);

	/** Restores a previously saved top level window's position and size. */
	static void loadWindowPos(const char * aWindowName, QWidget & aWindow);

	/** Saves the splitter section sizes. */
	static void saveSplitterSizes(const char * aWindowName, const char * aSplitterName, const QSplitter & aSplitter);

	/** Saves the splitter section sizes. */
	static void saveSplitterSizes(const char * aWindowName, const char * aSplitterName, const QSplitter * aSplitter);

	/** Restores previously saves splitter section sizes. */
	static void loadSplitterSizes(const char * aWindowName, const char * aSplitterName, QSplitter & aSplitter);

	/** Restores previously saves splitter section sizes. */
	static void loadSplitterSizes(const char * aWindowName, const char * aSplitterName, QSplitter * aSplitter);

	/** Saves a generic value. */
	static void saveValue(const char * aWindowName, const char * aValueName, const QVariant & aValue);

	/** Loads a previously saved generic value. */
	static QVariant loadValue(const char * aWindowName, const char * aValueName, const QVariant & aDefault = QVariant());


protected:

	static std::unique_ptr<QSettings> mSettings;
};





#endif // HEADERCOLUMNSAVER_H

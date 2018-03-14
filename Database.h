#ifndef SONGDATABASE_H
#define SONGDATABASE_H





#include <vector>
#include <memory>
#include <QObject>
#include <QSqlDatabase>
#include "Song.h"
#include "Playlist.h"
#include "MetadataScanner.h"
#include "Template.h"





/** The central object that binds everything together and provides DB serialization. */
class Database:
	public QObject
{
	Q_OBJECT
	using Super = QObject;


public:
	Database();

	/** Opens the specified SQLite file and reads its contents into this object.
	Keeps the DB open for subsequent immediate updates.
	Only one DB can ever be open. */
	void open(const QString & a_DBFileName);

	/** Returns the names of all the historical playlists. */
	std::vector<QString> playlistNames();

	/** Returns all songs currently known in the DB. */
	const std::vector<SongPtr> & songs() const { return m_Songs; }

	/** Adds the specified files to m_Songs, with no metadata.
	Each pair contains the file name and size.
	Schedules the metadata to be updated.
	Skips duplicate entries. */
	void addSongFiles(const std::vector<std::pair<QString, qulonglong> > & a_Files);

	/** Adds the specified file to m_Songs, with no metadata.
	Schedules the metadata to be updated.
	Skips duplicate entries. */
	void addSongFile(const QString & a_FileName, qulonglong a_FileSize);

	QSqlDatabase & database() { return m_Database; }

	/** Saves the specified song into the DB.
	Assumes (doesn't check) that the song is contained within this DB. */
	void saveSong(const Song & a_Song);

	/** Returns all templates stored in the DB. */
	const std::vector<TemplatePtr> & templates() const { return m_Templates; }

	/** Adds a new empty template and returns it.
	Note that changes aren't saved automatically, you need to call saveTemplate() to save. */
	TemplatePtr addTemplate();

	/** Removes the specified template from the DB. */
	void delTemplate(const TemplatePtr a_Template) { delTemplate(a_Template.get()); }

	/** Removes the specified template from the DB. */
	void delTemplate(const Template * a_Template);

	/** Saves the changes in the specified template to the DB. */
	void saveTemplate(const Template & a_Template);

	/** Returns all template items from all templates that have been marked as "favorite". */
	std::vector<Template::ItemPtr> getFavoriteTemplateItems() const;

	MetadataScanner & metadataScanner() { return m_MetadataScanner; }


protected:

	/** The DB connection .*/
	QSqlDatabase m_Database;

	/** The current playlist. */
	std::shared_ptr<Playlist> m_Playlist;

	/** All the known songs. */
	std::vector<SongPtr> m_Songs;

	/** The worker thread that scans the songs' metadata in the background. */
	MetadataScanner m_MetadataScanner;

	/** All the templates that can be used for filling the playlist. */
	std::vector<TemplatePtr> m_Templates;


	/** Updates all tables to the current format.
	Adds any missing tables and rows. */
	void fixupTables();

	/** Updates the specified table to contain at least the specified columns.
	a_ColumnDefs is a vector of string tuples, each tuple is column name and its type. */
	void fixupTable(const QString & a_TableName, const std::vector<std::pair<QString, QString>> & a_ColumnDefs);

	/** Loads all the songs in the DB into m_Songs.
	Note that songs are not checked whether they exists on the disk or if their hash still fits.
	Use Song::isStillValid() for checking before adding the song to playlist / before starting playback. */
	void loadSongs();

	/** Loads all the templates in the DB into m_Templates. */
	void loadTemplates();

	/** Loads the specified template from the DB.
	The template has its direct members initialized, this loads its items and their filters. */
	void loadTemplate(TemplatePtr a_Template);

	/**	Loads the specified Template Item's filters from the DB. */
	void loadTemplateFilters(TemplatePtr a_Template, qlonglong a_ItemRowId, Template::Item & a_Item);

	/** Saves or updates the specified template item in the DB.
	a_Index is the item's index within a_Template. */
	void saveTemplateItem(const Template & a_Template, int a_Index, const Template::Item & a_Item);

	/** Recursively saves (inserts) the filter subtree from a_Filter.
	a_TemplateRowId is the RowID of the template to which the filter belongs, through its template item
	a_TemplateItemRowId is the RowID of the template item to which the filter belongs
	a_ParentFilterRowId is the RowID of the filter that is the parent of a_Filter, -1 for root filter. */
	void saveTemplateFilters(
		qlonglong a_TemplateRowId,
		qlonglong a_TemplateItemRowId,
		const Template::Filter & a_Filter,
		qlonglong a_ParentFilterRowId
	);


signals:

	/** Emitted after a new song file is added to the list of songs (addSongFile, addSongFiles).
	NOT emitted when loading songs from the DB! */
	void songFileAdded(Song * a_Song);


protected slots:

	/** Emitted by m_MetadataScanner after metadata is updated for the specified song.
	Writes the whole updated song to the DB. */
	void songScanned(Song * a_Song);


public slots:

	/** Indicates that the song has started playing and the DB should store that info. */
	void songPlaybackStarted(Song * a_Song);
};





#endif // SONGDATABASE_H

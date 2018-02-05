#ifndef SONGDATABASE_H
#define SONGDATABASE_H





#include <vector>
#include <memory>
#include <QObject>
#include <QSqlDatabase>
#include "Song.h"
#include "Playlist.h"
#include "MetadataScanner.h"





/** The central object that binds everything together and provides DB serialization. */
class SongDatabase:
	public QObject
{
	Q_OBJECT
	using Super = QObject;


public:
	SongDatabase();

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

protected:

	/** The DB connection .*/
	QSqlDatabase m_Database;

	/** The current playlist. */
	std::shared_ptr<Playlist> m_Playlist;

	/** All the known songs. */
	std::vector<SongPtr> m_Songs;

	/** The worker thread that scans the songs' metadata in the background. */
	MetadataScanner m_MetadataScanner;


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

	/** Saves the specified song into the DB. */
	void saveSong(const Song & a_Song);


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

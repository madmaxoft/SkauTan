#ifndef SONGDATABASE_H
#define SONGDATABASE_H





#include <vector>
#include <memory>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "Song.h"
#include "Template.h"
#include "ComponentCollection.h"





/** The storage for all data that is persisted across sessions.
Stores the song library, the shared song data, the templates and playback history.
Note that all DB-access functions are not thread-safe. */
class Database:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDatabase>
{
	Q_OBJECT
	using Super = QObject;


public:

	struct RemovedSong
	{
		QString m_FileName;
		QDateTime m_DateRemoved;
		bool m_WasDeleted;
		QByteArray m_Hash;
		int m_NumDuplicates;  // ... at the time of removal
	};

	struct HistoryItem
	{
		QDateTime m_Timestamp;
		QByteArray m_Hash;
	};


	/** Container for individual local community votes.
	Used for syncing. */
	struct Vote
	{
		QByteArray m_SongHash;
		QDateTime  m_DateAdded;
		int        m_VoteValue;
	};

	using RemovedSongPtr = std::shared_ptr<RemovedSong>;


	Database(ComponentCollection & a_Components);

	/** Opens the specified SQLite file and reads its contents into this object.
	Keeps the DB open for subsequent immediate updates.
	Only one DB can ever be open. */
	void open(const QString & a_DBFileName);

	/** Returns all songs currently known in the DB. */
	const std::vector<SongPtr> & songs() const { return m_Songs; }

	/** Adds the specified files to the new files list.
	Schedules the hash to be calculated; once calculated, the song will be added to m_Songs.
	Skips duplicate entries. */
	void addSongFiles(const QStringList & a_Files);

	/** Adds the specified file to the new files list.
	Schedules the hash to be calculated; once calculated, the song will be added to m_Songs.
	Skips duplicate entries. */
	void addSongFile(const QString & a_FileName);

	/** Removes the specified song from the song list.
	If a_DeleteDiskFile is true, also deletes the song file from the disk.
	Assumes (asserts) that the song is contained within this DB.
	Adds an entry to the deletion log.
	Note that the SongSharedData entry is kept, in case there are duplicates or the song is re-added later on. */
	void removeSong(const Song & a_Song, bool a_DeleteDiskFile);

	/** Returns all templates stored in the DB. */
	const std::vector<TemplatePtr> & templates() const { return m_Templates; }

	/** Creates a new empty template, adds it to the DB and returns it.
	Note that changes aren't saved automatically, you need to call saveTemplate() to save. */
	TemplatePtr createTemplate();

	/** Adds the specified template to the DB.
	Used for importing templates, assumes that the template has no DB RowID assigned to it yet.
	Modifies a_Template - assigns a new DB RowID.
	Note that changes aren't saved automatically, you need to call saveTemplate() to save. */
	void addTemplate(TemplatePtr a_Template);

	/** Removes the specified template from the DB. */
	void delTemplate(const TemplatePtr a_Template) { delTemplate(a_Template.get()); }

	/** Removes the specified template from the DB. */
	void delTemplate(const Template * a_Template);

	/** Saves the changes in the specified template to the DB. */
	void saveTemplate(const Template & a_Template);

	/** Returns all template items from all templates that have been marked as "favorite". */
	std::vector<Template::ItemPtr> getFavoriteTemplateItems() const;

	/** Returns the number of songs that match the specified filter. */
	int numSongsMatchingFilter(Template::Filter & a_Filter) const;

	/** Picks a random song matching the specified template item.
	If possible, avoids a_Avoid from being picked (picks it only if it is the only song matching the template item). */
	SongPtr pickSongForTemplateItem(Template::ItemPtr a_Item, SongPtr a_Avoid = nullptr) const;

	/** Reads and returns all the songs that have been removed from the library. */
	std::vector<RemovedSongPtr> removedSongs() const;

	/** Removes all entries from the history of removed songs. */
	void clearRemovedSongs();

	/** Returns the SharedData for the specified song hash.
	Returns nullptr if no such SharedData was found. */
	Song::SharedDataPtr sharedDataFromHash(const QByteArray & a_Hash) const;

	/** Saves all songs and SharedData from the internal arrays to the DB. */
	void saveAllSongSharedData();

	/** Returns a map of all SongHash -> SongSharedData in the DB. */
	const std::map<QByteArray, Song::SharedDataPtr> & songSharedDataMap() const { return m_SongSharedData; }

	/** Returns the entire playback history.
	Returns by-value, since the history is not normally kept in memory, so it needs to be read from DB in this call. */
	std::vector<HistoryItem> playbackHistory() const;

	/** Adds the items in a_History to the playback history.
	Used primarily by the import. */
	void addPlaybackHistory(const std::vector<HistoryItem> & a_History);

	/** Adds the items in a_History to the song removal history in the DB.
	Used primarily by the import. */
	void addSongRemovalHistory(const std::vector<RemovedSongPtr> & a_History);

	/** Loads all votes from the specified table and returns them, ordered by their DateAdded.
	Returns by-value, since the data is not normally kept in memory, so it needs to be read from the DB in this call. */
	std::vector<Vote> loadVotes(const QString & a_TableName) const;

	/** Adds the specified votes into the specified DB table.
	Used primarily by the import. */
	void addVotes(const QString & a_TableName, const std::vector<Vote> & a_Votes);


protected:

	friend class DatabaseUpgrade;


	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The DB connection .*/
	QSqlDatabase m_Database;

	/** All the known songs. */
	std::vector<SongPtr> m_Songs;

	/** The data shared among songs with equal hash. */
	std::map<QByteArray, Song::SharedDataPtr> m_SongSharedData;

	/** All the templates that can be used for filling the playlist. */
	std::vector<TemplatePtr> m_Templates;


	/** Loads all the songs in the DB into m_Songs.
	Note that songs are not checked whether they exists on the disk or if their hash still fits.
	Use Song::isStillValid() for checking before adding the song to playlist / before starting playback. */
	void loadSongs();

	/** Loads the songs into m_Songs vector.
	Songs that need metadata rescan are sent through needSongTagRescan() signal. */
	void loadSongFiles();

	/** Loads the data shared between songs with the same hash (m_SongSharedData). */
	void loadSongSharedData();

	/** The new files stored in the DB are enqueued for hash calculation. */
	void loadNewFiles();

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

	/** Returns the internal Qt DB object.
	Only used in the DatabaseUpgrade class. */
	QSqlDatabase & database() { return m_Database; }

	/** Returns the relative "weight" of a song, when choosing by a template.
	The weight is adjusted based on song's rating, last played date, etc.
	If a_Playlist is given, songs on the playlist are further reduced. */
	int getSongWeight(const Song & a_Song, const Playlist * a_Playlist = nullptr) const;

	void addVote(
		const QByteArray & a_SongHash,
		int a_VoteValue,
		const QString & a_TableName,
		DatedOptional<double> Song::Rating::* a_DstRating
	);


signals:

	/** Emitted after a new song file is added to the list of songs (addSongFile, addSongFiles).
	NOT emitted when loading songs from the DB! */
	void songFileAdded(SongPtr a_Song);

	/** Emitted just before a song is removed from the DB. */
	void songRemoving(SongPtr a_Song, size_t a_Index);

	/** Emitted after a song is removed from the DB. */
	void songRemoved(SongPtr a_Song, size_t a_Index);

	/** Emitted when a new file is added to song list that has no hash assigned to it. */
	void needFileHash(const QString & a_FileName);

	/** Emitted after loading for each song shared data that has an invalid length (#141). */
	void needSongLength(Song::SharedDataPtr a_SongSharedData);

	/** Emitted when encountering a song with hash but without tag in the DB.
	This can happen either at DB load time (open()), or when adding new songs (songHashCalculated()). */
	void needSongTagRescan(SongPtr a_Song);

	/** Emitted after a song was saved, presumably because its data had changed.
	Used by clients to update the song properties in the UI. */
	void songSaved(SongPtr a_Song);


public slots:

	/** Indicates that the song has started playing and the DB should store that info. */
	void songPlaybackStarted(SongPtr a_Song);

	/** To be called when the song hash and length has been calculated.
	Moves the song from NewFiles to SongFiles, creates an in-memory Song object for the file.
	Prepares a row for the song hash in the SharedData table, or assigns an existing row to this song.
	Emits the songFileAdded signal. */
	void songHashCalculated(const QString & a_FileName, const QByteArray & a_Hash, double a_Length);

	/** To be called when the hash calculation fails.
	Removes the specified file from the DB table of new files. */
	void songHashFailed(const QString & a_FileName);

	/** To be called when the song length has been calculated. */
	void songLengthCalculated(Song::SharedDataPtr a_SharedData, double a_LengthSec);

	/** Saves (updates) the specified song into the DB.
	Assumes (doesn't check) that the song is contained within this DB. */
	void saveSong(SongPtr a_Song);

	/** Updates the song's file-related data in the DB. */
	void saveSongFileData(SongPtr a_Song);

	/** Updates the song's shared data in the SongSharedData DB table. */
	void saveSongSharedData(Song::SharedDataPtr a_SharedData);

	/** Emitted by m_MetadataScanner after metadata is updated for the specified song.
	Writes the whole updated song to the DB. */
	void songScanned(SongPtr a_Song);

	/** Adds a new RhythmClarity vote for the specified song hash into the DB
	Updates the aggregated rating in Song::SharedData and in the DB. */
	void addVoteRhythmClarity(QByteArray a_SongHash, int a_VoteValue);

	/** Adds a new GenreTypicality vote for the specified song hash into the DB
	Updates the aggregated rating in Song::SharedData and in the DB. */
	void addVoteGenreTypicality(QByteArray a_SongHash, int a_VoteValue);

	/** Adds a new Popularity vote for the specified song hash into the DB
	Updates the aggregated rating in Song::SharedData and in the DB. */
	void addVotePopularity(QByteArray a_SongHash, int a_VoteValue);


protected slots:

	/** Adds a new entry into the playback history in the DB. */
	void addPlaybackHistory(SongPtr a_Song, const QDateTime & a_Timestamp);

};





#endif // SONGDATABASE_H

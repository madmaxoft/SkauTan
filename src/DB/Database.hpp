#ifndef SONGDATABASE_H
#define SONGDATABASE_H





#include <vector>
#include <memory>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "../Song.hpp"
#include "../Template.hpp"
#include "../Filter.hpp"
#include "../ComponentCollection.hpp"





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
		QString mFileName;
		QDateTime mDateRemoved;
		bool mWasDeleted;
		QByteArray mHash;
		int mNumDuplicates;  // ... at the time of removal
	};

	struct HistoryItem
	{
		QDateTime mTimestamp;
		QByteArray mHash;
	};


	/** Container for individual local community votes.
	Used for syncing. */
	struct Vote
	{
		QByteArray mSongHash;
		QDateTime  mDateAdded;
		int        mVoteValue;
	};

	using RemovedSongPtr = std::shared_ptr<RemovedSong>;


	Database(ComponentCollection & aComponents);

	/** Opens the specified SQLite file and reads its contents into this object.
	Keeps the DB open for subsequent immediate updates.
	Only one DB can ever be open. */
	void open(const QString & aDBFileName);

	/** Returns all songs currently known in the DB. */
	const std::vector<SongPtr> & songs() const { return mSongs; }

	/** Adds the specified files to the new files list.
	Schedules the hash to be calculated; once calculated, the song will be added to mSongs.
	Skips duplicate entries. */
	void addSongFiles(const QStringList & aFiles);

	/** Adds the specified file to the new files list.
	Schedules the hash to be calculated; once calculated, the song will be added to mSongs.
	Skips duplicate entries. */
	void addSongFile(const QString & aFileName);

	/** Removes the specified song from the song list.
	If aDeleteDiskFile is true, also deletes the song file from the disk.
	Assumes (asserts) that the song is contained within this DB.
	Adds an entry to the deletion log.
	Note that the SongSharedData entry is kept, in case there are duplicates or the song is re-added later on. */
	void removeSong(const Song & aSong, bool aDeleteDiskFile);

	/** Returns the song with the specified hash (picks random one if there are multiple).
	Returns nullptr if there is no such song. */
	SongPtr songFromHash(const QByteArray & aSongHash);

	/** Returns the song with the specified filename.
	Returns nullptr if there is no such song. */
	SongPtr songFromFileName(const QString aSongFileName);

	/** Returns all templates stored in the DB, in the order dictated by their Position. */
	const std::vector<TemplatePtr> & templates() const { return mTemplates; }

	/** Returns all filters stored in the DB, in the order dictated by their Position. */
	const std::vector<FilterPtr> & filters() const { return mFilters; }

	/** Creates a new empty template, adds it to the DB and returns it.
	Note that changes aren't saved automatically, you need to call saveTemplate() to save. */
	TemplatePtr createTemplate();

	/** Adds the specified template to the DB.
	Used for importing templates, asserts that the template has no DB RowID assigned to it yet.
	Modifies aTemplate - assigns a new DB RowID.
	Note that changes aren't saved automatically, you need to call saveTemplate() to save. */
	void addTemplate(TemplatePtr aTemplate);

	/** Swaps the positions of the two filters specified by the indices.
	Updates the new order in the DB.
	Asserts that the indices are valid. */
	void swapTemplatesByIdx(size_t aIdx1, size_t aIdx2);

	/** Removes the specified template from the DB. */
	void delTemplate(const TemplatePtr aTemplate) { delTemplate(aTemplate.get()); }

	/** Removes the specified template from the DB. */
	void delTemplate(const Template * aTemplate);

	/** Saves the changes in the specified template to the DB. */
	void saveTemplate(const Template & aTemplate);

	/** Saves all the templates to the DB. */
	void saveAllTemplates();

	/** Creates a new empty filter, adds it in the DB and returns it.
	Note that changes aren't saved automatically, you need to call saveFilter() to save. */
	FilterPtr createFilter();

	/** Adds the specified filter to the DB.
	Used for importing filters, asserts that the filter has no DB RowID assigned to it yet.
	Modifies aFilter - assigns a new DB RowID.
	Note that changes aren't saved automatically, you need to call saveFilter() to save. */
	void addFilter(FilterPtr aFilter);

	/** Swaps the positions of the two filters specified by the indices.
	Updates the new order in the DB.
	Asserts that the indices are valid. */
	void swapFiltersByIdx(size_t aIdx1, size_t aIdx2);

	/** Removes the filter at the specified index from the DB.
	Ignored if index is invalid. */
	void delFilter(size_t aIndex);

	/** Saves the specified filter into the DB. */
	void saveFilter(const Filter & aFilter);

	/** Returns the filter that has the specified hash.
	If there are multiple filters with the hash, returns a random one.
	Returns nullptr if there's no such filter. */
	FilterPtr filterFromHash(const QByteArray & aFilterHash);

	/** Returns all the filters that have been marked as "favorite". */
	std::vector<FilterPtr> getFavoriteFilters() const;

	/** Returns the number of songs that match the specified filter. */
	int numSongsMatchingFilter(const Filter & aFilter) const;

	/** Returns the number of templates that contain the specified filter.
	If a template contains the filter multiple times, only one usage is reported for that template. */
	int numTemplatesContaining(const Filter & aFilter) const;

	/** Picks a random song matching the specified filter.
	If possible, avoids aAvoid from being picked (picks it only if it is the only song matching the filter). */
	SongPtr pickSongForFilter(const Filter & aFilter, SongPtr aAvoid = nullptr) const;

	/** Picks random songs matching the specified template.
	Returns pairs of {song, filter} for all matches in the template. */
	std::vector<std::pair<SongPtr, FilterPtr>> pickSongsForTemplate(const Template & aTemplate);

	/** Reads and returns all the songs that have been removed from the library. */
	std::vector<RemovedSongPtr> removedSongs() const;

	/** Removes all entries from the history of removed songs. */
	void clearRemovedSongs();

	/** Returns the SharedData for the specified song hash.
	Returns nullptr if no such SharedData was found. */
	Song::SharedDataPtr sharedDataFromHash(const QByteArray & aHash) const;

	/** Saves all songs and SharedData from the internal arrays to the DB. */
	void saveAllSongSharedData();

	/** Returns a map of all SongHash -> SongSharedData in the DB. */
	const std::map<QByteArray, Song::SharedDataPtr> & songSharedDataMap() const { return mSongSharedData; }

	/** Returns the entire playback history.
	Returns by-value, since the history is not normally kept in memory, so it needs to be read from DB in this call. */
	std::vector<HistoryItem> playbackHistory() const;

	/** Adds the items in aHistory to the playback history.
	Used primarily by the import. */
	void addPlaybackHistory(const std::vector<HistoryItem> & aHistory);

	/** Adds the items in aHistory to the song removal history in the DB.
	Used primarily by the import. */
	void addSongRemovalHistory(const std::vector<RemovedSongPtr> & aHistory);

	/** Loads all votes from the specified table and returns them, ordered by their DateAdded.
	Returns by-value, since the data is not normally kept in memory, so it needs to be read from the DB in this call. */
	std::vector<Vote> loadVotes(const QString & aTableName) const;

	/** Adds the specified votes into the specified DB table.
	Used primarily by the import. */
	void addVotes(const QString & aTableName, const std::vector<Vote> & aVotes);

	/** Removes songs whose files are not accessible.
	Keeps the SongSharedData. */
	quint32 removeInaccessibleSongs();

	/** Adds the specified values into the SharedData's Manual tag.
	Doesn't overwrite values, only adds into empty values.
	Hashes that don't exist are silently skipped.
	Saves all SharedData after applying all changes. */
	void addToSharedDataManualTags(const std::map<QByteArray /* hash */, Song::Tag> & aTags);


protected:

	friend class DatabaseUpgrade;


	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The DB connection .*/
	QSqlDatabase mDatabase;

	/** All the known songs. */
	std::vector<SongPtr> mSongs;

	/** The data shared among songs with equal hash. */
	std::map<QByteArray, Song::SharedDataPtr> mSongSharedData;

	/** All the filters that can be used for building templates. */
	std::vector<FilterPtr> mFilters;

	/** All the templates that can be used for filling the playlist. */
	std::vector<TemplatePtr> mTemplates;


	/** Loads all the songs in the DB into mSongs.
	Note that songs are not checked whether they exists on the disk or if their hash still fits.
	Use Song::isStillValid() for checking before adding the song to playlist / before starting playback. */
	void loadSongs();

	/** Loads the songs into mSongs vector.
	Songs that need metadata rescan are sent through needSongTagRescan() signal. */
	void loadSongFiles();

	/** Loads the data shared between songs with the same hash (mSongSharedData). */
	void loadSongSharedData();

	/** The new files stored in the DB are enqueued for hash calculation. */
	void loadNewFiles();

	/** Loads all the filters in the DB into mFilters. */
	void loadFilters();

	/** Loads the nodes belonging to the specified filter. */
	void loadFilterNodes(Filter & aFilter);

	/** Returns the filter corresponding to the specified DB RowId.
	Returns nullptr if no such filter. */
	FilterPtr filterFromRowId(qlonglong aRowId);

	/** Loads all the templates in the DB into mTemplates. */
	void loadTemplates();

	/** Loads the specified template from the DB.
	The template has its direct members initialized, this loads its items, matching from the available mFilters.
	If any filter is not found, the item is skipped. */
	void loadTemplateItems(Template & aTemplate);

	/** Recursively saves (inserts) the node subtree from aNode.
	aFilterRowId is the RowID of the filter to which the node belongs
	aParentNodeRowId is the RowID of the node that is the parent of aNode, -1 for root node. */
	void saveFilterNodes(
		qlonglong aFilterRowId,
		const Filter::Node & aNode,
		qlonglong aParentNodeRowId
	);

	/** Returns the internal Qt DB object.
	Only used in the DatabaseUpgrade class. */
	QSqlDatabase & database() { return mDatabase; }

	/** Returns the relative "weight" of a song, when choosing by a template.
	The weight is adjusted based on song's rating, last played date, etc.
	If aPlaylist is given, songs on the playlist are further reduced. */
	int getSongWeight(const Song & aSong, const Playlist * aPlaylist = nullptr) const;

	void addVote(
		const QByteArray & aSongHash,
		int aVoteValue,
		const QString & aTableName,
		DatedOptional<double> Song::Rating::* aDstRating
	);


signals:

	/** Emitted after a new song file is added to the list of songs (addSongFile, addSongFiles).
	NOT emitted when loading songs from the DB! */
	void songFileAdded(SongPtr aSong);

	/** Emitted just before a song is removed from the DB. */
	void songRemoving(SongPtr aSong, size_t aIndex);

	/** Emitted after a song is removed from the DB. */
	void songRemoved(SongPtr aSong, size_t aIndex);

	/** Emitted when a new file is added to song list that has no hash assigned to it. */
	void needFileHash(const QString & aFileName);

	/** Emitted after loading for each song shared data that has an invalid length (#141). */
	void needSongLength(Song::SharedDataPtr aSongSharedData);

	/** Emitted when encountering a song with hash but without tag in the DB.
	This can happen either at DB load time (open()), or when adding new songs (songHashCalculated()). */
	void needSongTagRescan(SongPtr aSong);

	/** Emitted after a song was saved, presumably because its data had changed.
	Used by clients to update the song properties in the UI. */
	void songSaved(SongPtr aSong);


public slots:

	/** Indicates that the song has started playing and the DB should store that info. */
	void songPlaybackStarted(SongPtr aSong);

	/** To be called when the song hash and length has been calculated.
	Moves the song from NewFiles to SongFiles, creates an in-memory Song object for the file.
	Prepares a row for the song hash in the SharedData table, or assigns an existing row to this song.
	Emits the songFileAdded signal. */
	void songHashCalculated(const QString & aFileName, const QByteArray & aHash, double aLength);

	/** To be called when the hash calculation fails.
	Removes the specified file from the DB table of new files. */
	void songHashFailed(const QString & aFileName);

	/** To be called when the song length has been calculated. */
	void songLengthCalculated(Song::SharedDataPtr aSharedData, double aLengthSec);

	/** Saves (updates) the specified song into the DB.
	Assumes (doesn't check) that the song is contained within this DB. */
	void saveSong(SongPtr aSong);

	/** Updates the song's file-related data in the DB. */
	void saveSongFileData(SongPtr aSong);

	/** Updates the song's shared data in the SongSharedData DB table. */
	void saveSongSharedData(Song::SharedDataPtr aSharedData);

	/** Emitted by mMetadataScanner after metadata is updated for the specified song.
	Writes the whole updated song to the DB. */
	void songScanned(SongPtr aSong);

	/** Adds a new RhythmClarity vote for the specified song hash into the DB
	Updates the aggregated rating in Song::SharedData and in the DB. */
	void addVoteRhythmClarity(QByteArray aSongHash, int aVoteValue);

	/** Adds a new GenreTypicality vote for the specified song hash into the DB
	Updates the aggregated rating in Song::SharedData and in the DB. */
	void addVoteGenreTypicality(QByteArray aSongHash, int aVoteValue);

	/** Adds a new Popularity vote for the specified song hash into the DB
	Updates the aggregated rating in Song::SharedData and in the DB. */
	void addVotePopularity(QByteArray aSongHash, int aVoteValue);


protected slots:

	/** Adds a new entry into the playback history in the DB. */
	void addPlaybackHistory(SongPtr aSong, const QDateTime & aTimestamp);

};





#endif // SONGDATABASE_H

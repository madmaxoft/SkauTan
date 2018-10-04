#include "DatabaseUpgrade.h"
#include <vector>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include "Database.h"





class VersionScript
{
public:
	VersionScript(std::vector<std::string> && a_Commands):
		m_Commands(std::move(a_Commands))
	{}

	std::vector<std::string> m_Commands;


	/** Applies this upgrade script to the specified DB, and updates its version to a_Version. */
	void apply(QSqlDatabase & a_DB, size_t a_Version) const
	{
		qDebug() << "Executing DB upgrade script to version " << a_Version;

		// Temporarily disable FKs:
		{
			auto query = a_DB.exec("pragma foreign_keys = off");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL query failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Begin transaction:
		{
			auto query = a_DB.exec("begin");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL query failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Execute the individual commands:
		for (const auto & cmd: m_Commands)
		{
			auto query = a_DB.exec(QString::fromStdString(cmd));
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL upgrade command failed: " << query.lastError();
				qDebug() << "  ^-- command: " << cmd.c_str();
				throw DatabaseUpgrade::SqlError(query.lastError(), cmd);
			}
		}

		// Set the new version:
		{
			auto query = a_DB.exec(QString("UPDATE Version SET Version = %1").arg(a_Version));
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Check the FK constraints:
		{
			auto query = a_DB.exec("pragma check_foreign_keys");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Commit the transaction:
		{
			auto query = a_DB.exec("commit");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Re-enable FKs:
		{
			auto query = a_DB.exec("pragma foreign_keys = on");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL query failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}
	}
};






static const std::vector<VersionScript> g_VersionScripts =
{
	// Version 0 (no versioning info / empty DB) to version 1:
	VersionScript({
		"CREATE TABLE IF NOT EXISTS SongHashes ("
			"FileName TEXT,"
			"FileSize NUMBER,"
			"Hash     BLOB"
		")",

		"CREATE TABLE IF NOT EXISTS SongMetadata ("
			"Hash                      BLOB PRIMARY KEY,"
			"Length                    NUMERIC,"
			"ManualAuthor              TEXT,"
			"ManualTitle               TEXT,"
			"ManualGenre               TEXT,"
			"ManualMeasuresPerMinute   NUMERIC,"
			"FileNameAuthor            TEXT,"
			"FileNameTitle             TEXT,"
			"FileNameGenre             TEXT,"
			"FileNameMeasuresPerMinute NUMERIC,"
			"ID3Author                 TEXT,"
			"ID3Title                  TEXT,"
			"ID3Genre                  TEXT,"
			"ID3MeasuresPerMinute      NUMERIC,"
			"LastPlayed                DATETIME,"
			"Rating                    NUMERIC,"
			"LastMetadataUpdated       DATETIME"
		")",

		"CREATE TABLE IF NOT EXISTS PlaybackHistory ("
			"SongHash  BLOB,"
			"Timestamp DATETIME"
		")",

		"CREATE TABLE IF NOT EXISTS Templates ("
			"RowID       INTEGER PRIMARY KEY,"
			"DisplayName TEXT,"
			"Notes       TEXT,"
			"BgColor     TEXT"  // "#rrggbb"
		")",

		"CREATE TABLE IF NOT EXISTS TemplateItems ("
			"RowID         INTEGER PRIMARY KEY,"
			"TemplateID    INTEGER,"
			"IndexInParent INTEGER,"  // The index of the Item within its Template
			"DisplayName   TEXT,"
			"Notes         TEXT,"
			"IsFavorite    INTEGER,"
			"BgColor       TEXT"  // "#rrggbb"
		")",

		"CREATE TABLE IF NOT EXISTS TemplateFilters ("
			"RowID        INTEGER PRIMARY KEY,"
			"ItemID       INTEGER,"  // RowID of the TemplateItem that owns this filter
			"TemplateID   INTEGER,"  // RowID of the Template that owns this filter's item
			"ParentID     INTEGER,"  // RowID of this filter's parent, or -1 if root
			"Kind         INTEGER,"  // Numeric representation of the Template::Filter::Kind enum
			"SongProperty INTEGER,"  // Numeric representation of the Template::Filter::SongProperty enum
			"Comparison   INTEGER,"  // Numeric representation of the Template::Filter::Comparison enum
			"Value        TEXT"     // The value against which to compare
		")",

		"CREATE TABLE IF NOT EXISTS Version ("
			"Version INTEGER"
		")",

		"INSERT INTO Version (Version) VALUES (1)",
	}),  // Version 0 to Version 1


	// Version 1 to Version 2:
	// Reorganize the data so that file-related data is with the filename, and song-related data is with the hash (#94)
	VersionScript({
		"CREATE TABLE SongFiles ("
			"FileName                  TEXT PRIMARY KEY,"
			"FileSize                  NUMERIC,"
			"Hash                      BLOB,"
			"ManualAuthor              TEXT,"
			"ManualTitle               TEXT,"
			"ManualGenre               TEXT,"
			"ManualMeasuresPerMinute   NUMERIC,"
			"FileNameAuthor            TEXT,"
			"FileNameTitle             TEXT,"
			"FileNameGenre             TEXT,"
			"FileNameMeasuresPerMinute NUMERIC,"
			"ID3Author                 TEXT,"
			"ID3Title                  TEXT,"
			"ID3Genre                  TEXT,"
			"ID3MeasuresPerMinute      NUMERIC,"
			"LastTagRescanned          DATETIME DEFAULT NULL,"
			"NumTagRescanAttempts      NUMERIC DEFAULT 0"
		")",

		"INSERT INTO SongFiles ("
			"FileName, FileSize, Hash,"
			"ManualAuthor, ManualTitle, ManualGenre, ManualMeasuresPerMinute,"
			"FileNameAuthor, FileNameTitle, FileNameGenre, FileNameMeasuresPerMinute,"
			"ID3Author, ID3Title, ID3Genre, ID3MeasuresPerMinute"
			") SELECT "
		"SongHashes.FileName AS FileName,"
		"SongHashes.FileSize AS FileSize,"
		"SongHashes.Hash AS Hash,"
		"SongMetadata.ManualAuthor AS ManualAuthor,"
		"SongMetadata.ManualTitle AS ManualTitle,"
		"SongMetadata.ManualGenre AS ManualGenre,"
		"SongMetadata.ManualMeasuresPerMinute AS ManualMeasuresPerMinute,"
		"SongMetadata.FileNameAuthor AS FileNameAuthor,"
		"SongMetadata.FileNameTitle AS FileNameTitle,"
		"SongMetadata.FileNameGenre AS FileNameGenre,"
		"SongMetadata.FileNameMeasuresPerMinute AS FileNameMeasuresPerMinute,"
		"SongMetadata.ID3Author AS ID3Author,"
		"SongMetadata.ID3Title AS ID3Title,"
		"SongMetadata.ID3Genre AS ID3Genre,"
		"SongMetadata.ID3MeasuresPerMinute AS ID3MeasuresPerMinute "
		"FROM SongHashes LEFT JOIN SongMetadata ON SongHashes.Hash == SongMetadata.Hash",

		"CREATE TABLE SongSharedData ("
			"Hash       BLOB PRIMARY KEY,"
			"Length     NUMERIC,"
			"LastPlayed DATETIME,"
			"Rating     NUMERIC"
		")",

		"INSERT INTO SongSharedData("
			"Hash,"
			"Length,"
			"LastPlayed,"
			"Rating"
		") SELECT "
			"Hash,"
			"Length,"
			"LastPlayed,"
			"Rating "
		"FROM SongMetadata",

		"DROP TABLE SongMetadata",

		"DROP TABLE SongHashes",
	}),  // Version 1 to Version 2


	// Version 2 to Version 3:
	// Add a last-modified field for each manual song prop:
	VersionScript({
		"ALTER TABLE SongFiles ADD COLUMN ManualAuthorLM DATETIME",
		"ALTER TABLE SongFiles ADD COLUMN ManualTitleLM DATETIME",
		"ALTER TABLE SongFiles ADD COLUMN ManualGenreLM DATETIME",
		"ALTER TABLE SongFiles ADD COLUMN ManualMeasuresPerMinuteLM DATETIME",
	}),  // Version 2 to Version 3


	// Version 3 to Version 4:
	// Split rating into categories:
	VersionScript({
		"ALTER TABLE SongSharedData RENAME TO SongSharedData_Old",

		"CREATE TABLE SongSharedData ("
			"Hash                    BLOB PRIMARY KEY,"
			"Length                  NUMERIC,"
			"LastPlayed              DATETIME,"
			"LocalRating             NUMERIC,"
			"LocalRatingLM           DATETIME,"
			"RatingRhythmClarity     NUMERIC,"
			"RatingRhythmClarityLM   DATETIME,"
			"RatingGenreTypicality   NUMERIC,"
			"RatingGenreTypicalityLM DATETIME,"
			"RatingPopularity        NUMERIC,"
			"RatingPopularityLM      DATETIME"
		")",

		"INSERT INTO SongSharedData("
			"Hash, Length, LastPlayed,"
			"LocalRating, LocalRatingLM,"
			"RatingRhythmClarity,   RatingRhythmClarityLM,"
			"RatingGenreTypicality, RatingGenreTypicalityLM,"
			"RatingPopularity,      RatingPopularityLM"
		") SELECT "
			"Hash, Length, LastPlayed,"
			"Rating, NULL,"
			"Rating, NULL,"
			"Rating, NULL,"
			"Rating, NULL"
		" FROM SongSharedData_Old",

		"DROP TABLE SongSharedData_Old",
	}),  // Version 3 to Version 4


	// Version 5 to Version 5:
	// Add song skip-start to shared data (#69):
	VersionScript({
		"ALTER TABLE SongSharedData ADD COLUMN SkipStart NUMERIC",
		"ALTER TABLE SongSharedData ADD COLUMN SkipStartLM DATETIME",
	}),  // Version 4 to Version 5


	// Version 5 to Version 6:
	// Add Notes to SongFiles (#137):
	VersionScript({
		"ALTER TABLE SongFiles ADD COLUMN Notes TEXT",
		"ALTER TABLE SongFiles ADD COLUMN NotesLM DATETIME",
	}),  // Version 5 to Version 6


	// Version 6 to Version 7:
	// Add LimitDuration to template filters:
	VersionScript({
		"ALTER TABLE TemplateItems ADD COLUMN DurationLimit NUMERIC",
	}),


	// Version 7 to Version 8:
	// Split TagManual into separate tables:
	VersionScript({
		"CREATE TEMPORARY VIEW ManualAuthor ("
			"Hash,"
			"Author,"
			"LastMod"
		") AS SELECT "
			"Hash,"
			"ManualAuthor,"
			"max(ManualAuthorLM) "
		"FROM SongFiles "
		"WHERE ManualAuthor IS NOT NULL "
		"GROUP BY Hash",

		"CREATE TEMPORARY VIEW ManualTitle ("
			"Hash,"
			"Title,"
			"LastMod"
		") AS SELECT "
			"Hash,"
			"ManualTitle,"
			"max(ManualTitleLM) "
		"FROM SongFiles "
		"WHERE ManualTitle IS NOT NULL "
		"GROUP BY Hash",

		"CREATE TEMPORARY VIEW ManualGenre ("
			"Hash,"
			"Genre,"
			"LastMod"
		") AS SELECT "
			"Hash,"
			"ManualGenre,"
			"max(ManualGenreLM) "
		"FROM SongFiles "
		"WHERE ManualGenre IS NOT NULL "
		"GROUP BY Hash",

		"CREATE TEMPORARY VIEW ManualMeasuresPerMinute ("
			"Hash,"
			"MeasuresPerMinute,"
			"LastMod"
		") AS SELECT "
			"Hash,"
			"ManualMeasuresPerMinute,"
			"max(ManualMeasuresPerMinuteLM) "
		"FROM SongFiles "
		"WHERE ManualMeasuresPerMinute IS NOT NULL "
		"GROUP BY Hash",

		"ALTER TABLE SongSharedData RENAME TO SongSharedData_Old",

		"CREATE TABLE SongSharedData ("
			"Hash                      BLOB PRIMARY KEY,"
			"Length                    NUMERIC,"
			"LastPlayed                DATETIME,"
			"LocalRating               NUMERIC,"
			"LocalRatingLM             DATETIME,"
			"RatingRhythmClarity       NUMERIC,"
			"RatingRhythmClarityLM     DATETIME,"
			"RatingGenreTypicality     NUMERIC,"
			"RatingGenreTypicalityLM   DATETIME,"
			"RatingPopularity          NUMERIC,"
			"RatingPopularityLM        DATETIME,"
			"ManualAuthor              TEXT,"
			"ManualAuthorLM            DATETIME,"
			"ManualTitle               TEXT,"
			"ManualTitleLM             DATETIME,"
			"ManualGenre               TEXT,"
			"ManualGenreLM             DATETIME,"
			"ManualMeasuresPerMinute   TEXT,"
			"ManualMeasuresPerMinuteLM DATETIME,"
			"SkipStart NUMERIC,"
			"SkipStartLM DATETIME"
		")",

		"INSERT INTO SongSharedData("
			"Hash, Length, LastPlayed,"
			"LocalRating, LocalRatingLM,"
			"RatingRhythmClarity,   RatingRhythmClarityLM,"
			"RatingGenreTypicality, RatingGenreTypicalityLM,"
			"RatingPopularity,      RatingPopularityLM,"
			"ManualAuthor,            ManualAuthorLM,"
			"ManualTitle,             ManualTitleLM,"
			"ManualGenre,             ManualGenreLM,"
			"ManualMeasuresPerMinute, ManualMeasuresPerMinuteLM,"
			"SkipStart, SkipStartLM"
		") SELECT "
			"SongSharedData_Old.Hash, SongSharedData_Old.Length, SongSharedData_Old.LastPlayed,"
			"SongSharedData_Old.LocalRating, SongSharedData_Old.LocalRatingLM,"
			"SongSharedData_Old.RatingRhythmClarity,   SongSharedData_Old.RatingRhythmClarityLM,"
			"SongSharedData_Old.RatingGenreTypicality, SongSharedData_Old.RatingGenreTypicalityLM,"
			"SongSharedData_Old.RatingPopularity,      SongSharedData_Old.RatingPopularityLM,"
			"ManualAuthor.Author,                       ManualAuthor.LastMod,"
			"ManualTitle.Title,                         ManualTitle.LastMod,"
			"ManualGenre.Genre,                         ManualGenre.LastMod,"
			"ManualMeasuresPerMinute.MeasuresPerMinute, ManualMeasuresPerMinute.LastMod,"
			"SongSharedData_Old.SkipStart, SongSharedData_Old.SkipStartLM "
		"FROM SongSharedData_Old "
		"LEFT JOIN ManualAuthor            ON ManualAuthor.Hash = SongSharedData_Old.Hash "
		"LEFT JOIN ManualTitle             ON ManualTitle.Hash = SongSharedData_Old.Hash "
		"LEFT JOIN ManualGenre             ON ManualGenre.Hash = SongSharedData_Old.Hash "
		"LEFT JOIN ManualMeasuresPerMinute ON ManualMeasuresPerMinute.Hash = SongSharedData_Old.Hash",

		"DROP TABLE SongSharedData_Old",

		"DROP VIEW ManualAuthor",
		"DROP VIEW ManualTitle",
		"DROP VIEW ManualGenre",
		"DROP VIEW ManualMeasuresPerMinute",

		"ALTER TABLE SongFiles RENAME TO SongFiles_Old",

		"CREATE TABLE SongFiles ("
			"FileName                  TEXT PRIMARY KEY,"
			"FileSize                  NUMERIC,"
			"Hash                      BLOB,"
			"FileNameAuthor            TEXT,"
			"FileNameTitle             TEXT,"
			"FileNameGenre             TEXT,"
			"FileNameMeasuresPerMinute NUMERIC,"
			"ID3Author                 TEXT,"
			"ID3Title                  TEXT,"
			"ID3Genre                  TEXT,"
			"ID3MeasuresPerMinute      NUMERIC,"
			"LastTagRescanned          DATETIME DEFAULT NULL,"
			"NumTagRescanAttempts      NUMERIC DEFAULT 0,"
			"Notes                     TEXT,"
			"NotesLM                   DATETIME"
		")",

		"INSERT INTO SongFiles("
			"FileName,"
			"FileSize,"
			"Hash,"
			"FileNameAuthor,"
			"FileNameTitle,"
			"FileNameGenre,"
			"FileNameMeasuresPerMinute,"
			"ID3Author,"
			"ID3Title,"
			"ID3Genre,"
			"ID3MeasuresPerMinute,"
			"LastTagRescanned,"
			"NumTagRescanAttempts,"
			"Notes,"
			"NotesLM"
		") SELECT "
			"FileName,"
			"FileSize,"
			"Hash,"
			"FileNameAuthor,"
			"FileNameTitle,"
			"FileNameGenre,"
			"FileNameMeasuresPerMinute,"
			"ID3Author,"
			"ID3Title,"
			"ID3Genre,"
			"ID3MeasuresPerMinute,"
			"LastTagRescanned,"
			"NumTagRescanAttempts,"
			"Notes,"
			"NotesLM "
		"FROM SongFiles_Old",

		"DROP TABLE SongFiles_Old",
	}),  // Version 7 to Version 8


	// Version 8 to Version 9:
	// Move the Notes from SongFiles to SongSharedData:
	VersionScript({
		"CREATE TEMPORARY VIEW SongNotes ("
			"Hash,"
			"Notes,"
			"LastMod"
		") AS SELECT "
			"Hash,"
			"Notes,"
			"max(NotesLM)"
		"FROM SongFiles WHERE Notes IS NOT NULL "
		"GROUP BY Hash",

		"ALTER TABLE SongSharedData RENAME TO SongSharedData_Old",

		"CREATE TABLE SongSharedData ("
			"Hash                      BLOB PRIMARY KEY,"
			"Length                    NUMERIC,"
			"LastPlayed                DATETIME,"
			"LocalRating               NUMERIC,"
			"LocalRatingLM             DATETIME,"
			"RatingRhythmClarity       NUMERIC,"
			"RatingRhythmClarityLM     DATETIME,"
			"RatingGenreTypicality     NUMERIC,"
			"RatingGenreTypicalityLM   DATETIME,"
			"RatingPopularity          NUMERIC,"
			"RatingPopularityLM        DATETIME,"
			"ManualAuthor              TEXT,"
			"ManualAuthorLM            DATETIME,"
			"ManualTitle               TEXT,"
			"ManualTitleLM             DATETIME,"
			"ManualGenre               TEXT,"
			"ManualGenreLM             DATETIME,"
			"ManualMeasuresPerMinute   TEXT,"
			"ManualMeasuresPerMinuteLM DATETIME,"
			"SkipStart                 NUMERIC,"
			"SkipStartLM               DATETIME,"
			"Notes                     TEXT,"
			"NotesLM                   DATETIME"
		")",

		"INSERT INTO SongSharedData("
			"Hash, Length, LastPlayed,"
			"LocalRating, LocalRatingLM,"
			"RatingRhythmClarity,   RatingRhythmClarityLM,"
			"RatingGenreTypicality, RatingGenreTypicalityLM,"
			"RatingPopularity,      RatingPopularityLM,"
			"ManualAuthor,            ManualAuthorLM,"
			"ManualTitle,             ManualTitleLM,"
			"ManualGenre,             ManualGenreLM,"
			"ManualMeasuresPerMinute, ManualMeasuresPerMinuteLM,"
			"SkipStart, SkipStartLM,"
			"Notes, NotesLM"
		") SELECT "
			"SongSharedData_Old.Hash, SongSharedData_Old.Length, SongSharedData_Old.LastPlayed,"
			"SongSharedData_Old.LocalRating, SongSharedData_Old.LocalRatingLM,"
			"SongSharedData_Old.RatingRhythmClarity,   SongSharedData_Old.RatingRhythmClarityLM,"
			"SongSharedData_Old.RatingGenreTypicality, SongSharedData_Old.RatingGenreTypicalityLM,"
			"SongSharedData_Old.RatingPopularity,      SongSharedData_Old.RatingPopularityLM,"
			"SongSharedData_Old.ManualAuthor,            SongSharedData_Old.ManualAuthorLM,"
			"SongSharedData_Old.ManualTitle,             SongSharedData_Old.ManualTitleLM,"
			"SongSharedData_Old.ManualGenre,             SongSharedData_Old.ManualGenreLM,"
			"SongSharedData_Old.ManualMeasuresPerMinute, SongSharedData_Old.ManualMeasuresPerMinuteLM,"
			"SongSharedData_Old.SkipStart, SongSharedData_Old.SkipStartLM, "
			"SongNotes.Notes, SongNotes.LastMod "
		"FROM SongSharedData_Old "
		"LEFT JOIN SongNotes ON SongNotes.Hash = SongSharedData_Old.Hash",

		"DROP TABLE SongSharedData_Old",

		"DROP VIEW SongNotes",

		"ALTER TABLE SongFiles RENAME TO SongFiles_Old",

		"CREATE TABLE SongFiles ("
			"FileName                  TEXT PRIMARY KEY,"
			"FileSize                  NUMERIC,"
			"Hash                      BLOB,"
			"FileNameAuthor            TEXT,"
			"FileNameTitle             TEXT,"
			"FileNameGenre             TEXT,"
			"FileNameMeasuresPerMinute NUMERIC,"
			"ID3Author                 TEXT,"
			"ID3Title                  TEXT,"
			"ID3Genre                  TEXT,"
			"ID3MeasuresPerMinute      NUMERIC,"
			"LastTagRescanned          DATETIME DEFAULT NULL,"
			"NumTagRescanAttempts      NUMERIC DEFAULT 0"
		")",

		"INSERT INTO SongFiles("
			"FileName,"
			"FileSize,"
			"Hash,"
			"FileNameAuthor,"
			"FileNameTitle,"
			"FileNameGenre,"
			"FileNameMeasuresPerMinute,"
			"ID3Author,"
			"ID3Title,"
			"ID3Genre,"
			"ID3MeasuresPerMinute,"
			"LastTagRescanned,"
			"NumTagRescanAttempts"
		") SELECT "
			"FileName,"
			"FileSize,"
			"Hash,"
			"FileNameAuthor,"
			"FileNameTitle,"
			"FileNameGenre,"
			"FileNameMeasuresPerMinute,"
			"ID3Author,"
			"ID3Title,"
			"ID3Genre,"
			"ID3MeasuresPerMinute,"
			"LastTagRescanned,"
			"NumTagRescanAttempts "
		"FROM SongFiles_Old",

		"DROP TABLE SongFiles_Old",
	}),  // Version 8 to Version 9


	// Version 9 to Version 10:
	// New files are put into a queue until their hash is calculated, only then added to SongFiles (#25)
	// Drop Song FileSize (it's useless and not updated on file change)
	VersionScript({
		"CREATE TABLE NewFiles ("
			"FileName TEXT PRIMARY KEY"
		")",

		"INSERT INTO NewFiles (FileName) "
		"SELECT FileName FROM SongFiles WHERE Hash IS NULL",

		"DELETE FROM SongFiles WHERE Hash IS NULL",

		"ALTER TABLE SongFiles RENAME TO SongFiles_Old",

		"CREATE TABLE SongFiles ("
			"FileName                  TEXT PRIMARY KEY,"
			"Hash                      BLOB NOT NULL REFERENCES SongSharedData(Hash),"
			"FileNameAuthor            TEXT,"
			"FileNameTitle             TEXT,"
			"FileNameGenre             TEXT,"
			"FileNameMeasuresPerMinute NUMERIC,"
			"ID3Author                 TEXT,"
			"ID3Title                  TEXT,"
			"ID3Genre                  TEXT,"
			"ID3MeasuresPerMinute      NUMERIC,"
			"LastTagRescanned          DATETIME DEFAULT NULL,"
			"NumTagRescanAttempts      NUMERIC DEFAULT 0"
		")",

		"INSERT INTO SongFiles("
			"FileName,"
			"Hash,"
			"FileNameAuthor,"
			"FileNameTitle,"
			"FileNameGenre,"
			"FileNameMeasuresPerMinute,"
			"ID3Author,"
			"ID3Title,"
			"ID3Genre,"
			"ID3MeasuresPerMinute,"
			"LastTagRescanned,"
			"NumTagRescanAttempts"
		") SELECT "
			"FileName,"
			"Hash,"
			"FileNameAuthor,"
			"FileNameTitle,"
			"FileNameGenre,"
			"FileNameMeasuresPerMinute,"
			"ID3Author,"
			"ID3Title,"
			"ID3Genre,"
			"ID3MeasuresPerMinute,"
			"LastTagRescanned,"
			"NumTagRescanAttempts "
		"FROM SongFiles_Old",

		"DROP TABLE SongFiles_Old",
	}),  // Version 9 to Version 10


	// Version 10 to Version 11
	// Add a log of removed and deleted songs
	VersionScript({
		"CREATE TABLE RemovedSongs ("
			"FileName       TEXT,"
			"Hash           BLOB REFERENCES SongSharedData(Hash),"
			"DateRemoved    DATETIME,"
			"WasFileDeleted INTEGER,"
			"NumDuplicates  INTEGER"
		")",
	}),  // Version 10 to Version 11


	// Version 11 to Version 12
	// Add a last modification date tracking for LastPlayed
	VersionScript({
		"ALTER TABLE SongSharedData ADD COLUMN LastPlayedLM DATETIME",
	}),  // Version 11 to Version 12


	// Version 12 to Version 13
	// Add a community voting
	VersionScript({
		"CREATE TABLE VotesRhythmClarity ("
			"SongHash BLOB,"
			"VoteValue INTEGER,"
			"DateAdded DATETIME"
		")",
		"CREATE TABLE VotesGenreTypicality ("
			"SongHash BLOB,"
			"VoteValue INTEGER,"
			"DateAdded DATETIME"
		")",
		"CREATE TABLE VotesPopularity ("
			"SongHash BLOB,"
			"VoteValue INTEGER,"
			"DateAdded DATETIME"
		")",
	}),  // Version 12 to Version 13
};





////////////////////////////////////////////////////////////////////////////////
// DatabaseUpgrade:

DatabaseUpgrade::DatabaseUpgrade(Database & a_DB):
	m_DB(a_DB.database())
{
}





void DatabaseUpgrade::upgrade(Database & a_DB)
{
	DatabaseUpgrade upg(a_DB);
	return upg.execute();
}





void DatabaseUpgrade::execute()
{
	auto version = getVersion();
	qDebug() << "DB is at version " << version;
	bool hasUpgraded = false;
	for (auto i = version; i < g_VersionScripts.size(); ++i)
	{
		qWarning() << "Upgrading DB to version" << i + 1;
		g_VersionScripts[i].apply(m_DB, i + 1);
		hasUpgraded = true;
	}

	// After upgrading, vacuum the leftover space:
	if (hasUpgraded)
	{
		auto query = m_DB.exec("VACUUM");
		if (query.lastError().type() != QSqlError::NoError)
		{
			throw SqlError(query.lastError(), "VACUUM");
		}
	}
}





size_t DatabaseUpgrade::getVersion()
{
	auto query = m_DB.exec("SELECT MAX(Version) AS Version FROM Version");
	if (!query.first())
	{
		return 0;
	}
	return query.record().value("Version").toULongLong();
}





////////////////////////////////////////////////////////////////////////////////
// DatabaseUpgrade::SqlError:

DatabaseUpgrade::SqlError::SqlError(const QSqlError & a_SqlError, const std::string & a_SqlCommand):
	Super((std::string("DatabaseUpgrade::SqlError: ") + a_SqlError.text().toStdString()).c_str()),
	m_Error(a_SqlError),
	m_Command(a_SqlCommand)
{
}

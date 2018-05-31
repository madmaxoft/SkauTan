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

		// Begin transaction:
		auto query = a_DB.exec("begin");
		if (query.lastError().type() != QSqlError::NoError)
		{
			qWarning() << "SQL query failed: " << query.lastError();
			throw DatabaseUpgrade::SqlError(query.lastError(), "begin");
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

		// Commit the transaction:
		{
			auto query = a_DB.exec("commit");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), "commit");
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

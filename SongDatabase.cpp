#include "SongDatabase.h"
#include <assert.h>
#include <set>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>





/** Returns the SQL field's value, taking its "isNull" into account.
For some reason Qt + Sqlite return an empty string variant if the field is null,
this function returns a null variant in such a case. */
static QVariant fieldValue(const QSqlField & a_Field)
{
	if (a_Field.isNull())
	{
		return QVariant();
	}
	return a_Field.value();
}




////////////////////////////////////////////////////////////////////////////////
// SongDatabase:

SongDatabase::SongDatabase()
{
	connect(&m_MetadataScanner, &MetadataScanner::songScanned, this, &SongDatabase::songScanned);
}





void SongDatabase::open(const QString & a_DBFileName)
{
	assert(!m_Database.isOpen());  // Opening another DB is not allowed

	m_Database = QSqlDatabase::addDatabase("QSQLITE");
	m_Database.setDatabaseName(a_DBFileName);
	if (!m_Database.open())
	{
		qWarning() << __FUNCTION__ << ": Cannot open DB " << a_DBFileName << ": " << m_Database.lastError();
		return;
	}
	fixupTables();
	loadSongs();
	m_MetadataScanner.start();
}





void SongDatabase::addSongFiles(const std::vector<std::pair<QString, qulonglong>> & a_Files)
{
	for (const auto & f: a_Files)
	{
		addSongFile(f.first, f.second);
	}
}





void SongDatabase::addSongFile(const QString & a_FileName, qulonglong a_FileSize)
{
	// Check for duplicates:
	for (const auto & song: m_Songs)
	{
		if (song->fileName() == a_FileName)
		{
			qDebug() << __FUNCTION__ << ": Skipping duplicate " << a_FileName;
			return;
		}
	}

	// Insert into DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO Songs (FileName, FileSize) VALUES(?, ?)"))
	{
		qWarning() << __FUNCTION__ << ": Cannot prepare statement: " << query.lastError();
		return;
	}
	query.bindValue(0, a_FileName);
	query.bindValue(1, a_FileSize);
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot exec statement: " << query.lastError();
		return;
	}

	// Insert into memory:
	auto song = std::make_shared<Song>(a_FileName, a_FileSize, query.lastInsertId().toLongLong());
	m_Songs.push_back(song);
	emit songFileAdded(song.get());
}





void SongDatabase::fixupTables()
{
	static const std::vector<std::pair<QString, QString>> cdSongs =
	{
		{"FileName",            "TEXT"},
		{"FileSize",            "NUMBER"},
		{"Hash",                "BLOB"},
		{"Length",              "NUMBER"},
		{"Genre",               "TEXT"},
		{"MeasuresPerMinute",   "NUMBER"},
		{"LastPlayed",          "DATETIME"},
		{"Rating",              "NUMBER"},
		{"LastMetadataUpdated", "DATETIME"},
	};

	fixupTable("Songs", cdSongs);
}





void SongDatabase::fixupTable(const QString & a_TableName, const std::vector<std::pair<QString, QString> > & a_ColumnDefs)
{
	// Create the table, if it doesn't exist:
	QString colDefs;
	for (const auto & col: a_ColumnDefs)
	{
		if (!colDefs.isEmpty())
		{
			colDefs.push_back(',');
		}
		colDefs.append(QString("%1 %2").arg(col.first, col.second));
	}
	QString createQuery("CREATE TABLE IF NOT EXISTS %1 (%2)");
	createQuery = createQuery.arg(a_TableName, colDefs);
	QSqlQuery query(m_Database);
	if (!query.exec(createQuery))
	{
		qWarning() << __FUNCTION__
			<< ": Failed to fixup table " << a_TableName
			<< ", query CreateTable: " << createQuery
			<< ": " << query.lastError();
	}

	// Check for existing columns:
	std::set<QString> columns;
	if (!query.exec(QString("PRAGMA table_info(%1)").arg(a_TableName)))
	{
		qWarning() << __FUNCTION__
			<< ": Failed to query columns for table " << a_TableName
			<< ": " << query.lastError();
		return;
	}
	while (query.next())
	{
		columns.insert(query.value(1).toString());
	}

	// Add missing columns:
	for (const auto & col: a_ColumnDefs)
	{
		if (columns.find(col.first) != columns.end())
		{
			// Column already exists
			continue;
		}
		qDebug() << __FUNCTION__ << ": Adding column " << a_TableName << "." << col.first << " " << col.second;
		if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
			.arg(a_TableName, col.first, col.second))
		)
		{
			qWarning() << __FUNCTION__
				<< ": Failed to add column " << col.first
				<< " to table " << a_TableName
				<< ": " << query.lastError();
		}
	}
}





void SongDatabase::loadSongs()
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT RowID, * FROM Songs"))
	{
		qWarning() << __FUNCTION__ << ": Cannot query songs from the DB: " << query.lastError();
		return;
	}
	auto fiFileName            = query.record().indexOf("FileName");
	auto fiFileSize            = query.record().indexOf("FileSize");
	auto fiRowId               = query.record().indexOf("RowID");
	auto fiHash                = query.record().indexOf("Hash");
	auto fiLength              = query.record().indexOf("Length");
	auto fiGenre               = query.record().indexOf("Genre");
	auto fiMeasuresPerMinute   = query.record().indexOf("MeasuresPerMinute");
	auto fiLastPlayed          = query.record().indexOf("LastPlayed");
	auto fiRating              = query.record().indexOf("Rating");
	auto fiLastMetadataUpdated = query.record().indexOf("LastMetadataUpdated");

	// Load each song:
	if (!query.isActive())
	{
		qWarning() << __FUNCTION__ << ": Query not active";
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << __FUNCTION__ << ": Not a select";
		return;
	}
	while (query.next())
	{
		const auto & rec = query.record();
		auto song = std::make_shared<Song>(
			std::move(query.value(fiFileName).toString()),
			query.value(fiFileSize).toULongLong(),
			query.value(fiRowId).toLongLong(),
			std::move(fieldValue(rec.field(fiHash))),
			std::move(fieldValue(rec.field(fiLength))),
			std::move(fieldValue(rec.field(fiGenre))),
			std::move(fieldValue(rec.field(fiMeasuresPerMinute))),
			std::move(fieldValue(rec.field(fiLastPlayed))),
			std::move(fieldValue(rec.field(fiRating))),
			std::move(fieldValue(rec.field(fiLastMetadataUpdated)))
		);
		m_Songs.push_back(song);
		if (song->needsMetadataRescan())
		{
			m_MetadataScanner.scan(song);
		}
	}
}





void SongDatabase::saveSong(const Song & a_Song)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Songs SET "
		"FileName = ?, FileSize = ?, Hash = ?, "
		"Length = ?, Genre = ?, MeasuresPerMinute = ?, "
		"LastPlayed = ?, Rating = ?, LastMetadataUpdated = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << __FUNCTION__ << ": Cannot prepare statement: " << query.lastError();
		return;
	}
	query.bindValue(0, a_Song.fileName());
	query.bindValue(1, a_Song.fileSize());
	query.bindValue(2, a_Song.hash());
	query.bindValue(3, a_Song.length());
	query.bindValue(4, a_Song.genre());
	query.bindValue(5, a_Song.measuresPerMinute());
	query.bindValue(6, a_Song.lastPlayed());
	query.bindValue(7, a_Song.rating());
	query.bindValue(8, a_Song.lastMetadataUpdated());
	query.bindValue(9, a_Song.dbRowId());
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot exec statement: " << query.lastError();
		return;
	}
}





void SongDatabase::songScanned(Song * a_Song)
{
	a_Song->setLastMetadataUpdated(QDateTime::currentDateTimeUtc());
	saveSong(*a_Song);
}





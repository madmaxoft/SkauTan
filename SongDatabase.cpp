#include "SongDatabase.h"
#include <set>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>





SongDatabase::SongDatabase()
{
}





void SongDatabase::open(const QString & a_DBFileName)
{
	m_Database = QSqlDatabase::addDatabase("QSQLITE");
	m_Database.setDatabaseName(a_DBFileName);
	if (!m_Database.open())
	{
		qWarning() << __FUNCTION__ << ": Cannot open DB " << a_DBFileName << ": " << m_Database.lastError();
		return;
	}
	fixupTables();
	loadSongs();
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
		{"FileName",          "TEXT"},
		{"FileSize",          "NUMBER"},
		{"Hash",              "BLOB"},
		{"Length",            "NUMBER"},
		{"Genre",             "TEXT"},
		{"MeasuresPerMinute", "NUMBER"},
		{"LastPlayed",        "DATETIME"},
		{"Rating",            "NUMBER"},
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
	auto fiFileName          = query.record().indexOf("FileName");
	auto fiFileSize          = query.record().indexOf("FileSize");
	auto fiRowId             = query.record().indexOf("RowID");
	auto fiHash              = query.record().indexOf("Hash");
	auto fiLength            = query.record().indexOf("Length");
	auto fiGenre             = query.record().indexOf("Genre");
	auto fiMeasuresPerMinute = query.record().indexOf("MeasuresPerMinute");
	auto fiLastPlayed        = query.record().indexOf("LastPlayed");
	auto fiRating            = query.record().indexOf("Rating");

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
		m_Songs.push_back(std::make_shared<Song>(
			std::move(query.value(fiFileName).toString()),
			query.value(fiFileSize).toULongLong(),
			query.value(fiRowId).toLongLong(),
			std::move(query.value(fiHash).toByteArray()),
			query.value(fiLength),
			std::move(query.value(fiGenre).toString()),
			query.value(fiMeasuresPerMinute),
			query.value(fiLastPlayed).toDateTime(),
			query.value(fiRating)
		));
	}
}





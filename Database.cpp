#include "Database.h"
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

Database::Database()
{
	connect(&m_MetadataScanner, &MetadataScanner::songScanned, this, &Database::songScanned);
}





void Database::open(const QString & a_DBFileName)
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
	loadTemplates();
}





void Database::addSongFiles(const std::vector<std::pair<QString, qulonglong>> & a_Files)
{
	for (const auto & f: a_Files)
	{
		addSongFile(f.first, f.second);
	}
}





void Database::addSongFile(const QString & a_FileName, qulonglong a_FileSize)
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





void Database::saveSong(const Song & a_Song)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Songs SET "
		"FileName = ?, FileSize = ?, Hash = ?, "
		"Length = ?, Genre = ?, MeasuresPerMinute = ?, "
		"Author = ?, Title = ?, "
		"LastPlayed = ?, Rating = ?, LastMetadataUpdated = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << __FUNCTION__ << ": Cannot prepare statement: " << query.lastError();
		return;
	}
	query.addBindValue(a_Song.fileName());
	query.addBindValue(a_Song.fileSize());
	query.addBindValue(a_Song.hash());
	query.addBindValue(a_Song.length());
	query.addBindValue(a_Song.genre());
	query.addBindValue(a_Song.measuresPerMinute());
	query.addBindValue(a_Song.author());
	query.addBindValue(a_Song.title());
	query.addBindValue(a_Song.lastPlayed());
	query.addBindValue(a_Song.rating());
	query.addBindValue(a_Song.lastMetadataUpdated());
	query.addBindValue(a_Song.dbRowId());
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot exec statement: " << query.lastError();
		return;
	}
}





TemplatePtr Database::addTemplate()
{
	// Insert into DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO Templates (DisplayName, Notes) VALUES(?, ?)"))
	{
		qWarning() << __FUNCTION__ << ": Cannot prepare statement: " << query.lastError();
		return nullptr;
	}
	query.bindValue(0, QString());
	query.bindValue(1, QString());
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot exec statement: " << query.lastError();
		return nullptr;
	}

	// Insert into memory:
	auto res = std::make_shared<Template>(query.lastInsertId().toLongLong());
	m_Templates.push_back(res);
	return res;
}





void Database::delTemplate(const Template * a_Template)
{
	for (auto itr = m_Templates.cbegin(), end = m_Templates.cend(); itr != end; ++itr)
	{
		if (itr->get() == a_Template)
		{
			m_Templates.erase(itr);
			return;
		}
	}
}





void Database::saveTemplate(const Template & a_Template)
{
	// Save the template direct values:
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Templates SET "
		"DisplayName = ?, Notes = ?"
		"WHERE RowID = ?")
	)
	{
		qWarning() << __FUNCTION__ << ": Cannot prepare statement: " << query.lastError();
		return;
	}
	query.addBindValue(a_Template.displayName());
	query.addBindValue(a_Template.notes());
	query.addBindValue(a_Template.dbRowId());
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot exec statement: " << query.lastError();
		return;
	}

	int idx = 0;
	for (const auto & item: a_Template.items())
	{
		saveTemplateItem(a_Template, idx, *item);
		idx += 1;
	}
}





std::vector<Template::ItemPtr> Database::getFavoriteTemplateItems() const
{
	std::vector<Template::ItemPtr> res;
	for (const auto & tmpl: m_Templates)
	{
		for (const auto & item: tmpl->items())
		{
			if (item->isFavorite())
			{
				res.push_back(item);
			}
		}
	}
	return res;
}





void Database::fixupTables()
{
	/* For songs, we want unique RowID keys even after deletion, so that bound data doesn't get re-assigned
	to newly added songs.
	See the SQLite docs for details: https://sqlite.org/autoinc.html
	*/
	static const std::vector<std::pair<QString, QString>> cdSongs =
	{
		{"RowID",               "INTEGER PRIMARY KEY AUTOINCREMENT"},
		{"FileName",            "TEXT"},
		{"FileSize",            "NUMBER"},
		{"Hash",                "BLOB"},
		{"Length",              "NUMBER"},
		{"Genre",               "TEXT"},
		{"MeasuresPerMinute",   "NUMBER"},
		{"LastPlayed",          "DATETIME"},
		{"Rating",              "NUMBER"},
		{"Author",              "TEXT"},
		{"Title",               "TEXT"},
		{"LastMetadataUpdated", "DATETIME"},
	};

	static const std::vector<std::pair<QString, QString>> cdPlaybackHistory =
	{
		{"SongID",    "NUMBER"},
		{"Timestamp", "DATETIME"},
	};

	static const std::vector<std::pair<QString, QString>> cdTemplates =
	{
		{"RowID",       "INTEGER PRIMARY KEY"},
		{"DisplayName", "TEXT"},
		{"Notes",       "TEXT"},
	};

	static const std::vector<std::pair<QString, QString>> cdTemplateItems =
	{
		{"RowID",         "INTEGER PRIMARY KEY"},
		{"TemplateID",    "INTEGER"},
		{"IndexInParent", "INTEGER"},  // The index of the Item within its Template
		{"DisplayName",   "TEXT"},
		{"Notes",         "TEXT"},
		{"IsFavorite",    "INTEGER"},
	};

	static const std::vector<std::pair<QString, QString>> cdTemplateFilters =
	{
		{"RowID",        "INTEGER PRIMARY KEY"},
		{"ItemID",       "INTEGER"},  // RowID of the TemplateItem that owns this filter
		{"ParentID",     "INTEGER"},  // RowID of this filter's parent, or -1 if root
		{"Kind",         "INTEGER"},  // Numeric representation of the Template::Filter::Kind enum
		{"SongProperty", "INTEGER"},  // Numeric representation of the Template::Filter::SongProperty enum
		{"Comparison",   "INTEGER"},  // Numeric representation of the Template::Filter::Comparison enum
		{"Value",        "TEXT"},     // The value against which to compare
	};

	fixupTable("Songs",           cdSongs);
	fixupTable("PlaybackHistory", cdPlaybackHistory);
	fixupTable("Templates",       cdTemplates);
	fixupTable("TemplateItems",   cdTemplateItems);
	fixupTable("TemplateFilters", cdTemplateFilters);
}





void Database::fixupTable(const QString & a_TableName, const std::vector<std::pair<QString, QString> > & a_ColumnDefs)
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





void Database::loadSongs()
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
	auto fiAuthor              = query.record().indexOf("Author");
	auto fiTitle               = query.record().indexOf("Title");
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
			std::move(fieldValue(rec.field(fiAuthor))),
			std::move(fieldValue(rec.field(fiTitle))),
			std::move(fieldValue(rec.field(fiLastPlayed))),
			std::move(fieldValue(rec.field(fiRating))),
			std::move(fieldValue(rec.field(fiLastMetadataUpdated)))
		);
		m_Songs.push_back(song);
		if (song->needsMetadataRescan())
		{
			m_MetadataScanner.queueScan(song);
		}
	}
}





void Database::loadTemplates()
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT RowID, * FROM Templates"))
	{
		qWarning() << __FUNCTION__ << ": Cannot query templates from the DB: " << query.lastError();
		return;
	}
	auto fiDisplayName = query.record().indexOf("DisplayName");
	auto fiNotes       = query.record().indexOf("Notes");
	auto fiRowId       = query.record().indexOf("RowID");

	// Load each template:
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
		auto tmpl = std::make_shared<Template>(
			query.value(fiRowId).toLongLong(),
			std::move(query.value(fiDisplayName).toString()),
			std::move(query.value(fiNotes).toString())
		);
		m_Templates.push_back(tmpl);
		loadTemplate(tmpl);
	}
}





void Database::loadTemplate(TemplatePtr a_Template)
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT RowID, * FROM TemplateItems WHERE TemplateID = ? ORDER BY IndexInParent ASC"))
	{
		qWarning() << __FUNCTION__ << ": Cannot query template items from the DB, template "
			<< a_Template->displayName()
			<< ", prep error: " << query.lastError();
		return;
	}
	query.addBindValue(a_Template->dbRowId());
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot query template items from the DB, template "
			<< a_Template->displayName()
			<< ", exec error: " << query.lastError();
		return;
	}
	auto fiRowId       = query.record().indexOf("RowID");
	auto fiDisplayName = query.record().indexOf("DisplayName");
	auto fiNotes       = query.record().indexOf("Notes");
	auto fiIsFavorite  = query.record().indexOf("IsFavorite");

	// Load each template:
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
		auto item = a_Template->addItem(
			query.value(fiDisplayName).toString(),
			query.value(fiNotes).toString(),
			query.value(fiIsFavorite).toBool()
		);
		auto rowId = query.value(fiRowId).toLongLong();
		loadTemplateFilters(a_Template, rowId, *item);
		if (item->filter() == nullptr)
		{
			qWarning() << __FUNCTION__ << ": Empty filter detected in item "
				<< "item " << item->displayName()
				<< "(rowId " << rowId << "), changing to no-op filter";
			item->setNoopFilter();
		}
	}
}





void Database::loadTemplateFilters(TemplatePtr a_Template, qlonglong a_ItemRowId, Template::Item & a_Item)
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT RowID, * FROM TemplateFilters WHERE ItemID = ?"))
	{
		qWarning() << __FUNCTION__ << ": Cannot query template filters from the DB, "
			<< "template " << a_Template->displayName()
			<< ", item " << a_Item.displayName()
			<< ", prep error: " << query.lastError();
		return;
	}
	query.addBindValue(a_ItemRowId);
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot query template filters from the DB, "
			<< "template " << a_Template->displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		return;
	}
	auto fiRowId        = query.record().indexOf("RowID");
	auto fiParentRowId  = query.record().indexOf("ParentID");
	auto fiKind         = query.record().indexOf("Kind");
	auto fiComparison   = query.record().indexOf("Comparison");
	auto fiSongProperty = query.record().indexOf("SongProperty");
	auto fiValue        = query.record().indexOf("Value");

	/*
	Load each filter.
	Each filter is loaded into a map of FilterRowID -> (ParentRowID, Filter); then all filters in the map
	are walked to assign children to the proper parent filter. The filter without any parent is set as
	the root filter for the item.
	*/
	std::map<qlonglong, std::pair<qlonglong, Template::FilterPtr>> filters;
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
		Template::FilterPtr filter;
		Template::Filter::Kind kind;
		auto parentRowId = query.value(fiParentRowId).toLongLong();
		auto rowId = query.value(fiRowId).toLongLong();
		auto intKind = query.value(fiKind).toInt();
		try
		{
			kind = Template::Filter::intToKind(intKind);
		}
		catch (const std::runtime_error & exc)
		{
			qWarning() << __FUNCTION__ << ": Failed to load Kind for filter for "
				<< " item " << a_Item.displayName()
				<< ", RowID = " << rowId
				<< ", err: " << exc.what();
			continue;
		}
		switch (kind)
		{
			case Template::Filter::fkComparison:
			{
				auto intComparison = query.value(fiComparison).toInt();
				int intProp = query.value(fiSongProperty).toInt();
				Template::Filter::Comparison comparison;
				Template::Filter::SongProperty prop;
				try
				{
					comparison = Template::Filter::intToComparison(intComparison);
				}
				catch (const std::runtime_error & exc)
				{
					qWarning() << __FUNCTION__ << ": Failed to load Comparison for filter for "
						<< " item " << a_Item.displayName()
						<< ", RowID = " << rowId
						<< ", err: " << exc.what();
					continue;
				}
				try
				{
					prop = Template::Filter::intToSongProperty(intProp);
				}
				catch (const std::runtime_error & exc)
				{
					qWarning() << __FUNCTION__ << ": Failed to load SongProperty for filter for "
						<< " item " << a_Item.displayName()
						<< ", RowID = " << rowId
						<< ", err: " << exc.what();
					continue;
				}
				filter.reset(new Template::Filter(prop, comparison, query.value(fiValue)));
				break;
			}  // case Tempalte::Filter::fkComparison

			case Template::Filter::fkAnd:
			case Template::Filter::fkOr:
			{
				filter.reset(new Template::Filter(kind, {}));
				break;
			}
		}
		filters[rowId] = std::make_pair(parentRowId, filter);
	}

	// Assign filters to their parents:
	bool hasSetRoot = false;
	for (auto & f: filters)
	{
		auto parentId = f.second.first;
		auto & filter = f.second.second;
		if (parentId < 0)
		{
			if (hasSetRoot)
			{
				qWarning() << __FUNCTION__ << ": Multiple root filters detected.";
				continue;
			}
			a_Item.setFilter(filter);
			hasSetRoot = true;
		}
		else
		{
			auto parentItr = filters.find(parentId);
			if (parentItr == filters.end())
			{
				qWarning() << __FUNCTION__ << ": Invalid filter parent: " << parentId;
				continue;
			}
			auto & parent = parentItr->second.second;
			if (!parent->canHaveChildren())
			{
				qWarning() << __FUNCTION__ << ": Bad filter parent, cannot have child filters: " << parentId;
				continue;
			}
			parent->addChild(filter);
		}
	}
}





void Database::saveTemplateItem(const Template & a_Template, int a_Index, const Template::Item & a_Item)
{
	// Check if item exists:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT RowID FROM TemplateItems WHERE TemplateID = ? AND IndexInParent = ?"))
	{
		qWarning() << __FUNCTION__ << ": Cannot query template items count from the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", prep error: " << query.lastError();
		return;
	}
	query.addBindValue(a_Template.dbRowId());
	query.addBindValue(a_Index);
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot query template items count from the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		return;
	}

	// Insert / update:
	auto doesExist = query.next();
	qlonglong rowId = -1;
	if (doesExist)
	{
		// Exists, update:
		rowId = query.value(0).toLongLong();
		if (!query.prepare(
			"UPDATE TemplateItems SET DisplayName = ?, Notes = ?, IsFavorite = ?"
			"WHERE TemplateID = ? AND IndexInParent = ?"
		))
		{
			qWarning() << __FUNCTION__ << ": Cannot update template item in the DB, template "
				<< a_Template.displayName()
				<< ", item " << a_Item.displayName()
				<< ", prep error: " << query.lastError();
			return;
		}
		query.addBindValue(a_Item.displayName());
		query.addBindValue(a_Item.notes());
		query.addBindValue(a_Item.isFavorite());
		query.addBindValue(a_Template.dbRowId());
		query.addBindValue(a_Index);
	}
	else
	{
		// Doesn't exist, create:
		if (!query.prepare(
			"INSERT INTO TemplateItems (DisplayName, Notes, IsFavorite, TemplateID, IndexInParent)"
			"VALUES (?, ?, ?, ?, ?)"
		))
		{
			qWarning() << __FUNCTION__ << ": Cannot insert template item into the DB, template "
				<< a_Template.displayName()
				<< ", item " << a_Item.displayName()
				<< ", exec error: " << query.lastError();
			return;
		}
		query.addBindValue(a_Item.displayName());
		query.addBindValue(a_Item.notes());
		query.addBindValue(a_Item.isFavorite());
		query.addBindValue(a_Template.dbRowId());
		query.addBindValue(a_Index);
	}
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot update template item in the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		return;
	}
	if (!doesExist)
	{
		rowId = query.lastInsertId().toLongLong();
	}
	assert(rowId != -1);

	// Remove all filters assigned to this item:
	if (!query.prepare("DELETE FROM TemplateFilters WHERE ItemID = ?"))
	{
		qWarning() << __FUNCTION__ << ": Cannot remove old template item filters from the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", prep error: " << query.lastError();
		return;
	}
	query.addBindValue(rowId);
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot remove old template item filters from the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		return;
	}

	// Save the filters recursively:
	saveTemplateFilters(rowId, *(a_Item.filter()), -1);
}





void Database::saveTemplateFilters(
	qlonglong a_TemplateItemRowId,
	const Template::Filter & a_Filter,
	qlonglong a_ParentFilterRowId
)
{
	QSqlQuery query(m_Database);
	if (a_Filter.canHaveChildren())
	{
		if (!query.prepare(
			"INSERT INTO TemplateFilters (ItemID, ParentID, Kind) "
			"VALUES (?, ?, ?)"
		))
		{
			qWarning() << __FUNCTION__ << ": Cannot insert template item filter to the DB"
				", prep error:" << query.lastError();
			return;
		}
		query.addBindValue(a_TemplateItemRowId);
		query.addBindValue(a_ParentFilterRowId);
		query.addBindValue(static_cast<int>(a_Filter.kind()));
	}
	else
	{
		if (!query.prepare(
			"INSERT INTO TemplateFilters (ItemID, ParentID, Kind, SongProperty, Comparison, Value) "
			"VALUES (?, ?, ?, ?, ?, ?)"
		))
		{
			qWarning() << __FUNCTION__ << ": Cannot insert template item filter to the DB"
				", prep error:" << query.lastError();
			return;
		}
		query.addBindValue(a_TemplateItemRowId);
		query.addBindValue(a_ParentFilterRowId);
		query.addBindValue(static_cast<int>(Template::Filter::fkComparison));
		query.addBindValue(static_cast<int>(a_Filter.songProperty()));
		query.addBindValue(static_cast<int>(a_Filter.comparison()));
		query.addBindValue(a_Filter.value());
	}
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot insert template item filter to the DB"
			", exec error:" << query.lastError();
		return;
	}
	auto rowId = query.lastInsertId().toLongLong();

	// Recurse over the children:
	if (a_Filter.canHaveChildren())
	{
		for (const auto & ch: a_Filter.children())
		{
			saveTemplateFilters(a_TemplateItemRowId, *ch, rowId);
		}
	}
}





void Database::songScanned(Song * a_Song)
{
	a_Song->setLastMetadataUpdated(QDateTime::currentDateTimeUtc());
	saveSong(*a_Song);
}





void Database::songPlaybackStarted(Song * a_Song)
{
	auto now = QDateTime::currentDateTimeUtc();
	a_Song->setLastPlayed(now);
	saveSong(*a_Song);

	// Add a history playlist record:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO PlaybackHistory (SongID, Timestamp) VALUES (?, ?)"))
	{
		qWarning() << __FUNCTION__ << ": Cannot prepare statement: " << query.lastError();
		return;
	}
	query.bindValue(0, a_Song->dbRowId());
	query.bindValue(1, now);
	if (!query.exec())
	{
		qWarning() << __FUNCTION__ << ": Cannot exec statement: " << query.lastError();
		return;
	}
}





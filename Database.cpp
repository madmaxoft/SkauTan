#include "Database.h"
#include <assert.h>
#include <set>
#include <array>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>
#include <QThread>
#include <QApplication>
#include "Stopwatch.h"





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





/** Constructs a Song::Tag instance from the specified SQL record,
reading the four values from the columns at the specified indices.
Index 0: author column
Index 1: title column
Index 2: genre column
Index 3: MPM column
If any index is negative, the value in the resulting tag is an uninitialized variant. */
static Song::Tag tagFromFields(const QSqlRecord & a_Record, const std::array<int, 4> & a_Indices)
{
	Song::Tag res;
	if (a_Indices[0] >= 0)
	{
		res.m_Author = fieldValue(a_Record.field(a_Indices[0]));
	}
	if (a_Indices[1] >= 0)
	{
		res.m_Title = fieldValue(a_Record.field(a_Indices[1]));
	}
	if (a_Indices[2] >= 0)
	{
		res.m_Genre = fieldValue(a_Record.field(a_Indices[2]));
	}
	if (a_Indices[3] >= 0)
	{
		res.m_MeasuresPerMinute = fieldValue(a_Record.field(a_Indices[3]));
	}
	return res;
}





////////////////////////////////////////////////////////////////////////////////
// SongDatabase:

Database::Database()
{
}





void Database::open(const QString & a_DBFileName)
{
	assert(!m_Database.isOpen());  // Opening another DB is not allowed

	m_Database = QSqlDatabase::addDatabase("QSQLITE");
	m_Database.setDatabaseName(a_DBFileName);
	if (!m_Database.open())
	{
		qWarning() << ": Cannot open DB " << a_DBFileName << ": " << m_Database.lastError();
		return;
	}
	fixupTables();
	loadSongs();
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
			qDebug() << ": Skipping duplicate " << a_FileName;
			return;
		}
	}

	// Insert into DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO SongHashes (FileName, FileSize) VALUES(?, ?)"))
	{
		qWarning() << ": Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_FileName);
	query.bindValue(1, a_FileSize);
	if (!query.exec())
	{
		qWarning() << ": Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Insert into memory:
	auto song = std::make_shared<Song>(a_FileName, a_FileSize);
	m_Songs.push_back(song);
	emit songFileAdded(song);
}





void Database::delSong(const Song & a_Song)
{
	size_t idx = 0;
	for (auto itr = m_Songs.cbegin(), end = m_Songs.cend(); itr != end; ++itr, ++idx)
	{
		if (itr->get() != &a_Song)
		{
			continue;
		}
		auto song = *itr;
		emit songRemoving(song, idx);
		m_Songs.erase(itr);

		// Remove from the DB:
		QSqlQuery query(m_Database);
		if (!query.prepare("DELETE FROM SongHashes WHERE FileName = ?"))
		{
			qWarning() << ": Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_Song.fileName());
		if (!query.exec())
		{
			qWarning() << ": Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}

		emit songRemoved(song, idx);
		return;
	}
	assert(!"Song not in the DB");
}





TemplatePtr Database::createTemplate()
{
	auto res = std::make_shared<Template>(-1);
	addTemplate(res);
	return res;
}





void Database::addTemplate(TemplatePtr a_Template)
{
	assert(a_Template->dbRowId() == -1);  // No RowID assigned yet

	// Insert into DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO Templates (DisplayName, Notes) VALUES(?, ?)"))
	{
		qWarning() << ": Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Template->displayName());
	query.bindValue(1, a_Template->notes());
	if (!query.exec())
	{
		qWarning() << ": Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Insert into memory:
	a_Template->setDbRowId(query.lastInsertId().toLongLong());
	m_Templates.push_back(a_Template);
}





void Database::delTemplate(const Template * a_Template)
{
	for (auto itr = m_Templates.cbegin(), end = m_Templates.cend(); itr != end; ++itr)
	{
		if (itr->get() == a_Template)
		{
			// Erase from the DB:
			QSqlQuery query(m_Database);
			if (!query.prepare("DELETE FROM Templates WHERE RowID = ?"))
			{
				qWarning() << ": Cannot prep DELETE(Templates) statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			query.addBindValue(a_Template->dbRowId());
			if (!query.exec())
			{
				qWarning() << ": Cannot exec DELETE(Templates) statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			if (!query.prepare("DELETE FROM TemplateItems WHERE TemplateID = ?"))
			{
				qWarning() << ": Cannot prep DELETE(TemplateItems) statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			query.addBindValue(a_Template->dbRowId());
			if (!query.exec())
			{
				qWarning() << ": Cannot exec DELETE(TemplateItems) statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			if (!query.prepare("DELETE FROM TemplateFilters WHERE TemplateID = ?"))
			{
				qWarning() << ": Cannot prep DELETE(TemplateFilters) statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			query.addBindValue(a_Template->dbRowId());
			if (!query.exec())
			{
				qWarning() << ": Cannot exec DELETE(TemplateFilters) statement: " << query.lastError();
				assert(!"DB error");
				return;
			}

			// Remove from the internal list:
			m_Templates.erase(itr);
			return;
		}
	}
	qWarning() << ": Attempting to remove a template not in the list: " << a_Template->displayName();
	assert(!"Template not in list");
}





void Database::saveTemplate(const Template & a_Template)
{
	// Save the template direct values:
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Templates SET "
		"DisplayName = ?, Notes = ?, BgColor = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << ": Cannot prepare UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Template.displayName());
	query.addBindValue(a_Template.notes());
	query.addBindValue(a_Template.bgColor().name());
	query.addBindValue(a_Template.dbRowId());
	if (!query.exec())
	{
		qWarning() << ": Cannot exec UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Remove all template's items from the DB:
	if (query.prepare("DELETE FROM TemplateItems WHERE TemplateID = ?"))
	{
		query.addBindValue(a_Template.dbRowId());
		if (!query.exec())
		{
			qWarning() << ": Cannot exec DELETE statement: " << query.lastError();
			assert(!"DB error");
		}
	}
	else
	{
		qWarning() << ": Cannot prepare DELETE statement: " << query.lastError();
		assert(!"DB error");
	}

	// Re-add all template's items into the DB:
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





QSqlQuery Database::playbackHistorySqlQuery()
{
	QSqlQuery res(m_Database);
	if (!res.prepare("SELECT "
		"PlaybackHistory.Timestamp, "
		"SongMetadata.ID3Genre, "
		"SongMetadata.ID3Author, "
		"SongMetadata.ID3Title, "
		"SongHashes.FileName "
		"FROM PlaybackHistory "
		"LEFT JOIN SongHashes ON PlaybackHistory.SongHash == SongHashes.Hash "
		"LEFT JOIN SongMetadata ON PlaybackHistory.SongHash == SongMetadata.Hash"
	))
	{
		qWarning() << "Cannot prep playback history query: " << res.lastError();
		return QSqlQuery();
	}

	if (!res.exec())
	{
		qWarning() << "Cannot exec playback history query: " << res.lastError();
		return QSqlQuery();
	}

	return res;
}





void Database::fixupTables()
{
	static const std::vector<std::pair<QString, QString>> cdSongHashes =
	{
		{"FileName",                  "TEXT"},
		{"FileSize",                  "NUMBER"},
		{"Hash",                      "BLOB"},
	};

	static const std::vector<std::pair<QString, QString>> cdSongMetadata =
	{
		{"Hash",                      "BLOB PRIMARY KEY"},
		{"Length",                    "NUMERIC"},
		{"ManualAuthor",              "TEXT"},
		{"ManualTitle",               "TEXT"},
		{"ManualGenre",               "TEXT"},
		{"ManualMeasuresPerMinute",   "NUMERIC"},
		{"FileNameAuthor",            "TEXT"},
		{"FileNameTitle",             "TEXT"},
		{"FileNameGenre",             "TEXT"},
		{"FileNameMeasuresPerMinute", "NUMERIC"},
		{"ID3Author",                 "TEXT"},
		{"ID3Title",                  "TEXT"},
		{"ID3Genre",                  "TEXT"},
		{"ID3MeasuresPerMinute",      "NUMERIC"},
		{"LastPlayed",                "DATETIME"},
		{"Rating",                    "NUMERIC"},
		{"LastMetadataUpdated",       "DATETIME"},
	};

	static const std::vector<std::pair<QString, QString>> cdPlaybackHistory =
	{
		{"SongHash",  "BLOB"},
		{"Timestamp", "DATETIME"},
	};

	static const std::vector<std::pair<QString, QString>> cdTemplates =
	{
		{"RowID",       "INTEGER PRIMARY KEY"},
		{"DisplayName", "TEXT"},
		{"Notes",       "TEXT"},
		{"BgColor",     "TEXT"},  // "#rrggbb"
	};

	static const std::vector<std::pair<QString, QString>> cdTemplateItems =
	{
		{"RowID",         "INTEGER PRIMARY KEY"},
		{"TemplateID",    "INTEGER"},
		{"IndexInParent", "INTEGER"},  // The index of the Item within its Template
		{"DisplayName",   "TEXT"},
		{"Notes",         "TEXT"},
		{"IsFavorite",    "INTEGER"},
		{"BgColor",       "TEXT"},  // "#rrggbb"
	};

	static const std::vector<std::pair<QString, QString>> cdTemplateFilters =
	{
		{"RowID",        "INTEGER PRIMARY KEY"},
		{"ItemID",       "INTEGER"},  // RowID of the TemplateItem that owns this filter
		{"TemplateID",   "INTEGER"},  // RowID of the Template that owns this filter's item
		{"ParentID",     "INTEGER"},  // RowID of this filter's parent, or -1 if root
		{"Kind",         "INTEGER"},  // Numeric representation of the Template::Filter::Kind enum
		{"SongProperty", "INTEGER"},  // Numeric representation of the Template::Filter::SongProperty enum
		{"Comparison",   "INTEGER"},  // Numeric representation of the Template::Filter::Comparison enum
		{"Value",        "TEXT"},     // The value against which to compare
	};

	fixupTable("SongHashes",      cdSongHashes);
	fixupTable("SongMetadata",    cdSongMetadata);
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
		qWarning()
			<< ": Failed to fixup table " << a_TableName
			<< ", query CreateTable: " << createQuery
			<< ": " << query.lastError();
	}

	// Check for existing columns:
	std::set<QString> columns;
	if (!query.exec(QString("PRAGMA table_info(%1)").arg(a_TableName)))
	{
		qWarning()
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
		qDebug() << ": Adding column " << a_TableName << "." << col.first << " " << col.second;
		if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
			.arg(a_TableName, col.first, col.second))
		)
		{
			qWarning()
				<< ": Failed to add column " << col.first
				<< " to table " << a_TableName
				<< ": " << query.lastError();
		}
	}
}





void Database::loadSongs()
{
	STOPWATCH("Loading songs from the DB");

	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT * FROM SongHashes LEFT JOIN SongMetadata ON SongHashes.Hash == SongMetadata.Hash"))
	{
		qWarning() << ": Cannot query songs from the DB: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiFileName            = query.record().indexOf("FileName");
	auto fiFileSize            = query.record().indexOf("FileSize");
	auto fiHash                = query.record().indexOf("Hash");
	auto fiLength              = query.record().indexOf("Length");
	auto fiLastPlayed          = query.record().indexOf("LastPlayed");
	auto fiRating              = query.record().indexOf("Rating");
	auto fiLastMetadataUpdated = query.record().indexOf("LastMetadataUpdated");
	std::array<int, 4> fisManual
	{{
		query.record().indexOf("ManualAuthor"),
		query.record().indexOf("ManualTitle"),
		query.record().indexOf("ManualGenre"),
		query.record().indexOf("ManualMeasuresPerMinute"),
	}};
	std::array<int, 4> fisFileName
	{{
		query.record().indexOf("FileNameAuthor"),
		query.record().indexOf("FileNameTitle"),
		query.record().indexOf("FileNameGenre"),
		query.record().indexOf("FileNameMeasuresPerMinute"),
	}};
	std::array<int, 4> fisId3 =
	{{
		query.record().indexOf("Id3Author"),
		query.record().indexOf("Id3Title"),
		query.record().indexOf("Id3Genre"),
		query.record().indexOf("Id3MeasuresPerMinute"),
	}};

	// Load each song:
	if (!query.isActive())
	{
		qWarning() << ": Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << ": Not a select";
		assert(!"DB error");
		return;
	}
	while (query.next())
	{
		const auto & rec = query.record();
		auto song = std::make_shared<Song>(
			query.value(fiFileName).toString(),
			query.value(fiFileSize).toULongLong(),
			fieldValue(rec.field(fiHash)),
			fieldValue(rec.field(fiLength)),
			tagFromFields(rec, fisManual),
			tagFromFields(rec, fisFileName),
			tagFromFields(rec, fisId3),
			fieldValue(rec.field(fiLastPlayed)),
			fieldValue(rec.field(fiRating)),
			fieldValue(rec.field(fiLastMetadataUpdated))
		);
		m_Songs.push_back(song);
		if (!song->hash().isValid())
		{
			emit needSongHash(song);
		}
		else
		{
			if (song->needsMetadataRescan())
			{
				emit needSongMetadata(song);
			}
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
		qWarning() << ": Cannot query templates from the DB: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiDisplayName = query.record().indexOf("DisplayName");
	auto fiNotes       = query.record().indexOf("Notes");
	auto fiBgColor     = query.record().indexOf("BgColor");
	auto fiRowId       = query.record().indexOf("RowID");

	// Load each template:
	if (!query.isActive())
	{
		qWarning() << ": Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << ": Not a select";
		assert(!"DB error");
		return;
	}
	while (query.next())
	{
		auto tmpl = std::make_shared<Template>(
			query.value(fiRowId).toLongLong(),
			query.value(fiDisplayName).toString(),
			query.value(fiNotes).toString()
		);
		QColor c(query.value(fiBgColor).toString());
		if (c.isValid())
		{
			tmpl->setBgColor(c);
		}
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
		qWarning() << ": Cannot query template items from the DB, template "
			<< a_Template->displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Template->dbRowId());
	if (!query.exec())
	{
		qWarning() << ": Cannot query template items from the DB, template "
			<< a_Template->displayName()
			<< ", exec error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiRowId       = query.record().indexOf("RowID");
	auto fiDisplayName = query.record().indexOf("DisplayName");
	auto fiNotes       = query.record().indexOf("Notes");
	auto fiIsFavorite  = query.record().indexOf("IsFavorite");
	auto fiBgColor     = query.record().indexOf("BgColor");

	// Load each template:
	if (!query.isActive())
	{
		qWarning() << ": Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << ": Not a select";
		assert(!"DB error");
		return;
	}
	while (query.next())
	{
		auto item = a_Template->addItem(
			query.value(fiDisplayName).toString(),
			query.value(fiNotes).toString(),
			query.value(fiIsFavorite).toBool()
		);
		QColor c(query.value(fiBgColor).toString());
		if (c.isValid())
		{
			item->setBgColor(c);
		}
		auto rowId = query.value(fiRowId).toLongLong();
		loadTemplateFilters(a_Template, rowId, *item);
		if (item->filter() == nullptr)
		{
			qWarning() << ": Empty filter detected in item "
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
		qWarning() << ": Cannot query template filters from the DB, "
			<< "template " << a_Template->displayName()
			<< ", item " << a_Item.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_ItemRowId);
	if (!query.exec())
	{
		qWarning() << ": Cannot query template filters from the DB, "
			<< "template " << a_Template->displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		assert(!"DB error");
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
		qWarning() << ": Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << ": Not a select";
		assert(!"DB error");
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
			qWarning() << ": Failed to load Kind for filter for "
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
					qWarning() << ": Failed to load Comparison for filter for "
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
					qWarning() << ": Failed to load SongProperty for filter for "
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
				qWarning() << ": Multiple root filters detected.";
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
				qWarning() << ": Invalid filter parent: " << parentId;
				continue;
			}
			auto & parent = parentItr->second.second;
			if (!parent->canHaveChildren())
			{
				qWarning() << ": Bad filter parent, cannot have child filters: " << parentId;
				continue;
			}
			parent->addChild(filter);
		}
	}
}





void Database::saveTemplateItem(const Template & a_Template, int a_Index, const Template::Item & a_Item)
{
	// Insert the item into the DB:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	qlonglong rowId = -1;
	if (!query.prepare(
		"INSERT INTO TemplateItems (DisplayName, Notes, IsFavorite, TemplateID, IndexInParent, BgColor)"
		"VALUES (?, ?, ?, ?, ?, ?)"
	))
	{
		qWarning() << ": Cannot insert template item into the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Item.displayName());
	query.addBindValue(a_Item.notes());
	query.addBindValue(a_Item.isFavorite());
	query.addBindValue(a_Template.dbRowId());
	query.addBindValue(a_Index);
	query.addBindValue(a_Item.bgColor().name());
	if (!query.exec())
	{
		qWarning() << ": Cannot insert template item in the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	rowId = query.lastInsertId().toLongLong();
	assert(rowId != -1);

	// Remove all filters assigned to this item:
	if (!query.prepare("DELETE FROM TemplateFilters WHERE ItemID = ?"))
	{
		qWarning() << ": Cannot remove old template item filters from the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(rowId);
	if (!query.exec())
	{
		qWarning() << ": Cannot remove old template item filters from the DB, template "
			<< a_Template.displayName()
			<< ", item " << a_Item.displayName()
			<< ", exec error: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Save the filters recursively:
	saveTemplateFilters(a_Template.dbRowId(), rowId, *(a_Item.filter()), -1);
}





void Database::saveTemplateFilters(
	qlonglong a_TemplateRowId,
	qlonglong a_TemplateItemRowId,
	const Template::Filter & a_Filter,
	qlonglong a_ParentFilterRowId
)
{
	QSqlQuery query(m_Database);
	if (a_Filter.canHaveChildren())
	{
		if (!query.prepare(
			"INSERT INTO TemplateFilters (TemplateID, ItemID, ParentID, Kind) "
			"VALUES (?, ?, ?, ?)"
		))
		{
			qWarning() << ": Cannot insert template item filter to the DB"
				", prep error:" << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_TemplateRowId);
		query.addBindValue(a_TemplateItemRowId);
		query.addBindValue(a_ParentFilterRowId);
		query.addBindValue(static_cast<int>(a_Filter.kind()));
	}
	else
	{
		if (!query.prepare(
			"INSERT INTO TemplateFilters (TemplateID, ItemID, ParentID, Kind, SongProperty, Comparison, Value) "
			"VALUES (?, ?, ?, ?, ?, ?, ?)"
		))
		{
			qWarning() << ": Cannot insert template item filter to the DB"
				", prep error:" << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_TemplateRowId);
		query.addBindValue(a_TemplateItemRowId);
		query.addBindValue(a_ParentFilterRowId);
		query.addBindValue(static_cast<int>(Template::Filter::fkComparison));
		query.addBindValue(static_cast<int>(a_Filter.songProperty()));
		query.addBindValue(static_cast<int>(a_Filter.comparison()));
		query.addBindValue(a_Filter.value());
	}
	if (!query.exec())
	{
		qWarning() << ": Cannot insert template item filter to the DB"
			", exec error:" << query.lastError();
		assert(!"DB error");
		return;
	}
	auto rowId = query.lastInsertId().toLongLong();

	// Recurse over the children:
	if (a_Filter.canHaveChildren())
	{
		for (const auto & ch: a_Filter.children())
		{
			saveTemplateFilters(a_TemplateRowId, a_TemplateItemRowId, *ch, rowId);
		}
	}
}





void Database::saveSongHash(SongPtr a_Song)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE SongHashes SET "
		"FileSize = ?, Hash = ? "
		"WHERE FileName = ?")
	)
	{
		qWarning() << ": Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Song->fileSize());
	query.addBindValue(a_Song->hash());
	query.addBindValue(a_Song->fileName());
	if (!query.exec())
	{
		qWarning() << ": Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::saveSongMetadata(SongPtr a_Song)
{
	assert(a_Song->hash().isValid());

	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE SongMetadata SET "
		"Length = ?, "
		"ManualAuthor = ?, ManualTitle = ?, ManualGenre = ?, ManualMeasuresPerMinute = ?, "
		"FileNameAuthor = ?, FileNameTitle = ?, FileNameGenre = ?, FileNameMeasuresPerMinute = ?, "
		"ID3Author = ?, ID3Title = ?, ID3Genre = ?, ID3MeasuresPerMinute = ?, "
		"LastPlayed = ?, Rating = ?, LastMetadataUpdated = ? "
		"WHERE Hash = ?")
	)
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Song->length());
	query.addBindValue(a_Song->tagManual().m_Author);
	query.addBindValue(a_Song->tagManual().m_Title);
	query.addBindValue(a_Song->tagManual().m_Genre);
	query.addBindValue(a_Song->tagManual().m_MeasuresPerMinute);
	query.addBindValue(a_Song->tagFileName().m_Author);
	query.addBindValue(a_Song->tagFileName().m_Title);
	query.addBindValue(a_Song->tagFileName().m_Genre);
	query.addBindValue(a_Song->tagFileName().m_MeasuresPerMinute);
	query.addBindValue(a_Song->tagId3().m_Author);
	query.addBindValue(a_Song->tagId3().m_Title);
	query.addBindValue(a_Song->tagId3().m_Genre);
	query.addBindValue(a_Song->tagId3().m_MeasuresPerMinute);
	query.addBindValue(a_Song->lastPlayed());
	query.addBindValue(a_Song->rating());
	query.addBindValue(a_Song->lastMetadataUpdated());
	query.addBindValue(a_Song->hash());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	if (query.numRowsAffected() != 1)
	{
		qWarning() << "Statement didn't modify one row: " << query.numRowsAffected()
			<< ", Song " << a_Song->fileName() << ", hash " << a_Song->hash();
		assert(!"DB error");
		return;
	}
}





void Database::songPlaybackStarted(SongPtr a_Song)
{
	auto now = QDateTime::currentDateTimeUtc();
	a_Song->setLastPlayed(now);
	QMetaObject::invokeMethod(this, "saveSong", Q_ARG(SongPtr, a_Song));
	QMetaObject::invokeMethod(this, "addPlaybackHistory", Q_ARG(SongPtr, a_Song), Q_ARG(QDateTime, now));
}





void Database::songHashCalculated(SongPtr a_Song)
{
	// Insert the metadata record, now that we know the song hash:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT OR IGNORE INTO SongMetadata (Hash) VALUES (?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Song->hash());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// If the hash was already in the DB, copy the metadata from the duplicate song:
	auto isDuplicate = (query.numRowsAffected() == 0);
	if (isDuplicate)
	{
		for (const auto & s: m_Songs)
		{
			if ((s != a_Song) && (s->hash() == a_Song->hash()))
			{
				a_Song->copyMetadataFrom(*s);
				break;
			}
		}
	}

	// Save into the DB:
	saveSong(a_Song);

	// Rescan metadata, if needed:
	if (!isDuplicate)
	{
		if (a_Song->needsMetadataRescan())
		{
			emit needSongMetadata(a_Song);
		}
	}
}





void Database::saveSong(SongPtr a_Song)
{
	assert(a_Song != nullptr);
	assert(QThread::currentThread() == QApplication::instance()->thread());

	qDebug() << "Saving song " << a_Song->fileName();

	saveSongHash(a_Song);
	if (a_Song->hash().isValid())
	{
		saveSongMetadata(a_Song);
	}
	emit songSaved(a_Song);
}





void Database::songScanned(SongPtr a_Song)
{
	a_Song->setLastMetadataUpdated(QDateTime::currentDateTimeUtc());
	saveSong(a_Song);
}





void Database::addPlaybackHistory(SongPtr a_Song, const QDateTime & a_Timestamp)
{
	// Add a history playlist record:
	if (a_Song->hash().isValid())
	{
		QSqlQuery query(m_Database);
		if (!query.prepare("INSERT INTO PlaybackHistory (SongHash, Timestamp) VALUES (?, ?)"))
		{
			qWarning() << ": Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.bindValue(0, a_Song->hash());
		query.bindValue(1, a_Timestamp);
		if (!query.exec())
		{
			qWarning() << ": Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
	}
}

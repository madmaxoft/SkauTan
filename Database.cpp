#include "Database.h"
#include <assert.h>
#include <set>
#include <array>
#include <fstream>
#include <random>
#include <atomic>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>
#include <QThread>
#include <QApplication>
#include <QFile>
#include "Stopwatch.h"
#include "DatabaseUpgrade.h"
#include "Playlist.h"
#include "PlaylistItemSong.h"
#include "Filter.h"
#include "InstallConfiguration.h"
#include "DatabaseBackup.h"
#include "Exception.h"





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





/** Constructs a DatedOptional from the two DB fields given the record to read and the field indices. */
template <typename T> static DatedOptional<T> datedOptionalFromFields(
	const QSqlRecord & a_Record,
	int a_IdxValue,
	int a_IdxLastModification
)
{
	return DatedOptional<T>(
		fieldValue(a_Record.field(a_IdxValue)),
		a_Record.value(a_IdxLastModification).toDateTime()
	);
}





/** Constructs a Song::Tag instance from the specified SQL record,
reading the four values from the columns at the specified indices.
Index 0: author column
Index 1: title column
Index 2: genre column
Index 3: MPM column
If any index is negative, the value in the resulting tag is an uninitialized variant. */
static Song::Tag tagFromFields(
	const QSqlRecord & a_Record,
	const std::array<int, 4> & a_Indices,
	const std::array<int, 4> & a_IndicesLM
)
{
	Song::Tag res;
	if (a_Indices[0] >= 0)
	{
		res.m_Author = datedOptionalFromFields<QString>(a_Record, a_Indices[0], a_IndicesLM[0]);
	}
	if (a_Indices[1] >= 0)
	{
		res.m_Title = datedOptionalFromFields<QString>(a_Record, a_Indices[1], a_IndicesLM[1]);
	}
	if (a_Indices[2] >= 0)
	{
		res.m_Genre = datedOptionalFromFields<QString>(a_Record, a_Indices[2], a_IndicesLM[2]);
	}
	if (a_Indices[3] >= 0)
	{
		res.m_MeasuresPerMinute = datedOptionalFromFields<double>(a_Record, a_Indices[3], a_IndicesLM[3]);
	}
	return res;
}





/** Applies the specified rating, if present, to the song weight. */
static qint64 applyRating(qint64 a_CurrentWeight, const DatedOptional<double> & a_Rating)
{
	if (a_Rating.isPresent())
	{
		// Even zero-rating songs need *some* chance, so we add 1 to all ratings
		return static_cast<qint64>(a_CurrentWeight * (a_Rating.value() + 1) / 5);
	}
	else
	{
		// Default to 2.5-star rating:
		return static_cast<qint64>(a_CurrentWeight * 3.5 / 5);
	}
}





/** Implements a RAII-like behavior for transactions.
Unless explicitly committed, a transaction is rolled back upon destruction of this object. */
class SqlTransaction
{
public:

	/** Starts a transaction.
	If a transaction cannot be started, logs and throws a RuntimeError. */
	SqlTransaction(QSqlDatabase & a_DB):
		m_DB(a_DB),
		m_IsActive(a_DB.transaction())
	{
		if (!m_IsActive)
		{
			throw RuntimeError("DB doesn't support transactions: %1", m_DB.lastError());
		}
	}


	/** Rolls back the transaction, unless it has been committed.
	If the transaction fails to roll back, an error is logged (but nothing thrown). */
	~SqlTransaction()
	{
		if (m_IsActive)
		{
			if (!m_DB.rollback())
			{
				qWarning() << "DB transaction rollback failed: " << m_DB.lastError();
				return;
			}
		}
	}


	/** Commits the transaction.
	If a transaction wasn't started, or is already committed, logs and throws a LogicError.
	If the transaction fails to commit, throws a RuntimeError. */
	void commit()
	{
		if (!m_IsActive)
		{
			throw RuntimeError("DB transaction not started");
		}
		if (!m_DB.commit())
		{
			throw LogicError("DB transaction commit failed: %1", m_DB.lastError());
		}
		m_IsActive = false;
	}


protected:

	QSqlDatabase & m_DB;
	bool m_IsActive;
};





////////////////////////////////////////////////////////////////////////////////
// Database:

Database::Database(ComponentCollection & a_Components):
	m_Components(a_Components)
{
}





void Database::open(const QString & a_DBFileName)
{
	assert(!m_Database.isOpen());  // Opening another DB is not allowed

	static std::atomic<int> counter(0);
	auto connName = QString::fromUtf8("DB%1").arg(counter.fetch_add(1));
	m_Database = QSqlDatabase::addDatabase("QSQLITE", connName);
	m_Database.setDatabaseName(a_DBFileName);
	if (!m_Database.open())
	{
		throw RuntimeError(tr("Cannot open the DB file: %1"), m_Database.lastError());
	}

	// Check DB version, if upgradeable, make a backup first:
	{
		auto query = std::make_unique<QSqlQuery>("SELECT MAX(Version) AS Version FROM Version", m_Database);
		if (query->first())
		{
			auto version = query->record().value("Version").toULongLong();
			auto maxVersion = DatabaseUpgrade::currentVersion();
			query.reset();
			if (version > maxVersion)
			{
				throw RuntimeError(tr("Cannot open DB, it is from a newer version %1, this program can only handle up to version %2"),
					version, maxVersion
				);
			}
			if (version < maxVersion)
			{
				// Close the DB:
				m_Database.close();
				QSqlDatabase::removeDatabase(connName);

				// Backup:
				auto backupFolder = m_Components.get<InstallConfiguration>()->dbBackupsFolder();
				DatabaseBackup::backupBeforeUpgrade(a_DBFileName, version, backupFolder);

				// Reopen the DB:
				m_Database = QSqlDatabase::addDatabase("QSQLITE", connName);
				m_Database.setDatabaseName(a_DBFileName);
				if (!m_Database.open())
				{
					throw RuntimeError(tr("Cannot open the DB file: %1"), m_Database.lastError());
				}
			}
		}
	}

	// Turn off synchronous queries (they slow up DB inserts by orders of magnitude):
	auto query = m_Database.exec("PRAGMA synchronous = off");
	if (query.lastError().type() != QSqlError::NoError)
	{
		qWarning() << "Turning off synchronous failed: " << query.lastError();
		// Continue, this is not a hard error, just perf may be bad
	}

	// Turn on foreign keys:
	query = m_Database.exec("PRAGMA foreign_keys = on");
	if (query.lastError().type() != QSqlError::NoError)
	{
		qWarning() << "Turning on foreign keys failed: " << query.lastError();
		// Continue, this is not a hard error, just errors in the DB code may not surface
	}

	// Upgrade the DB to the latest version:
	DatabaseUpgrade::upgrade(*this);

	loadSongs();
	loadFilters();
	loadTemplates();
}





void Database::addSongFiles(const QStringList & a_Files)
{
	for (const auto & f: a_Files)
	{
		addSongFile(f);
	}
}





void Database::addSongFile(const QString & a_FileName)
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
	if (!query.prepare("INSERT INTO NewFiles (FileName) VALUES(?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_FileName);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Enqueue it in the Hash calculator:
	// (After the hash is calculated, only then a Song instance is created for the song, in songHashCalculated())
	emit needFileHash(a_FileName);
}





void Database::removeSong(const Song & a_Song, bool a_DeleteDiskFile)
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
		song->sharedData()->delDuplicate(&a_Song);

		// Remove from the DB:
		{
			QSqlQuery query(m_Database);
			if (!query.prepare("DELETE FROM SongFiles WHERE FileName = ?"))
			{
				qWarning() << "Cannot prepare statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			query.addBindValue(a_Song.fileName());
			if (!query.exec())
			{
				qWarning() << "Cannot exec statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
		}

		// Add a log entry:
		{
			QSqlQuery query(m_Database);
			if (!query.prepare(
				"INSERT INTO RemovedSongs "
				"(FileName, Hash, DateRemoved, WasFileDeleted, NumDuplicates) "
				"VALUES (?, ?, ?, ?, ?)"
			))
			{
				qWarning() << "Cannot prepare statement: " << query.lastError();
				assert(!"DB error");
			}
			else
			{
				query.addBindValue(a_Song.fileName());
				query.addBindValue(a_Song.hash());
				query.addBindValue(QDateTime::currentDateTimeUtc());
				query.addBindValue(a_DeleteDiskFile);
				query.addBindValue(static_cast<qulonglong>(a_Song.sharedData()->duplicatesCount()));
				if (!query.exec())
				{
					qWarning() << "Cannot exec statement: " << query.lastError();
					assert(!"DB error");
				}
			}
		}

		// Delete the disk file, if requested:
		if (a_DeleteDiskFile)
		{
			if (!QFile::remove(a_Song.fileName()))
			{
				qWarning() << "Cannot delete disk file " << a_Song.fileName();
			}
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
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Template->displayName());
	query.bindValue(1, a_Template->notes());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Insert into memory:
	a_Template->setDbRowId(query.lastInsertId().toLongLong());
	m_Templates.push_back(a_Template);
}





void Database::swapTemplatesByIdx(size_t a_Idx1, size_t a_Idx2)
{
	assert(a_Idx1 < m_Templates.size());
	assert(a_Idx2 < m_Templates.size());

	if (a_Idx1 == a_Idx2)
	{
		assert(!"Attempting to swap a template with itself.");  // Most likely indicates an error in the caller
		return;
	}

	// Swap in memory:
	std::swap(m_Templates[a_Idx1], m_Templates[a_Idx2]);

	// Swap in the DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Templates SET Position = ? WHERE RowID = ?"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Idx1);
	query.bindValue(1, m_Templates[a_Idx1]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Idx2);
	query.bindValue(1, m_Templates[a_Idx2]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::delTemplate(const Template * a_Template)
{
	for (auto itr = m_Templates.cbegin(), end = m_Templates.cend(); itr != end; ++itr)
	{
		if (itr->get() != a_Template)
		{
			continue;
		}
		// Erase from the DB:
		QSqlQuery query(m_Database);
		if (!query.prepare("DELETE FROM TemplateItems WHERE TemplateID = ?"))
		{
			qWarning() << "Cannot prep DELETE(TemplateItems) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_Template->dbRowId());
		if (!query.exec())
		{
			qWarning() << "Cannot exec DELETE(TemplateItems) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		if (!query.prepare("DELETE FROM Templates WHERE RowID = ?"))
		{
			qWarning() << "Cannot prep DELETE(Templates) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_Template->dbRowId());
		if (!query.exec())
		{
			qWarning() << "Cannot exec DELETE(Templates) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}

		// Remove from the internal list:
		m_Templates.erase(itr);
		return;
	}
	qWarning() << "Attempting to remove a template not in the list: " << a_Template->displayName();
	assert(!"Template not in list");
}





void Database::saveTemplate(const Template & a_Template)
{
	// Find the template position:
	size_t position = m_Templates.size();
	for (size_t i = m_Templates.size(); i > 0;--i)
	{
		if (m_Templates[i - 1].get() == &a_Template)
		{
			position = i - 1;
			break;
		}
	}
	assert(position < m_Templates.size());  // Template not found?

	// Update the template direct values:
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Templates SET "
		"DisplayName = ?, Notes = ?, BgColor = ?, Position = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << "Cannot prepare UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Template.displayName());
	query.addBindValue(a_Template.notes());
	query.addBindValue(a_Template.bgColor().name());
	query.addBindValue(position);
	query.addBindValue(a_Template.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Remove all template's items from the DB:
	if (query.prepare("DELETE FROM TemplateItems WHERE TemplateID = ?"))
	{
		query.addBindValue(a_Template.dbRowId());
		if (!query.exec())
		{
			qWarning() << "Cannot exec DELETE statement: " << query.lastError();
			assert(!"DB error");
			// Continue saving - better to have duplicate entries, than none
		}
	}
	else
	{
		qWarning() << "Cannot prepare DELETE statement: " << query.lastError();
		assert(!"DB error");
		// Continue saving - better to have duplicate entries, than none
	}

	// Re-add all template's items into the DB:
	if (!query.prepare("INSERT INTO TemplateItems (TemplateID, IndexInTemplate, FilterID) VALUES (?, ?, ?)"))
	{
		qWarning() << "Cannot prepare template item insertion: " << query.lastError();
		assert(!"DB error");
		return;
	}
	int idx = 0;
	for (const auto & filter: a_Template.items())
	{
		query.addBindValue(a_Template.dbRowId());
		query.addBindValue(idx);
		query.addBindValue(filter->dbRowId());
		if (!query.exec())
		{
			qWarning() << "Failed to save template " << a_Template.displayName() << " item " << filter->displayName();
			assert(!"DB error");
			// Continue saving
		}
		idx += 1;
	}
}





FilterPtr Database::createFilter()
{
	auto res = std::make_shared<Filter>();
	addFilter(res);
	return res;
}





void Database::addFilter(FilterPtr a_Filter)
{
	assert(a_Filter->dbRowId() == -1);  // No RowID assigned yet

	// Insert into DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO Filters (DisplayName, Notes) VALUES(?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Filter->displayName());
	query.bindValue(1, a_Filter->notes());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Insert into memory:
	a_Filter->setDbRowId(query.lastInsertId().toLongLong());
	m_Filters.push_back(a_Filter);
}





void Database::swapFiltersByIdx(size_t a_Idx1, size_t a_Idx2)
{
	assert(a_Idx1 < m_Filters.size());
	assert(a_Idx2 < m_Filters.size());

	if (a_Idx1 == a_Idx2)
	{
		assert(!"Attempting to swap a filter with itself.");  // Most likely indicates an error in the caller
		return;
	}

	// Swap in memory:
	std::swap(m_Filters[a_Idx1], m_Filters[a_Idx2]);

	// Swap in the DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Filters SET Position = ? WHERE RowID = ?"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Idx1);
	query.bindValue(1, m_Filters[a_Idx1]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Idx2);
	query.bindValue(1, m_Filters[a_Idx2]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::delFilter(size_t a_Index)
{
	if (a_Index >= m_Filters.size())
	{
		return;
	}
	auto filter = m_Filters[a_Index];
	using diffType = std::vector<FilterPtr>::difference_type;
	m_Filters.erase(m_Filters.begin() + static_cast<diffType>(a_Index));

	// Delete the filter from any templates using it:
	SqlTransaction trans(m_Database);
	for (auto & tmpl: m_Templates)
	{
		if (tmpl->removeAllFilterRefs(filter))
		{
			saveTemplate(*tmpl);
		}
	}

	#ifdef _DEBUG
	{
		QSqlQuery query(m_Database);
		if (!query.prepare("SELECT * FROM TemplateItems WHERE FilterID = ?"))
		{
			qWarning() << "Cannot prep SELECT (TemplateItems) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(filter->dbRowId());
		if (!query.exec())
		{
			qWarning() << "Cannot exec SELECT (TemplateItems) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		qDebug() << "TemplateItems with filter: " << query.record();
	}
	#endif

	// Delete the filter nodes from the DB:
	QSqlQuery query(m_Database);
	if (!query.prepare("DELETE FROM FilterNodes WHERE FilterID = ?"))
	{
		qWarning() << "Cannot prep DELETE(FilterNodes) statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(filter->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec DELETE(FilterNodes) statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Delete the filter itself from the DB:
	if (!query.prepare("DELETE FROM Filters WHERE RowID = ?"))
	{
		qWarning() << "Cannot prep DELETE(Filters) statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(filter->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec DELETE(Filters) statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	try
	{
		trans.commit();
	}
	catch (const std::exception & exc)
	{
		qWarning() << "Cannot commit filter deletion: " << exc.what();
		assert(!"DB error");
		return;
	}
}





void Database::saveFilter(const Filter & a_Filter)
{
	// Find the filter's position:
	size_t position = m_Filters.size();
	for (size_t i = m_Filters.size(); i > 0;--i)
	{
		if (m_Filters[i - 1].get() == &a_Filter)
		{
			position = i - 1;
			break;
		}
	}
	assert(position < m_Filters.size());  // Filter not found?

	// Update the filter's direct values:
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE Filters SET "
		"DisplayName = ?, Notes = ?, BgColor = ?, IsFavorite = ?, DurationLimit = ?, Position = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << "Cannot prepare UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Filter.displayName());
	query.addBindValue(a_Filter.notes());
	query.addBindValue(a_Filter.bgColor().name());
	query.addBindValue(a_Filter.isFavorite());
	query.addBindValue(a_Filter.durationLimit().toVariant());
	query.addBindValue(position);
	query.addBindValue(a_Filter.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Remove all filter's nodes from the DB:
	if (query.prepare("DELETE FROM FilterNodes WHERE FilterID = ?"))
	{
		query.addBindValue(a_Filter.dbRowId());
		if (!query.exec())
		{
			qWarning() << "Cannot exec DELETE statement: " << query.lastError();
			assert(!"DB error");
			// Continue saving - better to have duplicate entries, than none
		}
	}
	else
	{
		qWarning() << "Cannot prepare DELETE statement: " << query.lastError();
		assert(!"DB error");
		// Continue saving - better to have duplicate entries, than none
	}

	// Re-add all filter's nodes into the DB:
	saveFilterNodes(a_Filter.dbRowId(), *a_Filter.rootNode(), -1);
}





std::vector<FilterPtr> Database::getFavoriteFilters() const
{
	std::vector<FilterPtr> res;
	for (const auto & filter: m_Filters)
	{
		if (filter->isFavorite())
		{
			res.push_back(filter);
		}
	}
	return res;
}





int Database::numSongsMatchingFilter(const Filter & a_Filter) const
{
	int res = 0;
	for (const auto & s: m_Songs)
	{
		if (a_Filter.rootNode()->isSatisfiedBy(*s))
		{
			res += 1;
		}
	}
	return res;
}





SongPtr Database::pickSongForFilter(const Filter & a_Filter, SongPtr a_Avoid) const
{
	std::vector<std::pair<SongPtr, int>> songs;  // Pairs of SongPtr and their weight
	std::set<Song::SharedDataPtr> sharedDatas;  // SharedDatas of songs added to "songs", to avoid dupes
	int totalWeight = 0;
	for (const auto & song: m_Songs)
	{
		if (song == a_Avoid)
		{
			continue;
		}
		if (sharedDatas.find(song->sharedData()) != sharedDatas.cend())
		{
			// Already present, through another Song, with the same hash
			continue;
		}
		if (a_Filter.rootNode()->isSatisfiedBy(*song))
		{
			auto weight = getSongWeight(*song);
			songs.push_back(std::make_pair(song, weight));
			totalWeight += weight;
			sharedDatas.insert(song->sharedData());
		}
	}

	if (songs.empty())
	{
		if ((a_Avoid != nullptr) && (a_Filter.rootNode()->isSatisfiedBy(*a_Avoid)))
		{
			return a_Avoid;
		}
		qDebug() << ": No song matches item " << a_Filter.displayName();
		return nullptr;
	}

	totalWeight = std::max(totalWeight, 1);
	static std::mt19937_64 mt(0);
	auto rnd = std::uniform_int_distribution<>(0, totalWeight)(mt);
	auto threshold = rnd;
	for (const auto & song: songs)
	{
		threshold -= song.second;
		if (threshold <= 0)
		{
			#if 0
			// DEBUG: Output the choices made into a file:
			static int counter = 0;
			auto fnam = QString("debug_template_%1.log").arg(QString::number(counter++), 3, '0');
			std::ofstream f(fnam.toStdString().c_str());
			f << "Choices for template item " << a_Item->displayName().toStdString() << std::endl;
			f << "------" << std::endl << std::endl;
			f << "Candidates:" << std::endl;
			for (const auto & s: songs)
			{
				f << s.second << '\t';
				f << s.first->lastPlayed().toString().toStdString() << '\t';
				f << s.first->fileName().toStdString() << std::endl;
			}
			f << std::endl;
			f << "totalWeight: " << totalWeight << std::endl;
			f << "threshold: " << rnd << std::endl;
			f << "chosen: " << song.first->fileName().toStdString() << std::endl;
			#endif

			return song.first;
		}
	}
	return songs[0].first;
}





std::vector<std::pair<SongPtr, FilterPtr>> Database::pickSongsForTemplate(const Template & a_Template)
{
	std::vector<std::pair<SongPtr, FilterPtr>> res;
	for (const auto & filter: a_Template.items())
	{
		auto song = pickSongForFilter(*filter);
		if (song != nullptr)
		{
			res.push_back({song, filter});
		}
	}
	return res;
}





std::vector<Database::RemovedSongPtr> Database::removedSongs() const
{
	std::vector<RemovedSongPtr> res;

	QSqlQuery query(m_Database);
	if (!query.prepare("SELECT * FROM RemovedSongs"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return res;
	}
	if (!query.exec())
	{
		qWarning() << "Cannot exec query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return res;
	}
	auto fiFileName      = query.record().indexOf("FileName");
	auto fiDateRemoved   = query.record().indexOf("DateRemoved");
	auto fiHash          = query.record().indexOf("Hash");
	auto fiWasDeleted    = query.record().indexOf("WasFileDeleted");
	auto fiNumDuplicates = query.record().indexOf("NumDuplicates");
	while (query.next())
	{
		auto rsp = std::make_shared<RemovedSong>();
		rsp->m_FileName      = query.value(fiFileName).toString();
		rsp->m_DateRemoved   = query.value(fiDateRemoved).toDateTime();
		rsp->m_Hash          = query.value(fiHash).toByteArray();
		rsp->m_WasDeleted    = query.value(fiWasDeleted).toBool();
		rsp->m_NumDuplicates = query.value(fiNumDuplicates).toInt();
		res.push_back(rsp);
	}
	return res;
}





void Database::clearRemovedSongs()
{
	auto query = m_Database.exec("DELETE FROM RemovedSongs");
	if (query.lastError().type() != QSqlError::NoError)
	{
		qWarning() << "SQL query failed: " << query.lastError();
		return;
	}
}





Song::SharedDataPtr Database::sharedDataFromHash(const QByteArray & a_Hash) const
{
	auto itr = m_SongSharedData.find(a_Hash);
	if (itr == m_SongSharedData.end())
	{
		return nullptr;
	}
	return itr->second;
}





void Database::saveAllSongSharedData()
{
	for (const auto & sd: m_SongSharedData)
	{
		saveSongSharedData(sd.second);
	}
}





std::vector<Database::HistoryItem> Database::playbackHistory() const
{
	std::vector<Database::HistoryItem> res;

	QSqlQuery query(m_Database);
	if (!query.exec("SELECT * FROM PlaybackHistory"))
	{
		qWarning() << "Cannot query playback history: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return res;
	}
	auto fiTimestamp = query.record().indexOf("Timestamp");
	auto fiHash      = query.record().indexOf("SongHash");
	while (query.next())
	{
		assert(query.isValid());
		res.push_back({
			query.value(fiTimestamp).toDateTime(),
			query.value(fiHash).toByteArray()
		});
	}
	return res;
}





void Database::addPlaybackHistory(const std::vector<Database::HistoryItem> & a_History)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO PlaybackHistory (Timestamp, SongHash) VALUES(?, ?)"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return;
	}
	for (const auto & item: a_History)
	{
		query.addBindValue(item.m_Timestamp);
		query.addBindValue(item.m_Hash);
		if (!query.exec())
		{
			qWarning() << "Cannot store playback history item "
				<< item.m_Timestamp << ", "
				<< item.m_Hash << ": "
				<< query.lastError();
			assert(!"DB error");
		}
	}
}





void Database::addSongRemovalHistory(const std::vector<Database::RemovedSongPtr> & a_History)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO RemovedSongs "
		"(Filename, Hash, DateRemoved, WasFileDeleted, NumDuplicates) VALUES "
		"(?, ?, ?, ?, ?)"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return;
	}
	for (const auto & item: a_History)
	{
		query.addBindValue(item->m_FileName);
		query.addBindValue(item->m_Hash);
		query.addBindValue(item->m_DateRemoved);
		query.addBindValue(item->m_WasDeleted);
		query.addBindValue(item->m_NumDuplicates);
		if (!query.exec())
		{
			qWarning() << "Cannot store removal history item "
				<< item->m_FileName << ", "
				<< item->m_Hash << ", "
				<< item->m_DateRemoved << ": "
				<< query.lastError();
			assert(!"DB error");
		}
	}
}





std::vector<Database::Vote> Database::loadVotes(const QString & a_TableName) const
{
	std::vector<Database::Vote> res;

	QSqlQuery query(m_Database);
	if (!query.exec("SELECT * FROM " + a_TableName + " ORDER BY DateAdded"))
	{
		qWarning() << "Cannot query vote history: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return res;
	}
	auto fiDateAdded = query.record().indexOf("DateAdded");
	auto fiSongHash  = query.record().indexOf("SongHash");
	auto fiVoteValue = query.record().indexOf("VoteValue");
	while (query.next())
	{
		assert(query.isValid());
		res.push_back({
			query.value(fiSongHash).toByteArray(),
			query.value(fiDateAdded).toDateTime(),
			query.value(fiVoteValue).toInt()
		});
	}
	return res;
}





void Database::addVotes(const QString & a_TableName, const std::vector<Database::Vote> & a_Votes)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO " + a_TableName + " (SongHash, DateAdded, VoteValue) VALUES(?, ?, ?)"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return;
	}
	for (const auto & item: a_Votes)
	{
		query.addBindValue(item.m_SongHash);
		query.addBindValue(item.m_DateAdded);
		query.addBindValue(item.m_VoteValue);
		if (!query.exec())
		{
			qWarning() << "Cannot store vote item: " << query.lastError();
			assert(!"DB error");
		}
	}
}





void Database::loadSongs()
{
	// First load the shared data:
	loadSongSharedData();
	loadSongFiles();
	loadNewFiles();

	// Enqueue songs with unknown length for length calc (#141):
	int numForRescan = 0;
	for (const auto & sd: m_SongSharedData)
	{
		if (
			!sd.second->m_Length.isPresent() &&  // Unknown length
			!sd.second->duplicates().empty()     // There is at least one file for the hash
		)
		{
			qDebug()
				<< "Song with hash " << sd.first << " (" << sd.second->duplicates()[0]->fileName()
				<< ") needs length, queueing for rescan.";
			emit needSongLength(sd.second);
			numForRescan += 1;
		}
	}
	if (numForRescan > 0)
	{
		qWarning() << "Number of songs without length that were queued for rescan: "
			<< numForRescan << " out of " << m_SongSharedData.size();
	}
}





void Database::loadSongFiles()
{
	STOPWATCH("Loading songs from the DB");

	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT * FROM SongFiles"))
	{
		qWarning() << "Cannot query song files from the DB: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiFileName             = query.record().indexOf("FileName");
	auto fiHash                 = query.record().indexOf("Hash");
	auto fiLastTagRescanned     = query.record().indexOf("LastTagRescanned");
	auto fiNumTagRescanAttempts = query.record().indexOf("NumTagRescanAttempts");
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
	std::array<int, 4> fisInvalidLM
	{{
		-1, -1, -1, -1
	}};

	// Load each song:
	while (query.next())
	{
		const auto & rec = query.record();
		auto hash = fieldValue(rec.field(fiHash));
		if (!hash.isValid())
		{
			emit needFileHash(query.value(fiFileName).toString());
			continue;
		}
		auto fileName = query.value(fiFileName).toString();
		auto sharedData = m_SongSharedData.find(hash.toByteArray());
		if (sharedData == m_SongSharedData.end())
		{
			qWarning() << "Song " << fileName << " has a hash not present in SongSharedData; skipping song.";
			continue;
		}
		auto song = std::make_shared<Song>(
			std::move(fileName),
			sharedData->second,
			tagFromFields(rec, fisFileName, fisInvalidLM),
			tagFromFields(rec, fisId3,      fisInvalidLM),
			fieldValue(rec.field(fiLastTagRescanned)),
			fieldValue(rec.field(fiNumTagRescanAttempts))
		);
		m_Songs.push_back(song);
		if (song->needsTagRescan())
		{
			emit needSongTagRescan(song);
		}
	}
}





void Database::loadSongSharedData()
{
	STOPWATCH("Loading song shared data from the DB");

	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT * FROM SongSharedData"))
	{
		qWarning() << "Cannot query song shared data from the DB: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiHash                    = query.record().indexOf("Hash");
	auto fiLength                  = query.record().indexOf("Length");
	auto fiLastPlayed              = query.record().indexOf("LastPlayed");
	auto fiLastPlayedLM            = query.record().indexOf("LastPlayedLM");
	auto fiLocalRating             = query.record().indexOf("LocalRating");
	auto fiLocalRatingLM           = query.record().indexOf("LocalRatingLM");
	auto fiRatingRhythmClarity     = query.record().indexOf("RatingRhythmClarity");
	auto fiRatingRhythmClarityLM   = query.record().indexOf("RatingRhythmClarityLM");
	auto fiRatingGenreTypicality   = query.record().indexOf("RatingGenreTypicality");
	auto fiRatingGenreTypicalityLM = query.record().indexOf("RatingGenreTypicalityLM");
	auto fiRatingPopularity        = query.record().indexOf("RatingPopularity");
	auto fiRatingPopularityLM      = query.record().indexOf("RatingPopularityLM");
	auto fiSkipStart               = query.record().indexOf("SkipStart");
	auto fiSkipStartLM             = query.record().indexOf("SkipStartLM");
	auto fiNotes                   = query.record().indexOf("Notes");
	auto fiNotesLM                 = query.record().indexOf("NotesLM");
	std::array<int, 4> fisManual
	{{
		query.record().indexOf("ManualAuthor"),
		query.record().indexOf("ManualTitle"),
		query.record().indexOf("ManualGenre"),
		query.record().indexOf("ManualMeasuresPerMinute"),
	}};
	std::array<int, 4> fisManualLM
	{{
		query.record().indexOf("ManualAuthorLM"),
		query.record().indexOf("ManualTitleLM"),
		query.record().indexOf("ManualGenreLM"),
		query.record().indexOf("ManualMeasuresPerMinuteLM"),
	}};

	// Load each record:
	while (query.next())
	{
		const auto & rec = query.record();
		auto hash = query.value(fiHash).toByteArray();
		auto data = std::make_shared<Song::SharedData>(
			hash,
			datedOptionalFromFields<double>(rec, fiLength, -1),
			datedOptionalFromFields<QDateTime>(rec, fiLastPlayed, fiLastPlayedLM),
			Song::Rating({
				datedOptionalFromFields<double>(rec, fiRatingRhythmClarity,   fiRatingRhythmClarityLM),
				datedOptionalFromFields<double>(rec, fiRatingGenreTypicality, fiRatingGenreTypicalityLM),
				datedOptionalFromFields<double>(rec, fiRatingPopularity,      fiRatingPopularityLM),
				datedOptionalFromFields<double>(rec, fiLocalRating,           fiLocalRatingLM)
			}),
			tagFromFields(rec, fisManual, fisManualLM),
			datedOptionalFromFields<double>(rec, fiSkipStart, fiSkipStartLM),
			datedOptionalFromFields<QString>(rec, fiNotes, fiNotesLM)
		);
		m_SongSharedData[hash] = data;
	}
}





void Database::loadNewFiles()
{
	STOPWATCH("Loading new files from the DB");

	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT * FROM NewFiles"))
	{
		qWarning() << "Cannot query new files from the DB: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiFileName = query.record().indexOf("FileName");

	// Load and enqueue each file:
	while (query.next())
	{
		auto fileName = query.value(fiFileName).toString();
		emit needFileHash(fileName);
	}
}





void Database::loadFilters()
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT RowID, * FROM Filters ORDER BY Position ASC"))
	{
		qWarning() << "Cannot query filters from the DB: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiRowId         = query.record().indexOf("RowID");
	auto fiDisplayName   = query.record().indexOf("DisplayName");
	auto fiNotes         = query.record().indexOf("Notes");
	auto fiIsFavorite    = query.record().indexOf("IsFavorite");
	auto fiBgColor       = query.record().indexOf("BgColor");
	auto fiDurationLimit = query.record().indexOf("DurationLimit");

	// Load each template:
	if (!query.isActive())
	{
		qWarning() << "Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << "Not a select";
		assert(!"DB error");
		return;
	}
	while (query.next())
	{
		QColor c(query.value(fiBgColor).toString());
		auto filter = std::make_shared<Filter>(
			query.value(fiRowId).toLongLong(),
			query.value(fiDisplayName).toString(),
			query.value(fiNotes).toString(),
			query.value(fiIsFavorite).toBool(),
			c,
			DatedOptional<double>(query.value(fiDurationLimit), QDateTime())
		);
		m_Filters.push_back(filter);
		loadFilterNodes(*filter);
	}
}





void Database::loadFilterNodes(Filter & a_Filter)
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT RowID, * FROM FilterNodes WHERE FilterID = ?"))
	{
		qWarning() << "Cannot query filter nodes from the DB, "
			<< "filter " << a_Filter.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Filter.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot query filter nodes from the DB, "
			<< "filter " << a_Filter.displayName()
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
	Load each node.
	Each filter is loaded into a map of NodeRowID -> (ParentRowID, Node); then all nodes in the map
	are walked to assign children to the proper parent node. The node without any parent is set as
	the root node for the filter.
	*/
	std::map<qlonglong, std::pair<qlonglong, Filter::NodePtr>> nodes;
	if (!query.isActive())
	{
		qWarning() << "Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << "Not a select";
		assert(!"DB error");
		return;
	}
	while (query.next())
	{
		Filter::NodePtr node;
		Filter::Node::Kind kind;
		auto parentRowId = query.value(fiParentRowId).toLongLong();
		auto rowId = query.value(fiRowId).toLongLong();
		auto intKind = query.value(fiKind).toInt();
		try
		{
			kind = Filter::Node::intToKind(intKind);
		}
		catch (const RuntimeError & exc)
		{
			qWarning() << "Failed to load Kind for filter node for "
				<< " filter " << a_Filter.displayName()
				<< ", RowID = " << rowId
				<< ", err: " << exc.what();
			continue;
		}
		switch (kind)
		{
			case Filter::Node::nkComparison:
			{
				auto intComparison = query.value(fiComparison).toInt();
				int intProp = query.value(fiSongProperty).toInt();
				Filter::Node::Comparison comparison;
				Filter::Node::SongProperty prop;
				try
				{
					comparison = Filter::Node::intToComparison(intComparison);
				}
				catch (const RuntimeError & exc)
				{
					qWarning() << "Failed to load Comparison for filter node for "
						<< " filter " << a_Filter.displayName()
						<< ", RowID = " << rowId
						<< ", err: " << exc.what();
					continue;
				}
				try
				{
					prop = Filter::Node::intToSongProperty(intProp);
				}
				catch (const RuntimeError & exc)
				{
					qWarning() << "Failed to load SongProperty for filter node for "
						<< " filter " << a_Filter.displayName()
						<< ", RowID = " << rowId
						<< ", err: " << exc.what();
					continue;
				}
				node.reset(new Filter::Node(prop, comparison, query.value(fiValue)));
				break;
			}  // case Filter::Node::nkComparison

			case Filter::Node::nkAnd:
			case Filter::Node::nkOr:
			{
				node.reset(new Filter::Node(kind, {}));
				break;
			}
		}
		nodes[rowId] = std::make_pair(parentRowId, node);
	}

	// Assign nodes to their parents:
	bool hasSetRoot = false;
	for (auto & f: nodes)
	{
		auto parentId = f.second.first;
		auto & node = f.second.second;
		if (parentId < 0)
		{
			if (hasSetRoot)
			{
				qWarning() << "Multiple root filters detected, ignoring RowID " << f.first << ".";
				continue;
			}
			a_Filter.setRootNode(node);
			hasSetRoot = true;
		}
		else
		{
			auto parentItr = nodes.find(parentId);
			if (parentItr == nodes.end())
			{
				qWarning() << "Invalid filter node parent: " << parentId << " in rowid " << f.first;
				continue;
			}
			auto & parent = parentItr->second.second;
			if (!parent->canHaveChildren())
			{
				qWarning() << "Bad filter node parent, cannot have child nodes: " << f.first;
				continue;
			}
			parent->addChild(node);
		}
	}
}





FilterPtr Database::filterFromRowId(qlonglong a_RowId)
{
	for (const auto & filter: m_Filters)
	{
		if (filter->dbRowId() == a_RowId)
		{
			return filter;
		}
	}
	qDebug() << "Filter " << a_RowId << " not found";
	return nullptr;
}





void Database::loadTemplates()
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.exec("SELECT RowID, * FROM Templates ORDER BY Position ASC"))
	{
		qWarning() << "Cannot query templates from the DB: " << query.lastError();
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
		qWarning() << "Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << "Not a select";
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
		loadTemplateItems(*tmpl);
	}
}





void Database::loadTemplateItems(Template & a_Template)
{
	// Initialize the query:
	QSqlQuery query(m_Database);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT * FROM TemplateItems WHERE TemplateID = ? ORDER BY IndexInTemplate ASC"))
	{
		qWarning() << "Cannot query template items from the DB, template "
			<< a_Template.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Template.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot query template items from the DB, template "
			<< a_Template.displayName()
			<< ", exec error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	auto fiFilterId = query.record().indexOf("FilterID");

	// Load each template:
	if (!query.isActive())
	{
		qWarning() << "Query not active";
		assert(!"DB error");
		return;
	}
	if (!query.isSelect())
	{
		qWarning() << "Not a select";
		assert(!"DB error");
		return;
	}
	while (query.next())
	{
		auto filterId = query.value(fiFilterId).toLongLong();
		auto filter = filterFromRowId(filterId);
		if (filter == nullptr)
		{
			qWarning() << "Template " << a_Template.displayName() << " references non-existent filter "
				<< filterId << ", skipping.";
			continue;
		}
		a_Template.appendItem(filter);
	}
}





void Database::saveFilterNodes(
	qlonglong a_FilterRowId,
	const Filter::Node & a_Node,
	qlonglong a_ParentNodeRowId
)
{
	QSqlQuery query(m_Database);
	if (a_Node.canHaveChildren())
	{
		if (!query.prepare(
			"INSERT INTO FilterNodes (FilterID, ParentID, Kind) "
			"VALUES (?, ?, ?)"
		))
		{
			qWarning() << "Cannot insert filter node to the DB"
				", prep error:" << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_FilterRowId);
		query.addBindValue(a_ParentNodeRowId);
		query.addBindValue(static_cast<int>(a_Node.kind()));
	}
	else
	{
		if (!query.prepare(
			"INSERT INTO FilterNodes (FilterID, ParentID, Kind, SongProperty, Comparison, Value) "
			"VALUES (?, ?, ?, ?, ?, ?)"
		))
		{
			qWarning() << "Cannot insert filter node to the DB"
				", prep error:" << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_FilterRowId);
		query.addBindValue(a_ParentNodeRowId);
		query.addBindValue(static_cast<int>(Filter::Node::nkComparison));
		query.addBindValue(static_cast<int>(a_Node.songProperty()));
		query.addBindValue(static_cast<int>(a_Node.comparison()));
		query.addBindValue(a_Node.value());
	}
	if (!query.exec())
	{
		qWarning() << "Cannot insert filter node to the DB"
			", exec error:" << query.lastError();
		assert(!"DB error");
		return;
	}
	auto rowId = query.lastInsertId().toLongLong();

	// Recurse over the children:
	if (a_Node.canHaveChildren())
	{
		for (const auto & ch: a_Node.children())
		{
			saveFilterNodes(a_FilterRowId, *ch, rowId);
		}
	}
}





int Database::getSongWeight(const Song & a_Song, const Playlist * a_Playlist) const
{
	qint64 res = 10000;  // Base weight

	// Penalize the last played date:
	auto lastPlayed = a_Song.lastPlayed();
	if (lastPlayed.isPresent())
	{
		auto numDays = lastPlayed.value().daysTo(QDateTime::currentDateTime());
		res = res * (numDays + 1) / (numDays + 2);  // The more days have passed, the less penalty
	}

	// Penalize presence in the list:
	if (a_Playlist != nullptr)
	{
		int idx = 0;
		for (const auto & itm: a_Playlist->items())
		{
			auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(itm);
			if (spi != nullptr)
			{
				if (spi->song().get() == &a_Song)
				{
					// This song is already present, penalize depending on distance from the end (where presumably it is to be added):
					auto numInBetween = static_cast<int>(a_Playlist->items().size()) - idx;
					res = res * (numInBetween + 100) / (numInBetween + 200);
					// Do not stop processing - if present multiple times, penalize multiple times
				}
			}
			idx += 1;
		}
	}

	// Penalize by rating:
	const auto & rating = a_Song.rating();
	res = applyRating(res, rating.m_GenreTypicality);
	res = applyRating(res, rating.m_Popularity);
	res = applyRating(res, rating.m_RhythmClarity);

	if (res > std::numeric_limits<int>::max())
	{
		return std::numeric_limits<int>::max();
	}
	return static_cast<int>(res);
}





void Database::addVote(
	const QByteArray & a_SongHash,
	int a_VoteValue,
	const QString & a_TableName,
	DatedOptional<double> Song::Rating::* a_DstRating
)
{
	qDebug() << "Adding community vote " << a_VoteValue << " for " << a_TableName << " to song " << a_SongHash;

	// Store the vote in the DB:
	{
		QSqlQuery query(m_Database);
		if (!query.prepare("INSERT INTO " + a_TableName + " (SongHash, VoteValue, DateAdded) VALUES(?, ?, ?)"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_SongHash);
		query.addBindValue(a_VoteValue);
		query.addBindValue(QDateTime::currentDateTimeUtc());
		if (!query.exec())
		{
			qWarning() << "Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
	}

	// Update the song rating:
	auto sharedData = m_SongSharedData.find(a_SongHash);
	if (sharedData != m_SongSharedData.end())
	{
		QSqlQuery query(m_Database);
		if (!query.prepare("SELECT AVG(VoteValue) FROM " + a_TableName + " WHERE SongHash = ?"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_SongHash);
		if (!query.exec())
		{
			qWarning() << "Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		if (!query.first())
		{
			qWarning() << "Cannot move to first value in statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		auto avg = query.value(0).toDouble();
		sharedData->second->m_Rating.*a_DstRating = avg;
		saveSongSharedData(sharedData->second);
	}
}





void Database::songPlaybackStarted(SongPtr a_Song)
{
	auto now = QDateTime::currentDateTimeUtc();
	auto shared = a_Song->sharedData();
	if (shared != nullptr)
	{
		shared->m_LastPlayed = now;
		QMetaObject::invokeMethod(this, "saveSongSharedData", Q_ARG(Song::SharedDataPtr, shared));
	}
	QMetaObject::invokeMethod(this, "addPlaybackHistory", Q_ARG(SongPtr, a_Song), Q_ARG(QDateTime, now));
}





void Database::songHashCalculated(const QString & a_FileName, const QByteArray & a_Hash, double a_Length)
{
	assert(!a_FileName.isEmpty());
	assert(!a_Hash.isEmpty());

	// Insert the SharedData record, now that we know the song hash:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT OR IGNORE INTO SongSharedData (Hash, Length) VALUES (?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Hash);
	query.bindValue(1, a_Length);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Create the SharedData, if not already created:
	auto itr = m_SongSharedData.find(a_Hash);
	Song::SharedDataPtr sharedData;
	if (itr == m_SongSharedData.end())
	{
		// Create new SharedData:
		sharedData = std::make_shared<Song::SharedData>(a_Hash);
		m_SongSharedData[a_Hash] = sharedData;
		saveSongSharedData(sharedData);  // Hash has been inserted above, so now can be updated
	}
	else
	{
		sharedData = itr->second;
	}

	// Save into the DB:
	{
		SqlTransaction transaction(m_Database);
		QSqlQuery query(m_Database);
		if (!query.prepare("DELETE FROM NewFiles WHERE FileName = ?"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_FileName);
		if (!query.exec())
		{
			qWarning() << "Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.finish();
		query.clear();

		if (!query.prepare("INSERT OR IGNORE INTO SongFiles (FileName, Hash) VALUES (?, ?)"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(a_FileName);
		query.addBindValue(a_Hash);
		if (!query.exec())
		{
			qWarning() << "Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		transaction.commit();
	}

	// Create the Song object:
	auto song = std::make_shared<Song>(a_FileName, sharedData);
	m_Songs.push_back(song);
	emit songFileAdded(song);

	// We finally have the hash, we can scan for tags and other metadata:
	emit needSongTagRescan(song);
}





void Database::songHashFailed(const QString & a_FileName)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("DELETE FROM NewFiles WHERE FileName = ?"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_FileName);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::songLengthCalculated(Song::SharedDataPtr a_SharedData, double a_LengthSec)
{
	a_SharedData->m_Length = a_LengthSec;
	saveSongSharedData(a_SharedData);
}





void Database::saveSong(SongPtr a_Song)
{
	assert(a_Song != nullptr);
	assert(QThread::currentThread() == QApplication::instance()->thread());

	saveSongFileData(a_Song);
	if (a_Song->sharedData())
	{
		saveSongSharedData(a_Song->sharedData());
	}
	emit songSaved(a_Song);
}





void Database::saveSongFileData(SongPtr a_Song)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE SongFiles SET "
		"Hash = ?, "
		"FileNameAuthor = ?, FileNameTitle = ?, FileNameGenre = ?, FileNameMeasuresPerMinute = ?,"
		"ID3Author = ?, ID3Title = ?, ID3Genre = ?, ID3MeasuresPerMinute = ?,"
		"LastTagRescanned = ?,"
		"NumTagRescanAttempts = ? "
		"WHERE FileName = ?")
	)
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_Song->hash());
	query.addBindValue(a_Song->tagFileName().m_Author.toVariant());
	query.addBindValue(a_Song->tagFileName().m_Title.toVariant());
	query.addBindValue(a_Song->tagFileName().m_Genre.toVariant());
	query.addBindValue(a_Song->tagFileName().m_MeasuresPerMinute.toVariant());
	query.addBindValue(a_Song->tagId3().m_Author.toVariant());
	query.addBindValue(a_Song->tagId3().m_Title.toVariant());
	query.addBindValue(a_Song->tagId3().m_Genre.toVariant());
	query.addBindValue(a_Song->tagId3().m_MeasuresPerMinute.toVariant());
	query.addBindValue(a_Song->lastTagRescanned());
	query.addBindValue(a_Song->numTagRescanAttempts());
	query.addBindValue(a_Song->fileName());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::saveSongSharedData(Song::SharedDataPtr a_SharedData)
{
	QSqlQuery query(m_Database);
	if (!query.prepare("UPDATE SongSharedData SET "
		"Length = ?, LastPlayed = ?, LastPlayedLM = ?, "
		"LocalRating = ?, LocalRatingLM = ?,"
		"RatingRhythmClarity = ?,   RatingRhythmClarityLM = ?, "
		"RatingGenreTypicality = ?, RatingGenreTypicalityLM = ?, "
		"RatingPopularity = ?,      RatingPopularityLM = ?, "
		"ManualAuthor = ?, ManualAuthorLM = ?,"
		"ManualTitle = ?, ManualTitleLM = ?,"
		"ManualGenre = ?, ManualGenreLM = ?,"
		"ManualMeasuresPerMinute = ?, ManualMeasuresPerMinuteLM = ?,"
		"SkipStart = ?, SkipStartLM = ?, "
		"Notes = ?, NotesLM = ? "
		"WHERE Hash = ?")
	)
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(a_SharedData->m_Length.toVariant());
	query.addBindValue(a_SharedData->m_LastPlayed.toVariant());
	query.addBindValue(a_SharedData->m_LastPlayed.lastModification());
	query.addBindValue(a_SharedData->m_Rating.m_Local.toVariant());
	query.addBindValue(a_SharedData->m_Rating.m_Local.lastModification());
	query.addBindValue(a_SharedData->m_Rating.m_RhythmClarity.toVariant());
	query.addBindValue(a_SharedData->m_Rating.m_RhythmClarity.lastModification());
	query.addBindValue(a_SharedData->m_Rating.m_GenreTypicality.toVariant());
	query.addBindValue(a_SharedData->m_Rating.m_GenreTypicality.lastModification());
	query.addBindValue(a_SharedData->m_Rating.m_Popularity.toVariant());
	query.addBindValue(a_SharedData->m_Rating.m_Popularity.lastModification());
	query.addBindValue(a_SharedData->m_TagManual.m_Author.toVariant());
	query.addBindValue(a_SharedData->m_TagManual.m_Author.lastModification());
	query.addBindValue(a_SharedData->m_TagManual.m_Title.toVariant());
	query.addBindValue(a_SharedData->m_TagManual.m_Title.lastModification());
	query.addBindValue(a_SharedData->m_TagManual.m_Genre.toVariant());
	query.addBindValue(a_SharedData->m_TagManual.m_Genre.lastModification());
	query.addBindValue(a_SharedData->m_TagManual.m_MeasuresPerMinute.toVariant());
	query.addBindValue(a_SharedData->m_TagManual.m_MeasuresPerMinute.lastModification());
	query.addBindValue(a_SharedData->m_SkipStart.toVariant());
	query.addBindValue(a_SharedData->m_SkipStart.lastModification());
	query.addBindValue(a_SharedData->m_Notes.toVariant());
	query.addBindValue(a_SharedData->m_Notes.lastModification());
	query.addBindValue(a_SharedData->m_Hash);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	if (query.numRowsAffected() != 1)
	{
		qWarning() << "SongSharedData update failed, no such hash row";
		assert(!"DB_error");
		return;
	}
}





void Database::songScanned(SongPtr a_Song)
{
	a_Song->setLastTagRescanned(QDateTime::currentDateTimeUtc());
	saveSongFileData(a_Song);
}





void Database::addVoteRhythmClarity(QByteArray a_SongHash, int a_VoteValue)
{
	addVote(a_SongHash, a_VoteValue, "VotesRhythmClarity", &Song::Rating::m_RhythmClarity);
}





void Database::addVoteGenreTypicality(QByteArray a_SongHash, int a_VoteValue)
{
	addVote(a_SongHash, a_VoteValue, "VotesGenreTypicality", &Song::Rating::m_GenreTypicality);
}





void Database::addVotePopularity(QByteArray a_SongHash, int a_VoteValue)
{
	addVote(a_SongHash, a_VoteValue, "VotesPopularity", &Song::Rating::m_Popularity);
}





void Database::addPlaybackHistory(SongPtr a_Song, const QDateTime & a_Timestamp)
{
	// Add a history playlist record:
	QSqlQuery query(m_Database);
	if (!query.prepare("INSERT INTO PlaybackHistory (SongHash, Timestamp) VALUES (?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, a_Song->hash());
	query.bindValue(1, a_Timestamp);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}

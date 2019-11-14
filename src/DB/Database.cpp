#include "Database.hpp"
#include <cassert>
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
#include "../Stopwatch.hpp"
#include "../Playlist.hpp"
#include "../PlaylistItemSong.hpp"
#include "../InstallConfiguration.hpp"
#include "../Exception.hpp"
#include "DatabaseUpgrade.hpp"
#include "DatabaseBackup.hpp"





/** Returns the SQL field's value, taking its "isNull" into account.
For some reason Qt + Sqlite return an empty string variant if the field is null,
this function returns a null variant in such a case. */
static QVariant fieldValue(const QSqlField & aField)
{
	if (aField.isNull())
	{
		return QVariant();
	}
	return aField.value();
}





/** Constructs a DatedOptional from the two DB fields given the record to read and the field indices. */
template <typename T> static DatedOptional<T> datedOptionalFromFields(
	const QSqlRecord & aRecord,
	int aIdxValue,
	int aIdxLastModification
)
{
	return DatedOptional<T>(
		fieldValue(aRecord.field(aIdxValue)),
		aRecord.value(aIdxLastModification).toDateTime()
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
	const QSqlRecord & aRecord,
	const std::array<int, 4> & aIndices,
	const std::array<int, 4> & aIndicesLM
)
{
	Song::Tag res;
	if (aIndices[0] >= 0)
	{
		res.mAuthor = datedOptionalFromFields<QString>(aRecord, aIndices[0], aIndicesLM[0]);
	}
	if (aIndices[1] >= 0)
	{
		res.mTitle = datedOptionalFromFields<QString>(aRecord, aIndices[1], aIndicesLM[1]);
	}
	if (aIndices[2] >= 0)
	{
		res.mGenre = datedOptionalFromFields<QString>(aRecord, aIndices[2], aIndicesLM[2]);
	}
	if (aIndices[3] >= 0)
	{
		res.mMeasuresPerMinute = datedOptionalFromFields<double>(aRecord, aIndices[3], aIndicesLM[3]);
	}
	return res;
}





/** Applies the specified rating, if present, to the song weight. */
static qint64 applyRating(qint64 aCurrentWeight, const DatedOptional<double> & aRating)
{
	if (aRating.isPresent())
	{
		// Even zero-rating songs need *some* chance, so we add 1 to all ratings
		return static_cast<qint64>(aCurrentWeight * (aRating.value() + 1) / 5);
	}
	else
	{
		// Default to 2.5-star rating:
		return static_cast<qint64>(aCurrentWeight * 3.5 / 5);
	}
}





/** Implements a RAII-like behavior for transactions.
Unless explicitly committed, a transaction is rolled back upon destruction of this object. */
class SqlTransaction
{
public:

	/** Starts a transaction.
	If a transaction cannot be started, logs and throws a RuntimeError. */
	SqlTransaction(QSqlDatabase & aDB):
		mDB(aDB),
		mIsActive(aDB.transaction())
	{
		if (!mIsActive)
		{
			throw RuntimeError("DB doesn't support transactions: %1", mDB.lastError());
		}
	}


	/** Rolls back the transaction, unless it has been committed.
	If the transaction fails to roll back, an error is logged (but nothing thrown). */
	~SqlTransaction()
	{
		if (mIsActive)
		{
			if (!mDB.rollback())
			{
				qWarning() << "DB transaction rollback failed: " << mDB.lastError();
				return;
			}
		}
	}


	/** Commits the transaction.
	If a transaction wasn't started, or is already committed, logs and throws a LogicError.
	If the transaction fails to commit, throws a RuntimeError. */
	void commit()
	{
		if (!mIsActive)
		{
			throw RuntimeError("DB transaction not started");
		}
		if (!mDB.commit())
		{
			throw LogicError("DB transaction commit failed: %1", mDB.lastError());
		}
		mIsActive = false;
	}


protected:

	QSqlDatabase & mDB;
	bool mIsActive;
};





////////////////////////////////////////////////////////////////////////////////
// Database:

Database::Database(ComponentCollection & aComponents):
	mComponents(aComponents)
{
}





void Database::open(const QString & aDBFileName)
{
	assert(!mDatabase.isOpen());  // Opening another DB is not allowed

	static std::atomic<int> counter(0);
	auto connName = QString::fromUtf8("DB%1").arg(counter.fetch_add(1));
	mDatabase = QSqlDatabase::addDatabase("QSQLITE", connName);
	mDatabase.setDatabaseName(aDBFileName);
	if (!mDatabase.open())
	{
		throw RuntimeError(tr("Cannot open the DB file: %1"), mDatabase.lastError());
	}

	// Check DB version, if upgradeable, make a backup first:
	{
		auto query = std::make_unique<QSqlQuery>("SELECT MAX(Version) AS Version FROM Version", mDatabase);
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
				mDatabase.close();
				QSqlDatabase::removeDatabase(connName);

				// Backup:
				auto backupFolder = mComponents.get<InstallConfiguration>()->dbBackupsFolder();
				DatabaseBackup::backupBeforeUpgrade(aDBFileName, version, backupFolder);

				// Reopen the DB:
				mDatabase = QSqlDatabase::addDatabase("QSQLITE", connName);
				mDatabase.setDatabaseName(aDBFileName);
				if (!mDatabase.open())
				{
					throw RuntimeError(tr("Cannot open the DB file: %1"), mDatabase.lastError());
				}
			}
		}
	}

	// Turn off synchronous queries (they slow up DB inserts by orders of magnitude):
	auto query = mDatabase.exec("PRAGMA synchronous = off");
	if (query.lastError().type() != QSqlError::NoError)
	{
		qWarning() << "Turning off synchronous failed: " << query.lastError();
		// Continue, this is not a hard error, just perf may be bad
	}

	// Turn on foreign keys:
	query = mDatabase.exec("PRAGMA foreign_keys = on");
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





void Database::addSongFiles(const QStringList & aFiles)
{
	for (const auto & f: aFiles)
	{
		addSongFile(f);
	}
}





void Database::addSongFile(const QString & aFileName)
{
	// Check for duplicates:
	for (const auto & song: mSongs)
	{
		if (song->fileName() == aFileName)
		{
			qDebug() << "Skipping duplicate " << aFileName;
			return;
		}
	}

	// Insert into DB:
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT OR IGNORE INTO NewFiles (FileName) VALUES(?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, aFileName);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Enqueue it in the Hash calculator:
	// (After the hash is calculated, only then a Song instance is created for the song, in songHashCalculated())
	emit needFileHash(aFileName);
}





void Database::removeSong(const Song & aSong, bool aDeleteDiskFile)
{
	size_t idx = 0;
	for (auto itr = mSongs.cbegin(), end = mSongs.cend(); itr != end; ++itr, ++idx)
	{
		if (itr->get() != &aSong)
		{
			continue;
		}
		auto song = *itr;
		emit songRemoving(song, idx);
		mSongs.erase(itr);
		song->sharedData()->delDuplicate(&aSong);

		// Remove from the DB:
		{
			QSqlQuery query(mDatabase);
			if (!query.prepare("DELETE FROM SongFiles WHERE FileName = ?"))
			{
				qWarning() << "Cannot prepare statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
			query.addBindValue(aSong.fileName());
			if (!query.exec())
			{
				qWarning() << "Cannot exec statement: " << query.lastError();
				assert(!"DB error");
				return;
			}
		}

		// Add a log entry:
		{
			QSqlQuery query(mDatabase);
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
				query.addBindValue(aSong.fileName());
				query.addBindValue(aSong.hash());
				query.addBindValue(QDateTime::currentDateTimeUtc());
				query.addBindValue(aDeleteDiskFile);
				query.addBindValue(static_cast<qulonglong>(aSong.sharedData()->duplicatesCount()));
				if (!query.exec())
				{
					qWarning() << "Cannot exec statement: " << query.lastError();
					assert(!"DB error");
				}
			}
		}

		// Delete the disk file, if requested:
		if (aDeleteDiskFile)
		{
			if (!QFile::remove(aSong.fileName()))
			{
				qWarning() << "Cannot delete disk file " << aSong.fileName();
			}
		}

		emit songRemoved(song, idx);
		return;
	}
	assert(!"Song not in the DB");
}





SongPtr Database::songFromHash(const QByteArray & aSongHash)
{
	auto sd = mSongSharedData.find(aSongHash);
	if (sd == mSongSharedData.end())
	{
		return nullptr;
	}
	auto dups = sd->second->duplicates();
	if (dups.empty())
	{
		return nullptr;
	}
	return dups[0]->shared_from_this();
}





SongPtr Database::songFromFileName(const QString aSongFileName)
{
	for (const auto & song: mSongs)
	{
		if (song->fileName() == aSongFileName)
		{
			return song;
		}
	}
	return nullptr;
}





TemplatePtr Database::createTemplate()
{
	auto res = std::make_shared<Template>(-1);
	addTemplate(res);
	return res;
}





void Database::addTemplate(TemplatePtr aTemplate)
{
	assert(aTemplate->dbRowId() == -1);  // No RowID assigned yet

	// Insert into DB:
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT INTO Templates (DisplayName, Notes) VALUES(?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, aTemplate->displayName());
	query.bindValue(1, aTemplate->notes());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Insert into memory:
	aTemplate->setDbRowId(query.lastInsertId().toLongLong());
	mTemplates.push_back(aTemplate);

	// Insert any items that are not yet stored in the DB:
	aTemplate->replaceSameFilters(mFilters);
	for (const auto & item: aTemplate->items())
	{
		if (item->dbRowId() == -1)
		{
			addFilter(item);
			saveFilter(*item);
		}
	}
}





void Database::swapTemplatesByIdx(size_t aIdx1, size_t aIdx2)
{
	assert(aIdx1 < mTemplates.size());
	assert(aIdx2 < mTemplates.size());

	if (aIdx1 == aIdx2)
	{
		assert(!"Attempting to swap a template with itself.");  // Most likely indicates an error in the caller
		return;
	}

	// Swap in memory:
	std::swap(mTemplates[aIdx1], mTemplates[aIdx2]);

	// Swap in the DB:
	QSqlQuery query(mDatabase);
	if (!query.prepare("UPDATE Templates SET Position = ? WHERE RowID = ?"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, static_cast<qulonglong>(aIdx1));
	query.bindValue(1, mTemplates[aIdx1]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, static_cast<qulonglong>(aIdx2));
	query.bindValue(1, mTemplates[aIdx2]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::delTemplate(const Template * aTemplate)
{
	for (auto itr = mTemplates.cbegin(), end = mTemplates.cend(); itr != end; ++itr)
	{
		if (itr->get() != aTemplate)
		{
			continue;
		}
		// Erase from the DB:
		QSqlQuery query(mDatabase);
		if (!query.prepare("DELETE FROM TemplateItems WHERE TemplateID = ?"))
		{
			qWarning() << "Cannot prep DELETE(TemplateItems) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(aTemplate->dbRowId());
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
		query.addBindValue(aTemplate->dbRowId());
		if (!query.exec())
		{
			qWarning() << "Cannot exec DELETE(Templates) statement: " << query.lastError();
			assert(!"DB error");
			return;
		}

		// Remove from the internal list:
		mTemplates.erase(itr);
		return;
	}
	qWarning() << "Attempting to remove a template not in the list: " << aTemplate->displayName();
	assert(!"Template not in list");
}





void Database::saveTemplate(const Template & aTemplate)
{
	// Find the template position:
	size_t position = mTemplates.size();
	for (size_t i = mTemplates.size(); i > 0;--i)
	{
		if (mTemplates[i - 1].get() == &aTemplate)
		{
			position = i - 1;
			break;
		}
	}
	assert(position < mTemplates.size());  // Template not found?

	// Update the template direct values:
	QSqlQuery query(mDatabase);
	if (!query.prepare("UPDATE Templates SET "
		"DisplayName = ?, Notes = ?, BgColor = ?, Position = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << "Cannot prepare UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(aTemplate.displayName());
	query.addBindValue(aTemplate.notes());
	query.addBindValue(aTemplate.bgColor().name());
	query.addBindValue(static_cast<qulonglong>(position));
	query.addBindValue(aTemplate.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Remove all template's items from the DB:
	if (query.prepare("DELETE FROM TemplateItems WHERE TemplateID = ?"))
	{
		query.addBindValue(aTemplate.dbRowId());
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
	for (const auto & filter: aTemplate.items())
	{
		query.addBindValue(aTemplate.dbRowId());
		query.addBindValue(idx);
		query.addBindValue(filter->dbRowId());
		if (!query.exec())
		{
			qWarning() << "Failed to save template " << aTemplate.displayName() << " item " << filter->displayName();
			assert(!"DB error");
			// Continue saving
		}
		idx += 1;
	}
}





void Database::saveAllTemplates()
{
	// TODO: This could be optimized - no need to calc position for each template, and wrap in a transaction
	for (const auto & tmpl: mTemplates)
	{
		saveTemplate(*tmpl);
	}
}





FilterPtr Database::createFilter()
{
	auto res = std::make_shared<Filter>();
	addFilter(res);
	return res;
}





void Database::addFilter(FilterPtr aFilter)
{
	assert(aFilter->dbRowId() == -1);  // No RowID assigned yet

	// Insert into DB:
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT INTO Filters (DisplayName, Notes) VALUES(?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, aFilter->displayName());
	query.bindValue(1, aFilter->notes());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Insert into memory:
	aFilter->setDbRowId(query.lastInsertId().toLongLong());
	mFilters.push_back(aFilter);
}





void Database::swapFiltersByIdx(size_t aIdx1, size_t aIdx2)
{
	assert(aIdx1 < mFilters.size());
	assert(aIdx2 < mFilters.size());

	if (aIdx1 == aIdx2)
	{
		assert(!"Attempting to swap a filter with itself.");  // Most likely indicates an error in the caller
		return;
	}

	// Swap in memory:
	std::swap(mFilters[aIdx1], mFilters[aIdx2]);

	// Swap in the DB:
	QSqlQuery query(mDatabase);
	if (!query.prepare("UPDATE Filters SET Position = ? WHERE RowID = ?"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, static_cast<qulonglong>(aIdx1));
	query.bindValue(1, mFilters[aIdx1]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, static_cast<qulonglong>(aIdx2));
	query.bindValue(1, mFilters[aIdx2]->dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::delFilter(size_t aIndex)
{
	if (aIndex >= mFilters.size())
	{
		return;
	}
	auto filter = mFilters[aIndex];
	using diffType = std::vector<FilterPtr>::difference_type;
	mFilters.erase(mFilters.begin() + static_cast<diffType>(aIndex));

	// Delete the filter from any templates using it:
	SqlTransaction trans(mDatabase);
	for (auto & tmpl: mTemplates)
	{
		if (tmpl->removeAllFilterRefs(filter))
		{
			saveTemplate(*tmpl);
		}
	}

	// Delete the filter nodes from the DB:
	QSqlQuery query(mDatabase);
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





void Database::saveFilter(const Filter & aFilter)
{
	// Find the filter's position:
	size_t position = mFilters.size();
	for (size_t i = mFilters.size(); i > 0;--i)
	{
		if (mFilters[i - 1].get() == &aFilter)
		{
			position = i - 1;
			break;
		}
	}
	assert(position < mFilters.size());  // Filter not found?

	// Update the filter's direct values:
	QSqlQuery query(mDatabase);
	if (!query.prepare("UPDATE Filters SET "
		"DisplayName = ?, Notes = ?, BgColor = ?, IsFavorite = ?, DurationLimit = ?, Position = ? "
		"WHERE RowID = ?")
	)
	{
		qWarning() << "Cannot prepare UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(aFilter.displayName());
	query.addBindValue(aFilter.notes());
	query.addBindValue(aFilter.bgColor().name());
	query.addBindValue(aFilter.isFavorite());
	query.addBindValue(aFilter.durationLimit().toVariant());
	query.addBindValue(static_cast<qulonglong>(position));
	query.addBindValue(aFilter.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot exec UPDATE statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Remove all filter's nodes from the DB:
	if (query.prepare("DELETE FROM FilterNodes WHERE FilterID = ?"))
	{
		query.addBindValue(aFilter.dbRowId());
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
	saveFilterNodes(aFilter.dbRowId(), *aFilter.rootNode(), -1);
}





FilterPtr Database::filterFromHash(const QByteArray & aFilterHash)
{
	for (const auto & filter: mFilters)
	{
		if (filter->hash() == aFilterHash)
		{
			return filter;
		}
	}
	return nullptr;
}





std::vector<FilterPtr> Database::getFavoriteFilters() const
{
	std::vector<FilterPtr> res;
	for (const auto & filter: mFilters)
	{
		if (filter->isFavorite())
		{
			res.push_back(filter);
		}
	}
	return res;
}





int Database::numSongsMatchingFilter(const Filter & aFilter) const
{
	int res = 0;
	for (const auto & s: mSongs)
	{
		if (aFilter.rootNode()->isSatisfiedBy(*s))
		{
			res += 1;
		}
	}
	return res;
}





int Database::numTemplatesContaining(const Filter & aFilter) const
{
	int res = 0;
	for (const auto & tmpl: mTemplates)
	{
		for (const auto & item: tmpl->items())
		{
			if (item.get() == &aFilter)
			{
				res += 1;
				break;
			}
		}
	}
	return res;
}





SongPtr Database::pickSongForFilter(const Filter & aFilter, SongPtr aAvoid) const
{
	std::vector<std::pair<SongPtr, int>> songs;  // Pairs of SongPtr and their weight
	std::set<Song::SharedDataPtr> sharedDatas;  // SharedDatas of songs added to "songs", to avoid dupes
	int totalWeight = 0;
	for (const auto & song: mSongs)
	{
		if (song == aAvoid)
		{
			continue;
		}
		if (sharedDatas.find(song->sharedData()) != sharedDatas.cend())
		{
			// Already present, through another Song, with the same hash
			continue;
		}
		if (aFilter.rootNode()->isSatisfiedBy(*song))
		{
			auto weight = getSongWeight(*song);
			songs.push_back(std::make_pair(song, weight));
			totalWeight += weight;
			sharedDatas.insert(song->sharedData());
		}
	}

	if (songs.empty())
	{
		if ((aAvoid != nullptr) && (aFilter.rootNode()->isSatisfiedBy(*aAvoid)))
		{
			return aAvoid;
		}
		qDebug() << "No song matches item " << aFilter.displayName();
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
			f << "Choices for template item " << aItem->displayName().toStdString() << std::endl;
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





std::vector<std::pair<SongPtr, FilterPtr>> Database::pickSongsForTemplate(const Template & aTemplate)
{
	std::vector<std::pair<SongPtr, FilterPtr>> res;
	for (const auto & filter: aTemplate.items())
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

	QSqlQuery query(mDatabase);
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
		rsp->mFileName      = query.value(fiFileName).toString();
		rsp->mDateRemoved   = query.value(fiDateRemoved).toDateTime();
		rsp->mHash          = query.value(fiHash).toByteArray();
		rsp->mWasDeleted    = query.value(fiWasDeleted).toBool();
		rsp->mNumDuplicates = query.value(fiNumDuplicates).toInt();
		res.push_back(rsp);
	}
	return res;
}





void Database::clearRemovedSongs()
{
	auto query = mDatabase.exec("DELETE FROM RemovedSongs");
	if (query.lastError().type() != QSqlError::NoError)
	{
		qWarning() << "SQL query failed: " << query.lastError();
		return;
	}
}





Song::SharedDataPtr Database::sharedDataFromHash(const QByteArray & aHash) const
{
	auto itr = mSongSharedData.find(aHash);
	if (itr == mSongSharedData.end())
	{
		return nullptr;
	}
	return itr->second;
}





void Database::saveAllSongSharedData()
{
	for (const auto & sd: mSongSharedData)
	{
		saveSongSharedData(sd.second);
	}
}





std::vector<Database::HistoryItem> Database::playbackHistory() const
{
	std::vector<Database::HistoryItem> res;

	QSqlQuery query(mDatabase);
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





void Database::addPlaybackHistory(const std::vector<Database::HistoryItem> & aHistory)
{
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT INTO PlaybackHistory (Timestamp, SongHash) VALUES(?, ?)"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return;
	}
	for (const auto & item: aHistory)
	{
		query.addBindValue(item.mTimestamp);
		query.addBindValue(item.mHash);
		if (!query.exec())
		{
			qWarning() << "Cannot store playback history item "
				<< item.mTimestamp << ", "
				<< item.mHash << ": "
				<< query.lastError();
			assert(!"DB error");
		}
	}
}





void Database::addSongRemovalHistory(const std::vector<Database::RemovedSongPtr> & aHistory)
{
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT INTO RemovedSongs "
		"(Filename, Hash, DateRemoved, WasFileDeleted, NumDuplicates) VALUES "
		"(?, ?, ?, ?, ?)"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return;
	}
	for (const auto & item: aHistory)
	{
		query.addBindValue(item->mFileName);
		query.addBindValue(item->mHash);
		query.addBindValue(item->mDateRemoved);
		query.addBindValue(item->mWasDeleted);
		query.addBindValue(item->mNumDuplicates);
		if (!query.exec())
		{
			qWarning() << "Cannot store removal history item "
				<< item->mFileName << ", "
				<< item->mHash << ", "
				<< item->mDateRemoved << ": "
				<< query.lastError();
			assert(!"DB error");
		}
	}
}





std::vector<Database::Vote> Database::loadVotes(const QString & aTableName) const
{
	std::vector<Database::Vote> res;

	QSqlQuery query(mDatabase);
	if (!query.exec("SELECT * FROM " + aTableName + " ORDER BY DateAdded"))
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





void Database::addVotes(const QString & aTableName, const std::vector<Database::Vote> & aVotes)
{
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT INTO " + aTableName + " (SongHash, DateAdded, VoteValue) VALUES(?, ?, ?)"))
	{
		qWarning() << "Cannot prepare query: " << query.lastError();
		qDebug() << query.lastQuery();
		assert(!"DB error");
		return;
	}
	for (const auto & item: aVotes)
	{
		query.addBindValue(item.mSongHash);
		query.addBindValue(item.mDateAdded);
		query.addBindValue(item.mVoteValue);
		if (!query.exec())
		{
			qWarning() << "Cannot store vote item: " << query.lastError();
			assert(!"DB error");
		}
	}
}





quint32 Database::removeInaccessibleSongs()
{
	// NOTE: This function is called from a BackgroundTasks thread, needs to synchronize DB access
	unsigned res = 0;
	auto songs = mSongs;  // Make a copy, we're running in a background thread
	for (const auto & song: songs)
	{
		if (!QFile::exists(song->fileName()))
		{
			qDebug() << "Song file " << song->fileName() << " doesn't exist, removing";
			QMetaObject::invokeMethod(this, "removeSong", Q_ARG(SongPtr, song));
			res += 1;
		}
	}
	return res;
}





void Database::addToSharedDataManualTags(const std::map<QByteArray, Song::Tag> & aTags)
{
	for (const auto & sdt: aTags)
	{
		auto itr = mSongSharedData.find(sdt.first);
		if (itr == mSongSharedData.end())
		{
			continue;
		}
		const auto & tag = sdt.second;
		itr->second->mTagManual.mAuthor.updateIfNewer(tag.mAuthor);
		itr->second->mTagManual.mTitle.updateIfNewer(tag.mTitle);
		itr->second->mTagManual.mGenre.updateIfNewer(tag.mGenre);
		itr->second->mTagManual.mMeasuresPerMinute.updateIfNewer(tag.mMeasuresPerMinute);
	}
	saveAllSongSharedData();
}





bool Database::renameFile(Song & aSong, const QString & aFileName)
{
	if (!QFile::rename(aSong.fileName(), aFileName))
	{
		return false;
	}
	aSong.setFileName(aFileName);
	return true;
}





void Database::loadSongs()
{
	// First load the shared data:
	loadSongSharedData();
	loadSongFiles();
	loadNewFiles();

	// Enqueue songs with unknown length for length calc (#141):
	int numForRescan = 0;
	for (const auto & sd: mSongSharedData)
	{
		if (
			!sd.second->mLength.isPresent() &&  // Unknown length
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
			<< numForRescan << " out of " << mSongSharedData.size();
	}
}





void Database::loadSongFiles()
{
	STOPWATCH("Loading songs from the DB");

	// Initialize the query:
	QSqlQuery query(mDatabase);
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
		auto sharedData = mSongSharedData.find(hash.toByteArray());
		if (sharedData == mSongSharedData.end())
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
		mSongs.push_back(song);
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
	QSqlQuery query(mDatabase);
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
	auto fiBgColor                 = query.record().indexOf("BgColor");
	auto fiBgColorLM               = query.record().indexOf("BgColorLM");
	auto fiDetectedTempo           = query.record().indexOf("DetectedTempo");
	auto fiDetectedTempoLM         = query.record().indexOf("DetectedTempoLM");
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
			datedOptionalFromFields<QString>(rec, fiNotes, fiNotesLM),
			datedOptionalFromFields<QColor> (rec, fiBgColor, fiBgColorLM),
			datedOptionalFromFields<double> (rec, fiDetectedTempo, fiDetectedTempoLM)
		);
		mSongSharedData[hash] = data;
	}
}





void Database::loadNewFiles()
{
	STOPWATCH("Loading new files from the DB");

	// Initialize the query:
	QSqlQuery query(mDatabase);
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
	QSqlQuery query(mDatabase);
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
		mFilters.push_back(filter);
		loadFilterNodes(*filter);
	}
}





void Database::loadFilterNodes(Filter & aFilter)
{
	// Initialize the query:
	QSqlQuery query(mDatabase);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT RowID, * FROM FilterNodes WHERE FilterID = ?"))
	{
		qWarning() << "Cannot query filter nodes from the DB, "
			<< "filter " << aFilter.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(aFilter.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot query filter nodes from the DB, "
			<< "filter " << aFilter.displayName()
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
				<< " filter " << aFilter.displayName()
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
						<< " filter " << aFilter.displayName()
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
						<< " filter " << aFilter.displayName()
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
			aFilter.setRootNode(node);
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





FilterPtr Database::filterFromRowId(qlonglong aRowId)
{
	for (const auto & filter: mFilters)
	{
		if (filter->dbRowId() == aRowId)
		{
			return filter;
		}
	}
	qDebug() << "Filter " << aRowId << " not found";
	return nullptr;
}





void Database::loadTemplates()
{
	// Initialize the query:
	QSqlQuery query(mDatabase);
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
		mTemplates.push_back(tmpl);
		loadTemplateItems(*tmpl);
	}
}





void Database::loadTemplateItems(Template & aTemplate)
{
	// Initialize the query:
	QSqlQuery query(mDatabase);
	query.setForwardOnly(true);
	if (!query.prepare("SELECT * FROM TemplateItems WHERE TemplateID = ? ORDER BY IndexInTemplate ASC"))
	{
		qWarning() << "Cannot query template items from the DB, template "
			<< aTemplate.displayName()
			<< ", prep error: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(aTemplate.dbRowId());
	if (!query.exec())
	{
		qWarning() << "Cannot query template items from the DB, template "
			<< aTemplate.displayName()
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
			qWarning() << "Template " << aTemplate.displayName() << " references non-existent filter "
				<< filterId << ", skipping.";
			continue;
		}
		aTemplate.appendItem(filter);
	}
}





void Database::saveFilterNodes(
	qlonglong aFilterRowId,
	const Filter::Node & aNode,
	qlonglong aParentNodeRowId
)
{
	QSqlQuery query(mDatabase);
	if (aNode.canHaveChildren())
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
		query.addBindValue(aFilterRowId);
		query.addBindValue(aParentNodeRowId);
		query.addBindValue(static_cast<int>(aNode.kind()));
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
		query.addBindValue(aFilterRowId);
		query.addBindValue(aParentNodeRowId);
		query.addBindValue(static_cast<int>(Filter::Node::nkComparison));
		query.addBindValue(static_cast<int>(aNode.songProperty()));
		query.addBindValue(static_cast<int>(aNode.comparison()));
		query.addBindValue(aNode.value());
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
	if (aNode.canHaveChildren())
	{
		for (const auto & ch: aNode.children())
		{
			saveFilterNodes(aFilterRowId, *ch, rowId);
		}
	}
}





int Database::getSongWeight(const Song & aSong, const Playlist * aPlaylist) const
{
	qint64 res = 10000;  // Base weight

	// Penalize the last played date:
	auto lastPlayed = aSong.lastPlayed();
	if (lastPlayed.isPresent())
	{
		auto numDays = lastPlayed.value().daysTo(QDateTime::currentDateTime());
		res = res * (numDays + 1) / (numDays + 2);  // The more days have passed, the less penalty
	}

	// Penalize presence in the list:
	if (aPlaylist != nullptr)
	{
		int idx = 0;
		for (const auto & itm: aPlaylist->items())
		{
			auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(itm);
			if (spi != nullptr)
			{
				if (spi->song().get() == &aSong)
				{
					// This song is already present, penalize depending on distance from the end (where presumably it is to be added):
					auto numInBetween = static_cast<int>(aPlaylist->items().size()) - idx;
					res = res * (numInBetween + 100) / (numInBetween + 200);
					// Do not stop processing - if present multiple times, penalize multiple times
				}
			}
			idx += 1;
		}
	}

	// Penalize by rating:
	const auto & rating = aSong.rating();
	res = applyRating(res, rating.mGenreTypicality);
	res = applyRating(res, rating.mPopularity);
	res = applyRating(res, rating.mRhythmClarity);

	if (res > std::numeric_limits<int>::max())
	{
		return std::numeric_limits<int>::max();
	}
	return static_cast<int>(res);
}





void Database::addVote(
	const QByteArray & aSongHash,
	int aVoteValue,
	const QString & aTableName,
	DatedOptional<double> Song::Rating::* aDstRating
)
{
	qDebug() << "Adding community vote " << aVoteValue << " for " << aTableName << " to song " << aSongHash;

	// Store the vote in the DB:
	{
		QSqlQuery query(mDatabase);
		if (!query.prepare("INSERT INTO " + aTableName + " (SongHash, VoteValue, DateAdded) VALUES(?, ?, ?)"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(aSongHash);
		query.addBindValue(aVoteValue);
		query.addBindValue(QDateTime::currentDateTimeUtc());
		if (!query.exec())
		{
			qWarning() << "Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
	}

	// Update the song rating:
	auto sharedData = mSongSharedData.find(aSongHash);
	if (sharedData != mSongSharedData.end())
	{
		QSqlQuery query(mDatabase);
		if (!query.prepare("SELECT AVG(VoteValue) FROM " + aTableName + " WHERE SongHash = ?"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(aSongHash);
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
		sharedData->second->mRating.*aDstRating = avg;
		saveSongSharedData(sharedData->second);
	}
}





void Database::songPlaybackStarted(SongPtr aSong)
{
	auto now = QDateTime::currentDateTimeUtc();
	auto shared = aSong->sharedData();
	if (shared != nullptr)
	{
		shared->mLastPlayed = now;
		QMetaObject::invokeMethod(this, "saveSongSharedData", Q_ARG(Song::SharedDataPtr, shared));
	}
	QMetaObject::invokeMethod(this, "addPlaybackHistory", Q_ARG(SongPtr, aSong), Q_ARG(QDateTime, now));
}





void Database::songHashCalculated(const QString & aFileName, const QByteArray & aHash, double aLength)
{
	assert(!aFileName.isEmpty());
	assert(!aHash.isEmpty());

	// Insert the SharedData record, now that we know the song hash:
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT OR IGNORE INTO SongSharedData (Hash, Length) VALUES (?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, aHash);
	query.bindValue(1, aLength);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}

	// Create the SharedData, if not already created:
	auto itr = mSongSharedData.find(aHash);
	Song::SharedDataPtr sharedData;
	if (itr == mSongSharedData.end())
	{
		// Create new SharedData:
		sharedData = std::make_shared<Song::SharedData>(aHash, aLength);
		mSongSharedData[aHash] = sharedData;
		saveSongSharedData(sharedData);  // Hash has been inserted above, so now can be updated
	}
	else
	{
		sharedData = itr->second;
	}

	// Save into the DB:
	{
		SqlTransaction transaction(mDatabase);
		QSqlQuery query(mDatabase);
		if (!query.prepare("DELETE FROM NewFiles WHERE FileName = ?"))
		{
			qWarning() << "Cannot prepare statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		query.addBindValue(aFileName);
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
		query.addBindValue(aFileName);
		query.addBindValue(aHash);
		if (!query.exec())
		{
			qWarning() << "Cannot exec statement: " << query.lastError();
			assert(!"DB error");
			return;
		}
		transaction.commit();
	}

	// Create the Song object:
	auto song = std::make_shared<Song>(aFileName, sharedData);
	mSongs.push_back(song);
	emit songFileAdded(song);

	// We finally have the hash, we can scan for tags and other metadata:
	emit needSongTagRescan(song);
}





void Database::songHashFailed(const QString & aFileName)
{
	QSqlQuery query(mDatabase);
	if (!query.prepare("DELETE FROM NewFiles WHERE FileName = ?"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(aFileName);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::songLengthCalculated(Song::SharedDataPtr aSharedData, double aLengthSec)
{
	aSharedData->mLength = aLengthSec;
	saveSongSharedData(aSharedData);
}





void Database::saveSong(SongPtr aSong)
{
	assert(aSong != nullptr);
	assert(QThread::currentThread() == QApplication::instance()->thread());

	saveSongFileData(aSong);
	if (aSong->sharedData())
	{
		saveSongSharedData(aSong->sharedData());
	}
	emit songSaved(aSong);
}





void Database::saveSongFileData(SongPtr aSong)
{
	QSqlQuery query(mDatabase);
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
	query.addBindValue(aSong->hash());
	query.addBindValue(aSong->tagFileName().mAuthor.toVariant());
	query.addBindValue(aSong->tagFileName().mTitle.toVariant());
	query.addBindValue(aSong->tagFileName().mGenre.toVariant());
	query.addBindValue(aSong->tagFileName().mMeasuresPerMinute.toVariant());
	query.addBindValue(aSong->tagId3().mAuthor.toVariant());
	query.addBindValue(aSong->tagId3().mTitle.toVariant());
	query.addBindValue(aSong->tagId3().mGenre.toVariant());
	query.addBindValue(aSong->tagId3().mMeasuresPerMinute.toVariant());
	query.addBindValue(aSong->lastTagRescanned());
	query.addBindValue(aSong->numTagRescanAttempts());
	query.addBindValue(aSong->fileName());
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}





void Database::saveSongSharedData(Song::SharedDataPtr aSharedData)
{
	QSqlQuery query(mDatabase);
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
		"Notes = ?, NotesLM = ?, "
		"BgColor = ?, BgColorLM = ?, "
		"DetectedTempo = ?, DetectedTempoLM = ? "
		"WHERE Hash = ?")
	)
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.addBindValue(aSharedData->mLength.toVariant());
	query.addBindValue(aSharedData->mLastPlayed.toVariant());
	query.addBindValue(aSharedData->mLastPlayed.lastModification());
	query.addBindValue(aSharedData->mRating.mLocal.toVariant());
	query.addBindValue(aSharedData->mRating.mLocal.lastModification());
	query.addBindValue(aSharedData->mRating.mRhythmClarity.toVariant());
	query.addBindValue(aSharedData->mRating.mRhythmClarity.lastModification());
	query.addBindValue(aSharedData->mRating.mGenreTypicality.toVariant());
	query.addBindValue(aSharedData->mRating.mGenreTypicality.lastModification());
	query.addBindValue(aSharedData->mRating.mPopularity.toVariant());
	query.addBindValue(aSharedData->mRating.mPopularity.lastModification());
	query.addBindValue(aSharedData->mTagManual.mAuthor.toVariant());
	query.addBindValue(aSharedData->mTagManual.mAuthor.lastModification());
	query.addBindValue(aSharedData->mTagManual.mTitle.toVariant());
	query.addBindValue(aSharedData->mTagManual.mTitle.lastModification());
	query.addBindValue(aSharedData->mTagManual.mGenre.toVariant());
	query.addBindValue(aSharedData->mTagManual.mGenre.lastModification());
	query.addBindValue(aSharedData->mTagManual.mMeasuresPerMinute.toVariant());
	query.addBindValue(aSharedData->mTagManual.mMeasuresPerMinute.lastModification());
	query.addBindValue(aSharedData->mSkipStart.toVariant());
	query.addBindValue(aSharedData->mSkipStart.lastModification());
	query.addBindValue(aSharedData->mNotes.toVariant());
	query.addBindValue(aSharedData->mNotes.lastModification());
	query.addBindValue(aSharedData->mBgColor.toVariant());
	query.addBindValue(aSharedData->mBgColor.lastModification());
	query.addBindValue(aSharedData->mDetectedTempo.toVariant());
	query.addBindValue(aSharedData->mDetectedTempo.lastModification());
	query.addBindValue(aSharedData->mHash);
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





void Database::songScanned(SongPtr aSong)
{
	aSong->setLastTagRescanned(QDateTime::currentDateTimeUtc());
	saveSongFileData(aSong);
}





void Database::addVoteRhythmClarity(QByteArray aSongHash, int aVoteValue)
{
	addVote(aSongHash, aVoteValue, "VotesRhythmClarity", &Song::Rating::mRhythmClarity);
}





void Database::addVoteGenreTypicality(QByteArray aSongHash, int aVoteValue)
{
	addVote(aSongHash, aVoteValue, "VotesGenreTypicality", &Song::Rating::mGenreTypicality);
}





void Database::addVotePopularity(QByteArray aSongHash, int aVoteValue)
{
	addVote(aSongHash, aVoteValue, "VotesPopularity", &Song::Rating::mPopularity);
}





void Database::addPlaybackHistory(SongPtr aSong, const QDateTime & aTimestamp)
{
	// Add a history playlist record:
	QSqlQuery query(mDatabase);
	if (!query.prepare("INSERT INTO PlaybackHistory (SongHash, Timestamp) VALUES (?, ?)"))
	{
		qWarning() << "Cannot prepare statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
	query.bindValue(0, aSong->hash());
	query.bindValue(1, aTimestamp);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return;
	}
}

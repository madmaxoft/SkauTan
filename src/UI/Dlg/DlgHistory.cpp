#include "DlgHistory.hpp"
#include "ui_DlgHistory.h"
#include <QDebug>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "DlgSongProperties.hpp"





/** The number of ticks (periodic UI updates) to wait between the user changing the search text
and applying it to the filter. This is to avoid slowdowns while typing the search text. */
static const int TICKS_UNTIL_SET_SEARCH_TEXT = 3;




/** Returns the string representation of the song's tempo.
Returns empty string if tempo not available. */
static QString formatMPM(const DatedOptional<double> & aSongMPM)
{
	if (!aSongMPM.isPresent())
	{
		return {};
	}
	return QLocale().toString(aSongMPM.value(), 'f', 1);
}





/** Returns the string representation of the date (and time). */
static QString formatDate(const QDateTime aDateTime)
{
	return aDateTime.toString("yyyy-MM-dd HH:mm:ss");
}





class HistoryModel:
	public QAbstractTableModel
{
	using Super = QAbstractTableModel;


public:

	enum Column
	{
		colDate,
		colGenre,
		colMPM,
		colAuthor,
		colTitle,

		colMax,
	};


	enum
	{
		roleSharedData = Qt::UserRole + 1,
	};


	HistoryModel(Database & aDB):
		mDB(aDB),
		mSongSharedDataMap(aDB.songSharedDataMap()),
		mItems(aDB.playbackHistory())
	{
		// Sort the history by datetime, most recent at the top:
		std::sort(mItems.begin(), mItems.end(),
			[](auto aItem1, auto aItem2)
			{
				return (aItem1.mTimestamp > aItem2.mTimestamp);
			}
		);
	}





	virtual int rowCount(const QModelIndex & aParent) const override
	{
		if (aParent.isValid())
		{
			return 0;
		}
		return static_cast<int>(mItems.size());
	}





	virtual int columnCount(const QModelIndex & aParent) const override
	{
		if (aParent.isValid())
		{
			return 0;
		}
		return colMax;
	}





	virtual QVariant data(const QModelIndex & aIndex, int aRole) const override
	{
		if (!aIndex.isValid())
		{
			return QVariant();
		}

		auto row = aIndex.row();
		if ((row < 0) || (static_cast<size_t>(row) > mItems.size()))
		{
			return QVariant();
		}
		const auto & item = mItems[static_cast<size_t>(row)];

		// If requesting the pointer to the shared data, return it:
		if (aRole == roleSharedData)
		{
			const auto sdItr = mSongSharedDataMap.find(item.mHash);
			if (sdItr == mSongSharedDataMap.end())
			{
				return QVariant();
			}
			return QVariant::fromValue(sdItr->second);
		}

		// Only process display data from now on:
		if (aRole != Qt::DisplayRole)
		{
			return QVariant();
		}

		const auto col = aIndex.column();
		if (col == colDate)  // Special case - doesn't need hash->song lookup
		{
			return formatDate(item.mTimestamp);
		}
		const auto sdItr = mSongSharedDataMap.find(item.mHash);
		if (sdItr == mSongSharedDataMap.end())
		{
			// Song hash is not in the DB at all
			return QVariant();
		}

		// If the song hash is in the DB, but no actual file is present, use the manual tag only:
		if (sdItr->second->duplicates().empty())
		{
			switch (col)
			{
				case colGenre:  return sdItr->second->mTagManual.mGenre.valueOrDefault();
				case colMPM:    return formatMPM(sdItr->second->mTagManual.mMeasuresPerMinute);
				case colAuthor: return sdItr->second->mTagManual.mAuthor.valueOrDefault();
				case colTitle:  return sdItr->second->mTagManual.mTitle.valueOrDefault();
			}
			return QVariant();
		}

		// Use the info from the first file for the song hash:
		const auto & song = *(sdItr->second->duplicates()[0]);
		switch (col)
		{
			case colGenre:  return song.primaryGenre().valueOrDefault();
			case colMPM:    return formatMPM(song.primaryMeasuresPerMinute());
			case colAuthor: return song.primaryAuthor().valueOrDefault();
			case colTitle:  return song.primaryTitle().valueOrDefault();
		}
		return QVariant();
	}




	virtual QVariant headerData(int aSection, Qt::Orientation aOrientation, int aRole) const override
	{
		if ((aOrientation != Qt::Horizontal) || (aRole != Qt::DisplayRole))
		{
			return QVariant();
		}
		switch (aSection)
		{
			case colDate:   return DlgHistory::tr("Date and time");
			case colGenre:  return DlgHistory::tr("Genre");
			case colMPM:    return DlgHistory::tr("MPM", "MeasuresPerMinute");
			case colAuthor: return DlgHistory::tr("Author");
			case colTitle:  return DlgHistory::tr("Title");
		}
		return QVariant();
	}



protected:

	/** The Database from which to query the data. */
	Database & mDB;

	/** SharedData map from the current DB for resolving hashes into song details. */
	using SongSharedDataMap = decltype(mDB.songSharedDataMap());
	SongSharedDataMap mSongSharedDataMap;

	/** The entire playback history, as read from the DB. */
	std::vector<Database::HistoryItem> mItems;
};





class HistoryModelFilter:
	public QSortFilterProxyModel
{
	using Super = QSortFilterProxyModel;

public:

	HistoryModelFilter(QObject * aParent):
		Super(aParent)
	{
	}


	/** Sets the text to filter by.
	Only items containing the specified text are shown in the model.
	If the text is empty, all items are shown. */
	void setSearchText(const QString & aSearchText)
	{
		mSearchText = aSearchText;
		invalidateFilter();
	}


protected:

	/** The text to be searched within the items. */
	QString mSearchText;


	// QSortFilterProxyModel overrides:
	virtual bool filterAcceptsRow(int aSrcRow, const QModelIndex & aSrcParent) const override
	{
		// Empty search text matches everything:
		if (mSearchText.isEmpty())
		{
			return true;
		}

		// Search for the text in the whole row:
		auto numCols = sourceModel()->columnCount(aSrcParent);
		for (int col = 0; col < numCols; ++col)
		{
			auto txt = sourceModel()->data(sourceModel()->index(aSrcRow, col, aSrcParent)).toString();
			if (txt.contains(mSearchText, Qt::CaseInsensitive))
			{
				return true;
			}
		}
		return false;
	}
};





////////////////////////////////////////////////////////////////////////////////
// DlgHistory:

DlgHistory::DlgHistory(ComponentCollection & aComponents, QWidget * aParent) :
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::DlgHistory),
	mTicksUntilSetSearchText(0)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgHistory", *this);
	auto model = new HistoryModel(*(aComponents.get<Database>()));
	auto modelFilter = new HistoryModelFilter(this);
	modelFilter->setSourceModel(model);
	mModelFilter = modelFilter;
	mUI->tblHistory->setModel(mModelFilter);

	// Create the context menu:
	auto tbl = mUI->tblHistory;
	mContextMenu.reset(new QMenu());
	mContextMenu->addAction(mUI->actAppendToPlaylist);
	mContextMenu->addAction(mUI->actInsertIntoPlaylist);
	mContextMenu->addSeparator();
	mContextMenu->addAction(mUI->actProperties);
	tbl->addActions({
		mUI->actAppendToPlaylist,
		mUI->actInsertIntoPlaylist,
		mUI->actProperties
	});

	// Stretch the columns to fit in the headers / data:
	QFontMetrics fmHdr(mUI->tblHistory->horizontalHeader()->font());
	QFontMetrics fmItems(mUI->tblHistory->font());
	mUI->tblHistory->setColumnWidth(0, fmItems.width("W2088-08-08T00:00:00.000W"));
	mUI->tblHistory->setColumnWidth(1, fmHdr.width("WGenreW"));
	Settings::loadHeaderView("DlgHistory", "tblHistory", *mUI->tblHistory->horizontalHeader());

	// Connect the signals / slots:
	connect(mUI->btnClose,              &QPushButton::clicked,                this, &DlgHistory::close);
	connect(mUI->btnExport,             &QPushButton::clicked,                this, &DlgHistory::exportToFile);
	connect(tbl,                         &QWidget::customContextMenuRequested, this, &DlgHistory::showHistoryContextMenu);
	connect(mUI->actAppendToPlaylist,   &QAction::triggered,                  this, &DlgHistory::appendToPlaylist);
	connect(mUI->actInsertIntoPlaylist, &QAction::triggered,                  this, &DlgHistory::insertIntoPlaylist);
	connect(mUI->actProperties,         &QAction::triggered,                  this, &DlgHistory::showProperties);
	connect(mUI->leSearch,              &QLineEdit::textEdited,               this, &DlgHistory::searchTextEdited);
	connect(&mPeriodicUiUpdate,         &QTimer::timeout,                     this, &DlgHistory::periodicUiUpdate);

	// Allow the window to be maximized:
	setWindowFlags(Qt::Window);

	mPeriodicUiUpdate.start(100);
}





DlgHistory::~DlgHistory()
{
	Settings::saveHeaderView("DlgHistory", "tblHistory", *mUI->tblHistory->horizontalHeader());
	Settings::saveWindowPos("DlgHistory", *this);
}





SongPtr DlgHistory::songFromIndex(const QModelIndex & aIndex)
{
	auto sd = mUI->tblHistory->model()->data(aIndex, HistoryModel::roleSharedData).value<Song::SharedDataPtr>();
	if ((sd == nullptr) || sd->duplicates().empty())
	{
		return nullptr;
	}
	return sd->duplicates()[0]->shared_from_this();
}





void DlgHistory::showHistoryContextMenu(const QPoint & aPos)
{
	// Update the actions based on the selection:
	const auto & sel = mUI->tblHistory->selectionModel()->selectedRows();
	mUI->actAppendToPlaylist->setEnabled(!sel.isEmpty());
	mUI->actInsertIntoPlaylist->setEnabled(!sel.isEmpty());
	mUI->actProperties->setEnabled(sel.count() == 1);

	// Show the context menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? aPos : widget->mapToGlobal(aPos);
	mContextMenu->exec(pos, nullptr);
}





void DlgHistory::appendToPlaylist()
{
	foreach(const auto & idx, mUI->tblHistory->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		if (song != nullptr)
		{
			emit appendSongToPlaylist(song);
		}
	}
}





void DlgHistory::insertIntoPlaylist()
{
	// Collect all the songs:
	std::vector<SongPtr> songs;
	foreach(const auto & idx, mUI->tblHistory->selectionModel()->selectedRows())
	{
		auto song = songFromIndex(idx);
		if (song != nullptr)
		{
			songs.push_back(song);
		}
	}

	// Emit the signal, reverse the songs:
	for (auto itr = songs.crbegin(), end = songs.crend(); itr != end; ++itr)
	{
		emit insertSongToPlaylist(*itr);
	}
}





void DlgHistory::showProperties()
{
	auto song = songFromIndex(mUI->tblHistory->currentIndex());
	if (song != nullptr)
	{
		DlgSongProperties dlg(mComponents, song, this);
		dlg.exec();
	}
}





void DlgHistory::exportToFile()
{
	// Get the file name:
	auto fileName = QFileDialog::getSaveFileName(
		this,
		tr("SkauTan: Export selected history"),
		QString(),
		tr("CSV file (*.csv)")
	);
	if (fileName.isEmpty())
	{
		return;
	}

	// Open the file:
	QFile f(fileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(
			this,
			tr("SkauTan: Cannot write file"),
			tr("Cannot write to file %1, export aborted.").arg(fileName)
		);
		return;
	}

	// Export:
	auto tbl = mUI->tblHistory;
	auto model = tbl->model();
	if (tbl->selectionModel()->selection().isEmpty())
	{
		// Empty selection -> export everything:
		auto numRows = model->rowCount();
		for (int row = 0; row < numRows; ++row)
		{
			for (int col = 0; col < HistoryModel::colMax; ++col)
			{
				f.write("\"");
				f.write(model->data(model->index(row, col)).toString().replace("\"", "\\\"").toUtf8());
				f.write("\",");
			}
			f.write("\r\n");
		}
	}
	else
	{
		// Export selection only:
		foreach(const auto & idx, tbl->selectionModel()->selectedRows())
		{
			for (int col = 0; col < HistoryModel::colMax; ++col)
			{
				f.write("\"");
				f.write(model->data(model->index(idx.row(), col)).toString().replace("\"", "\\\"").toUtf8());
				f.write("\",");
			}
			f.write("\r\n");
		}
	}
	f.close();
	QMessageBox::information(
		this,
		tr("SkauTan: Export finished"),
		tr("The history was exported successfully.")
	);
}





void DlgHistory::periodicUiUpdate()
{
	// Update the search filter, if appropriate:
	if (mTicksUntilSetSearchText > 0)
	{
		mTicksUntilSetSearchText -= 1;
		if (mTicksUntilSetSearchText == 0)
		{
			reinterpret_cast<HistoryModelFilter *>(mModelFilter)->setSearchText(mNewSearchText);
		}
	}
}





void DlgHistory::searchTextEdited(const QString & aNewText)
{
	mNewSearchText = aNewText;
	mTicksUntilSetSearchText = TICKS_UNTIL_SET_SEARCH_TEXT;
}

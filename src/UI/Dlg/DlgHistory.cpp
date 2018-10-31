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
static QString formatMPM(const DatedOptional<double> & a_SongMPM)
{
	if (!a_SongMPM.isPresent())
	{
		return {};
	}
	return QLocale().toString(a_SongMPM.value(), 'f', 1);
}





/** Returns the string representation of the date (and time). */
static QString formatDate(const QDateTime a_DateTime)
{
	return a_DateTime.toString("yyyy-MM-dd HH:mm:ss");
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


	HistoryModel(Database & a_DB):
		m_DB(a_DB),
		m_SongSharedDataMap(a_DB.songSharedDataMap()),
		m_Items(a_DB.playbackHistory())
	{
		// Sort the history by datetime, most recent at the top:
		std::sort(m_Items.begin(), m_Items.end(),
			[](auto a_Item1, auto a_Item2)
			{
				return (a_Item1.m_Timestamp > a_Item2.m_Timestamp);
			}
		);
	}





	virtual int rowCount(const QModelIndex & a_Parent) const override
	{
		if (a_Parent.isValid())
		{
			return 0;
		}
		return static_cast<int>(m_Items.size());
	}





	virtual int columnCount(const QModelIndex & a_Parent) const override
	{
		if (a_Parent.isValid())
		{
			return 0;
		}
		return colMax;
	}





	virtual QVariant data(const QModelIndex & a_Index, int a_Role) const override
	{
		if (!a_Index.isValid())
		{
			return QVariant();
		}

		auto row = a_Index.row();
		if ((row < 0) || (static_cast<size_t>(row) > m_Items.size()))
		{
			return QVariant();
		}
		const auto & item = m_Items[static_cast<size_t>(row)];

		// If requesting the pointer to the shared data, return it:
		if (a_Role == roleSharedData)
		{
			const auto sdItr = m_SongSharedDataMap.find(item.m_Hash);
			if (sdItr == m_SongSharedDataMap.end())
			{
				return QVariant();
			}
			return QVariant::fromValue(sdItr->second);
		}

		// Only process display data from now on:
		if (a_Role != Qt::DisplayRole)
		{
			return QVariant();
		}

		const auto col = a_Index.column();
		if (col == colDate)  // Special case - doesn't need hash->song lookup
		{
			return formatDate(item.m_Timestamp);
		}
		const auto sdItr = m_SongSharedDataMap.find(item.m_Hash);
		if (sdItr == m_SongSharedDataMap.end())
		{
			// Song hash is not in the DB at all
			return QVariant();
		}

		// If the song hash is in the DB, but no actual file is present, use the manual tag only:
		if (sdItr->second->duplicates().empty())
		{
			switch (col)
			{
				case colGenre:  return sdItr->second->m_TagManual.m_Genre.valueOrDefault();
				case colMPM:    return formatMPM(sdItr->second->m_TagManual.m_MeasuresPerMinute);
				case colAuthor: return sdItr->second->m_TagManual.m_Author.valueOrDefault();
				case colTitle:  return sdItr->second->m_TagManual.m_Title.valueOrDefault();
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




	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override
	{
		if ((a_Orientation != Qt::Horizontal) || (a_Role != Qt::DisplayRole))
		{
			return QVariant();
		}
		switch (a_Section)
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
	Database & m_DB;

	/** SharedData map from the current DB for resolving hashes into song details. */
	using SongSharedDataMap = decltype(m_DB.songSharedDataMap());
	SongSharedDataMap m_SongSharedDataMap;

	/** The entire playback history, as read from the DB. */
	std::vector<Database::HistoryItem> m_Items;
};





class HistoryModelFilter:
	public QSortFilterProxyModel
{
	using Super = QSortFilterProxyModel;

public:

	HistoryModelFilter(QObject * a_Parent):
		Super(a_Parent)
	{
	}


	/** Sets the text to filter by.
	Only items containing the specified text are shown in the model.
	If the text is empty, all items are shown. */
	void setSearchText(const QString & a_SearchText)
	{
		m_SearchText = a_SearchText;
		invalidateFilter();
	}


protected:

	/** The text to be searched within the items. */
	QString m_SearchText;


	// QSortFilterProxyModel overrides:
	virtual bool filterAcceptsRow(int a_SrcRow, const QModelIndex & a_SrcParent) const override
	{
		// Empty search text matches everything:
		if (m_SearchText.isEmpty())
		{
			return true;
		}

		// Search for the text in the whole row:
		auto numCols = sourceModel()->columnCount(a_SrcParent);
		for (int col = 0; col < numCols; ++col)
		{
			auto txt = sourceModel()->data(sourceModel()->index(a_SrcRow, col, a_SrcParent)).toString();
			if (txt.contains(m_SearchText, Qt::CaseInsensitive))
			{
				return true;
			}
		}
		return false;
	}
};





////////////////////////////////////////////////////////////////////////////////
// DlgHistory:

DlgHistory::DlgHistory(ComponentCollection & a_Components, QWidget * a_Parent) :
	Super(a_Parent),
	m_Components(a_Components),
	m_UI(new Ui::DlgHistory),
	m_TicksUntilSetSearchText(0)
{
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgHistory", *this);
	auto model = new HistoryModel(*(a_Components.get<Database>()));
	auto modelFilter = new HistoryModelFilter(this);
	modelFilter->setSourceModel(model);
	m_ModelFilter = modelFilter;
	m_UI->tblHistory->setModel(m_ModelFilter);

	// Create the context menu:
	auto tbl = m_UI->tblHistory;
	m_ContextMenu.reset(new QMenu());
	m_ContextMenu->addAction(m_UI->actAppendToPlaylist);
	m_ContextMenu->addAction(m_UI->actInsertIntoPlaylist);
	m_ContextMenu->addSeparator();
	m_ContextMenu->addAction(m_UI->actProperties);
	tbl->addActions({
		m_UI->actAppendToPlaylist,
		m_UI->actInsertIntoPlaylist,
		m_UI->actProperties
	});

	// Stretch the columns to fit in the headers / data:
	QFontMetrics fmHdr(m_UI->tblHistory->horizontalHeader()->font());
	QFontMetrics fmItems(m_UI->tblHistory->font());
	m_UI->tblHistory->setColumnWidth(0, fmItems.width("W2088-08-08T00:00:00.000W"));
	m_UI->tblHistory->setColumnWidth(1, fmHdr.width("WGenreW"));
	Settings::loadHeaderView("DlgHistory", "tblHistory", *m_UI->tblHistory->horizontalHeader());

	// Connect the signals / slots:
	connect(m_UI->btnClose,              &QPushButton::clicked,                this, &DlgHistory::close);
	connect(m_UI->btnExport,             &QPushButton::clicked,                this, &DlgHistory::exportToFile);
	connect(tbl,                         &QWidget::customContextMenuRequested, this, &DlgHistory::showHistoryContextMenu);
	connect(m_UI->actAppendToPlaylist,   &QAction::triggered,                  this, &DlgHistory::appendToPlaylist);
	connect(m_UI->actInsertIntoPlaylist, &QAction::triggered,                  this, &DlgHistory::insertIntoPlaylist);
	connect(m_UI->actProperties,         &QAction::triggered,                  this, &DlgHistory::showProperties);
	connect(m_UI->leSearch,              &QLineEdit::textEdited,               this, &DlgHistory::searchTextEdited);
	connect(&m_PeriodicUiUpdate,         &QTimer::timeout,                     this, &DlgHistory::periodicUiUpdate);

	// Allow the window to be maximized:
	setWindowFlags(Qt::Window);

	m_PeriodicUiUpdate.start(100);
}





DlgHistory::~DlgHistory()
{
	Settings::saveHeaderView("DlgHistory", "tblHistory", *m_UI->tblHistory->horizontalHeader());
	Settings::saveWindowPos("DlgHistory", *this);
}





SongPtr DlgHistory::songFromIndex(const QModelIndex & a_Index)
{
	auto sd = m_UI->tblHistory->model()->data(a_Index, HistoryModel::roleSharedData).value<Song::SharedDataPtr>();
	if ((sd == nullptr) || sd->duplicates().empty())
	{
		return nullptr;
	}
	return sd->duplicates()[0]->shared_from_this();
}





void DlgHistory::showHistoryContextMenu(const QPoint & a_Pos)
{
	// Update the actions based on the selection:
	const auto & sel = m_UI->tblHistory->selectionModel()->selectedRows();
	m_UI->actAppendToPlaylist->setEnabled(!sel.isEmpty());
	m_UI->actInsertIntoPlaylist->setEnabled(!sel.isEmpty());
	m_UI->actProperties->setEnabled(sel.count() == 1);

	// Show the context menu:
	auto widget = dynamic_cast<QWidget *>(sender());
	auto pos = (widget == nullptr) ? a_Pos : widget->mapToGlobal(a_Pos);
	m_ContextMenu->exec(pos, nullptr);
}





void DlgHistory::appendToPlaylist()
{
	foreach(const auto & idx, m_UI->tblHistory->selectionModel()->selectedRows())
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
	foreach(const auto & idx, m_UI->tblHistory->selectionModel()->selectedRows())
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
	auto song = songFromIndex(m_UI->tblHistory->currentIndex());
	if (song != nullptr)
	{
		DlgSongProperties dlg(m_Components, song, this);
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
	auto tbl = m_UI->tblHistory;
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
	if (m_TicksUntilSetSearchText > 0)
	{
		m_TicksUntilSetSearchText -= 1;
		if (m_TicksUntilSetSearchText == 0)
		{
			reinterpret_cast<HistoryModelFilter *>(m_ModelFilter)->setSearchText(m_NewSearchText);
		}
	}
}





void DlgHistory::searchTextEdited(const QString & a_NewText)
{
	m_NewSearchText = a_NewText;
	m_TicksUntilSetSearchText = TICKS_UNTIL_SET_SEARCH_TEXT;
}

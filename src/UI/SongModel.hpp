#pragma once

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include "../Song.hpp"
#include "../Template.hpp"





// fwd:
class Database;





/** Provides a Qt model-view architecture's Model implementation for displaying all songs in a SongDatabase. */
class SongModel:
	public QAbstractTableModel
{
	Q_OBJECT
	using Super = QAbstractTableModel;


public:

	/** Symbolic names for the individual columns. */
	enum EColumn
	{
		colLocalRating,
		colRating,
		colManualAuthor,
		colManualTitle,
		colManualGenre,
		colManualMeasuresPerMinute,
		colLength,
		colLastPlayed,
		colId3Author,
		colId3Title,
		colId3Genre,
		colId3MeasuresPerMinute,
		colFileNameAuthor,
		colFileNameTitle,
		colFileNameGenre,
		colFileNameMeasuresPerMinute,
		colDetectedTempo,
		colNumMatchingFilters,
		colNumDuplicates,
		colSkipStart,
		colFileName,

		colMax,
	};

	/** The data role that returns the song pointer for the specified QModelIndex, when queried by data().
	Used by filter models to get back to the song represented by the row. */
	static const int roleSongPtr = Qt::UserRole + 1;


	SongModel(Database & aDB);

	/** Returns the song represented by the specified model index. */
	SongPtr songFromIndex(const QModelIndex & aIdx) const;

	/** Returns the song represented by the specified row. */
	SongPtr songFromRow(int aRow) const;

	/** Returns the QModelIndex representing the specified song, pointing to the specified column.
	Asserts and returns an invalid QModelIndex if song not found. */
	QModelIndex indexFromSong(const Song * aSong, int aColumn = 0);


protected:

	/** The DB on which the model is based. */
	Database & mDB;


	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & aParent) const override;
	virtual int columnCount(const QModelIndex & aParent) const override;
	virtual QVariant data(const QModelIndex & aIndex, int aRole) const override;
	virtual QVariant headerData(int aSection, Qt::Orientation aOrientation, int aRole) const override;
	virtual Qt::ItemFlags flags(const QModelIndex & aIndex) const override;
	virtual bool setData(const QModelIndex & aIndex, const QVariant & aValue, int aRole) override;

	/** Returns the number of filters that the specified song matches. */
	qulonglong numMatchingFilters(SongPtr aSong) const;

	/** Returns the number of duplicates of the specified song (through its SharedData).
	If the song has no SharedData yet, returns 1. */
	qulonglong numDuplicates(SongPtr aSong) const;


protected slots:

	/** Emitted by mDB when a new song is added to it. */
	void addSongFile(SongPtr aNewSong);

	/** Emitted by mDB just before a song is removed; remove it from the model. */
	void delSong(SongPtr aSong, size_t aIndex);

	/** Emitted by mDB after a song data has changed.
	Updates the model entries for the specified song. */
	void songChanged(SongPtr aSong);


signals:

	/** Emitted after the specified song has been edited by the user. */
	void songEdited(SongPtr aSong);
};





/** A QItemDelegate descendant that provides specialized editors for SongModel-based views:
- MPM gets a QLineEdit with a QDoubleValidator
- Genre gets a QComboBox pre-filled with genres.
*/
class SongModelEditorDelegate:
	public QStyledItemDelegate
{
	using Super = QStyledItemDelegate;
	Q_OBJECT


public:

	SongModelEditorDelegate(QWidget * aParent = nullptr);

	// QStyledItemDelegate overrides:
	virtual QWidget * createEditor(
		QWidget * aParent,
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) const override;

	virtual void setEditorData(
		QWidget * aEditor,
		const QModelIndex & aIndex
	) const override;

	virtual void setModelData(
		QWidget * aEditor,
		QAbstractItemModel * aModel,
		const QModelIndex & aIndex
	) const override;


private slots:

	void commitAndCloseEditor();
};





class SongModelFilter:
	public QSortFilterProxyModel
{
	using Super = QSortFilterProxyModel;


public:

	/** Special filters that can be applied to songs. */
	enum EFilter
	{
		fltNone,                   ///< No filtering
		fltNoId3,                  ///< Songs with an empty / missing ID3 tag
		fltNoGenre,                ///< Songs with an empty / missing primary genre
		fltNoMeasuresPerMinute,    ///< Songs missing their MPM
		fltWarnings,               ///< Songs with warnings
		fltNoFilterMatch,          ///< Songs matching no Filter
		fltDuplicates,             ///< Songs that have duplicates
		fltSkipStart,              ///< Songs that have a skip-start defined
		fltFailedTempoDetect,      ///< Songs for which the tempo detection produced different tempo than stored in ID3 / Manual tag
	};


	SongModelFilter(SongModel & aParentModel);

	/** Sets the filter to apply on the songs. */
	void setFilter(EFilter aFilter);

	/** Sets the search string to be used for filtering. */
	void setSearchString(const QString & aSearchString);

	/** Sets the Filters that are considered for fltNoFilterMatch filtering. */
	void setFavoriteFilters(const std::vector<FilterPtr> & aFavoriteFilters);


protected:

	/** The model that provides all songs. */
	SongModel & mParentModel;

	/** The special filter to apply. */
	EFilter mFilter;

	/** The string to search for when filtering. */
	QRegularExpression mSearchString;

	/** The template items that are considered for fltNoFilterMatch. */
	std::vector<FilterPtr> mFavoriteFilters;


	// QSortFilterProxyModel overrides:
	virtual bool filterAcceptsRow(int aSrcRow, const QModelIndex & aSrcParent) const override;

	/** Returns true if the specified row matches the currently set search string. */
	bool songMatchesSearchString(SongPtr aSong) const;
};

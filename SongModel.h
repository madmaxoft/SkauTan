#ifndef SONGMODEL_H
#define SONGMODEL_H




#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include "Song.h"
#include "Template.h"





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
		colNumMatchingFilters,
		colFileName,

		colMax,
	};


	SongModel(Database & a_DB);

	/** Returns the song represented by the specified model index. */
	SongPtr songFromIndex(const QModelIndex & a_Idx) const;

	/** Returns the song represented by the specified row. */
	SongPtr songFromRow(int a_Row) const;

	/** Returns the QModelIndex representing the specified song, pointing to the specified column.
	Asserts and returns an invalid QModelIndex if song not found. */
	QModelIndex indexFromSong(const Song * a_Song, int a_Column = 0);


protected:

	/** The DB on which the model is based. */
	Database & m_DB;


	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & a_Parent) const override;
	virtual int columnCount(const QModelIndex & a_Parent) const override;
	virtual QVariant data(const QModelIndex & a_Index, int a_Role) const override;
	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex & a_Index) const override;
	virtual bool setData(const QModelIndex & a_Index, const QVariant & a_Value, int a_Role) override;

	/** Returns the number of filters that the specified song matches. */
	qulonglong numMatchingFilters(SongPtr a_Song) const;


protected slots:

	/** Emitted by m_DB when a new song is added to it. */
	void addSongFile(SongPtr a_NewSong);

	/** Emitted by m_DB just before a song is removed; remove it from the model. */
	void delSong(const Song * a_Song, size_t a_Index);

	/** Emitted by m_DB after a song data has changed.
	Updates the model entries for the specified song. */
	void songChanged(SongPtr a_Song);


signals:

	/** Emitted after the specified song has been edited by the user. */
	void songEdited(SongPtr a_Song);
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

	SongModelEditorDelegate(QWidget * a_Parent = nullptr);

	// QStyledItemDelegate overrides:
	virtual QWidget * createEditor(
		QWidget * a_Parent,
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) const override;

	virtual void setEditorData(
		QWidget * a_Editor,
		const QModelIndex & a_Index
	) const override;

	virtual void setModelData(
		QWidget * a_Editor,
		QAbstractItemModel * a_Model,
		const QModelIndex & a_Index
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
		fltNone,                   //< No filtering
		fltNoId3,                  //< Songs with an empty / missing ID3 tag
		fltNoGenre,                //< Songs with an empty / missing primary genre
		fltNoMeasuresPerMinute,    //< Songs missing their MPM
		fltWarnings,               //< Songs with warnings
		fltNoTemplateFilterMatch,  //< Songs matching no Template filter
	};


	SongModelFilter(SongModel & a_ParentModel);

	/** Sets the filter to apply on the songs. */
	void setFilter(EFilter a_Filter);

	/** Sets the search string to be used for filtering. */
	void setSearchString(const QString & a_SearchString);

	/** Sets the template items that are considered for fltNoTemplateFilter filtering. */
	void setFavoriteTemplateItems(const std::vector<Template::ItemPtr> & a_FavoriteTemplateItems);


protected:

	/** The model that provides all songs. */
	SongModel & m_ParentModel;

	/** The special filter to apply. */
	EFilter m_Filter;

	/** The string to search for when filtering. */
	QRegularExpression m_SearchString;

	/** The template items that are considered for fltNoTemplateFilter. */
	std::vector<Template::ItemPtr> m_FavoriteTemplateItems;


	// QSortFilterProxyModel overrides:
	virtual bool filterAcceptsRow(int a_SrcRow, const QModelIndex & a_SrcParent) const override;

	/** Returns true if the specified row matches the currently set search string. */
	bool songMatchesSearchString(SongPtr a_Song) const;
};





#endif // SONGMODEL_H

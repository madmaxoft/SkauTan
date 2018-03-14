#ifndef SONGMODEL_H
#define SONGMODEL_H




#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include "Song.h"





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
		colRating,
		colAuthor,
		colTitle,
		colGenre,
		colMeasuresPerMinute,
		colLength,
		colLastPlayed,
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


protected slots:

	/** Emitted by m_DB when a new song is added to it. */
	void addSongFile(Song * a_NewSong);

	/** Emitted by m_DB just before a song is removed; remove it from the model. */
	void delSong(const Song * a_Song, size_t a_Index);

	/** Emitted by m_DB.metadataScanner after a song metadata is updated by a scan. */
	void songMetadataScanned(Song * a_Song);


signals:

	/** Emitted after the specified song has been edited by the user. */
	void songEdited(Song * a_Song);
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





#endif // SONGMODEL_H

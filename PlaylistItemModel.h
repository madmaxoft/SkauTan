#ifndef PLAYLISTITEMMODEL_H
#define PLAYLISTITEMMODEL_H




#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include "Playlist.h"





/** Provides the data for Qt widgets based on a playlist. */
class PlaylistItemModel:
	public QAbstractTableModel
{
	using Super = QAbstractTableModel;
	Q_OBJECT

public:
	enum EColumn
	{
		colLength,
		colDurationLimit,
		colGenre,
		colMeasuresPerMinute,
		colReplace,
		colAuthor,
		colTitle,
		colDisplayName,

		colMax,
	};

	PlaylistItemModel(Playlist & a_Playlist);

protected:

	/** The playlist on which the model is based. */
	Playlist & m_Playlist;

	/** The item that is highlighted as the current one. */
	int m_CurrentItemIdx;


	// QAbstractTableModel overrides:
	virtual Qt::ItemFlags flags(const QModelIndex & a_Index) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual bool dropMimeData(const QMimeData * a_Data, Qt::DropAction a_Action, int a_Row, int a_Column, const QModelIndex & a_Parent) override;
	virtual QMimeData * mimeData(const QModelIndexList & a_Indexes) const override;
	virtual QStringList mimeTypes() const override;
	virtual int rowCount(const QModelIndex & a_Parent) const override;
	virtual int columnCount(const QModelIndex & a_Parent) const override;
	virtual QVariant data(const QModelIndex & a_Index, int a_Role) const override;
	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override;


protected slots:

	void playlistItemAdded(IPlaylistItem * a_Item);
	void playlistItemDeleting(IPlaylistItem * a_Item, int a_Index);
	void playlistItemReplaced(int a_Index, IPlaylistItem * a_NewItem);
	void playlistItemInserted(int a_Index, IPlaylistItem * a_NewItem);

	/** Emitted by m_Playlist when its index of the current item changes. */
	void playlistCurrentChanged(int a_CurrentItemIdx);
};





/** Provides special drawing and handling for the "Replace" column. */
class PlaylistItemDelegate:
	public QStyledItemDelegate
{
	using Super = QStyledItemDelegate;
	Q_OBJECT

public:

	// QStyledItemDelegate overrides:
	virtual void paint(
		QPainter * a_Painter,
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) const override;
	virtual QSize sizeHint(
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) const override;
	virtual bool editorEvent(
		QEvent * a_Event,
		QAbstractItemModel * a_Model,
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) override;


signals:

	/** Emitted when the user clicks the Replace button. */
	void replaceSong(const QModelIndex & a_Index);
};





#endif // PLAYLISTITEMMODEL_H

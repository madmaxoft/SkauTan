#ifndef PLAYLISTITEMMODEL_H
#define PLAYLISTITEMMODEL_H




#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include "../Playlist.hpp"





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
		colTimeStart,
		colTimeEnd,

		colMax,
	};

	PlaylistItemModel(Playlist & aPlaylist);

	/** Called when a track was modified externally and the model should update. */
	void trackWasModified(const IPlaylistItem & aItem);


protected:

	/** The playlist on which the model is based. */
	Playlist & mPlaylist;

	/** The item that is highlighted as the current one. */
	int mCurrentItemIdx;


	// QAbstractTableModel overrides:
	virtual Qt::ItemFlags flags(const QModelIndex & aIndex) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual bool dropMimeData(const QMimeData * aData, Qt::DropAction aAction, int aRow, int aColumn, const QModelIndex & aParent) override;
	virtual QMimeData * mimeData(const QModelIndexList & aIndexes) const override;
	virtual QStringList mimeTypes() const override;
	virtual int rowCount(const QModelIndex & aParent) const override;
	virtual int columnCount(const QModelIndex & aParent) const override;
	virtual QVariant data(const QModelIndex & aIndex, int aRole) const override;
	virtual QVariant headerData(int aSection, Qt::Orientation aOrientation, int aRole) const override;


protected slots:

	void playlistItemAdded(IPlaylistItem * aItem);
	void playlistItemDeleting(IPlaylistItem * aItem, int aIndex);
	void playlistItemReplaced(int aIndex, IPlaylistItem * aNewItem);
	void playlistItemInserted(int aIndex, IPlaylistItem * aNewItem);

	/** Emitted by mPlaylist when its index of the current item changes. */
	void playlistCurrentChanged(int aCurrentItemIdx);

	/** Emitted by mPlaylist when an item's start or end time changed. */
	void playlistItemTimesChanged(int aItemIdx, IPlaylistItem * aItem);
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
		QPainter * aPainter,
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) const override;
	virtual QSize sizeHint(
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) const override;
	virtual bool editorEvent(
		QEvent * aEvent,
		QAbstractItemModel * aModel,
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) override;


signals:

	/** Emitted when the user clicks the Replace button. */
	void replaceSong(const QModelIndex & aIndex);
};





#endif // PLAYLISTITEMMODEL_H

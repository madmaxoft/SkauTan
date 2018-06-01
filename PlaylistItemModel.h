#ifndef PLAYLISTITEMMODEL_H
#define PLAYLISTITEMMODEL_H




#include <QAbstractTableModel>
#include "Playlist.h"





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

	/** Emitted by m_Playlist when its index of the current item changes. */
	void playlistCurrentChanged(int a_CurrentItemIdx);
};





#endif // PLAYLISTITEMMODEL_H

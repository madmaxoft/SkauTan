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
		colGenre,
		colMeasuresPerMinute,
		colAuthor,
		colTitle,
		colDisplayName,

		colMax,
	};

	PlaylistItemModel(PlaylistPtr a_Playlist);

protected:

	/** The playlist on which the model is based. */
	PlaylistPtr m_Playlist;


	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & a_Parent) const override;
	virtual int columnCount(const QModelIndex & a_Parent) const override;
	virtual QVariant data(const QModelIndex & a_Index, int a_Role) const override;
	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override;


protected slots:

	void playlistItemAdded(IPlaylistItem * a_Item);
};





#endif // PLAYLISTITEMMODEL_H

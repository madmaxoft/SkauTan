#include "PlaylistItemModel.h"





static QString formatLength(double a_Length)
{
	if (a_Length < 0)
	{
		return PlaylistItemModel::tr("<unknown>", "Length");
	}
	auto len = static_cast<int>(floor(a_Length + 0.5));
	return QString("%1:%2").arg(len / 60).arg(len % 60);
}





static QString formatTempo(double a_Tempo)
{
	if (a_Tempo < 0)
	{
		return PlaylistItemModel::tr("<unknown>", "Tempo");
	}
	return QString::number(a_Tempo, 'f', 1);
}





////////////////////////////////////////////////////////////////////////////////
// PlaylistItemModel:

PlaylistItemModel::PlaylistItemModel(PlaylistPtr a_Playlist):
	m_Playlist(a_Playlist)
{
	connect(m_Playlist.get(), &Playlist::itemAdded, this, &PlaylistItemModel::playlistItemAdded);
}





int PlaylistItemModel::rowCount(const QModelIndex & a_Parent) const
{
	if (!a_Parent.isValid())
	{
		return static_cast<int>(m_Playlist->items().size());
	}
	return 0;
}





int PlaylistItemModel::columnCount(const QModelIndex & a_Parent) const
{
	Q_UNUSED(a_Parent);

	return colMax;
}





QVariant PlaylistItemModel::data(const QModelIndex & a_Index, int a_Role) const
{
	if (!a_Index.isValid())
	{
		return QVariant();
	}
	switch (a_Role)
	{
		case Qt::DisplayRole:
		{
			if ((a_Index.row() < 0) || (a_Index.row() >= static_cast<int>(m_Playlist->items().size())))
			{
				return QVariant();
			}
			const auto & item = m_Playlist->items()[a_Index.row()];
			switch (a_Index.column())
			{
				case colGenre:             return item->displayGenre();
				case colLength:            return formatLength(item->displayLength());
				case colMeasuresPerMinute: return formatTempo(item->displayTempo());
				case colAuthor:            return item->displayAuthor();
				case colTitle:             return item->displayTitle();
				case colDisplayName:       return item->displayName();
			}
			return QVariant();
		}  // case Qt::DisplayRole

		case Qt::DecorationRole:
		{
			// TODO: Currently-playing item decoration
		}
	}
	// TODO
	return QVariant();
}





QVariant PlaylistItemModel::headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const
{
	if ((a_Orientation != Qt::Horizontal) || (a_Role != Qt::DisplayRole))
	{
		return QVariant();
	}
	switch (a_Section)
	{
		case colGenre:             return tr("Genre");
		case colLength:            return tr("Length");
		case colMeasuresPerMinute: return tr("MPM");
		case colAuthor:            return tr("Author");
		case colTitle:             return tr("Title");
		case colDisplayName:       return tr("Name");
	}
	return QVariant();
}





void PlaylistItemModel::playlistItemAdded(IPlaylistItem * a_Item)
{
	Q_UNUSED(a_Item);

	auto idx = static_cast<int>(m_Playlist->items().size()) - 1;
	beginInsertRows(QModelIndex(), idx, idx + 1);
	endInsertRows();
}

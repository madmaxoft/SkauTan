#include "PlaylistItemModel.h"
#include <QMimeData>
#include <QDebug>





#define MIME_TYPE "application/x-playlist-row"





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
	connect(m_Playlist.get(), &Playlist::itemAdded,    this, &PlaylistItemModel::playlistItemAdded);
	connect(m_Playlist.get(), &Playlist::itemDeleting, this, &PlaylistItemModel::playlistItemDeleting);
}





Qt::ItemFlags PlaylistItemModel::flags(const QModelIndex & a_Index) const
{
	if (a_Index.isValid())
	{
		return Super::flags(a_Index) | Qt::ItemIsDragEnabled;
	}
	else
	{
		return Qt::ItemIsDropEnabled;
	}
}





Qt::DropActions PlaylistItemModel::supportedDropActions() const
{
	return Qt::MoveAction;
}





bool PlaylistItemModel::dropMimeData(const QMimeData * a_Data, Qt::DropAction a_Action, int a_Row, int a_Column, const QModelIndex & a_Parent)
{
	Q_UNUSED(a_Parent);

	if (!a_Data->hasFormat(MIME_TYPE))
	{
		return false;
	}
	if (a_Action != Qt::MoveAction)
	{
		return true;
	}
	if (a_Row < 0)
	{
		qDebug() << "dropMimeData: invalid destination: row " << a_Row << ", column " << a_Column << ", parent: " << a_Parent;
		return true;
	}

	QByteArray encodedData = a_Data->data(MIME_TYPE);
	std::vector<int> rows;
	auto numRows = static_cast<size_t>(encodedData.size() / 4);
	for (size_t i = 0; i < numRows; ++i)
	{
		int row = 0;
		memcpy(&row, encodedData.data() + i * 4, 4);
		rows.push_back(row);
	}
	auto minRow = a_Row;
	auto maxRow = a_Row;
	while (!rows.empty())
	{
		auto row = rows.back();
		m_Playlist->moveItem(static_cast<size_t>(row), static_cast<size_t>(a_Row));
		if (minRow > row)
		{
			minRow = row;
		}
		if (maxRow < row)
		{
			maxRow = row;
		}
		rows.pop_back();
		if (row < a_Row)
		{
			a_Row -= 1;  // Moving an item down the list means the destination index moved as well.
		}
	}
	emit dataChanged(createIndex(minRow, 0), createIndex(maxRow, colMax - 1));
	return true;
}





QMimeData * PlaylistItemModel::mimeData(const QModelIndexList & a_Indexes) const
{
	QMimeData * mimeData = new QMimeData();
	QByteArray encodedData;
	foreach (QModelIndex index, a_Indexes)
	{
		if (index.isValid() && (index.column() == 0))
		{
			int row = index.row();
			qDebug() << "Requesting mime data for row " << row;
			encodedData.append(reinterpret_cast<const char *>(&row), 4);
		}
	}
	mimeData->setData(MIME_TYPE, encodedData);
	return mimeData;
}





QStringList PlaylistItemModel::mimeTypes() const
{
	return QStringList({MIME_TYPE});
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
			const auto & item = m_Playlist->items()[static_cast<size_t>(a_Index.row())];
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
	beginInsertRows(QModelIndex(), idx, idx);
	endInsertRows();
}





void PlaylistItemModel::playlistItemDeleting(IPlaylistItem * a_Item, int a_Index)
{
	Q_UNUSED(a_Item);
	beginRemoveRows(QModelIndex(), a_Index, a_Index);
	endRemoveRows();
}

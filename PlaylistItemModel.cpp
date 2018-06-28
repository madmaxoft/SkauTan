#include "PlaylistItemModel.h"
#include <QMimeData>
#include <QDebug>
#include <QIcon>
#include <QFont>
#include <QApplication>
#include <QPainter>
#include <cmath>
#include "Utils.h"
#include "PlaylistItemSong.h"





#define MIME_TYPE "application/x-playlist-row"





static QString formatLength(double a_Length)
{
	if (a_Length < 0)
	{
		return PlaylistItemModel::tr("<unknown>", "Length");
	}
	auto len = static_cast<int>(std::round(a_Length));
	return QString("%1:%2").arg(len / 60).arg(QString::number(len % 60), 2, '0');
}





static QString formatTempo(double a_Tempo)
{
	if (a_Tempo < 0)
	{
		return PlaylistItemModel::tr("<unknown>", "Tempo");
	}
	return QString::number(a_Tempo, 'f', 1);
}





static QString formatDurationLimit(double a_DurationLimit)
{
	if (a_DurationLimit < 0)
	{
		return QString();
	}
	return Utils::formatTime(a_DurationLimit);
}





////////////////////////////////////////////////////////////////////////////////
// PlaylistItemModel:

PlaylistItemModel::PlaylistItemModel(Playlist & a_Playlist):
	m_Playlist(a_Playlist)
{
	connect(&m_Playlist, &Playlist::itemAdded,          this, &PlaylistItemModel::playlistItemAdded);
	connect(&m_Playlist, &Playlist::itemDeleting,       this, &PlaylistItemModel::playlistItemDeleting);
	connect(&m_Playlist, &Playlist::itemReplaced,       this, &PlaylistItemModel::playlistItemReplaced);
	connect(&m_Playlist, &Playlist::itemInserted,       this, &PlaylistItemModel::playlistItemInserted);
	connect(&m_Playlist, &Playlist::currentItemChanged, this, &PlaylistItemModel::playlistCurrentChanged);
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
		m_Playlist.moveItem(static_cast<size_t>(row), static_cast<size_t>(a_Row));
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
		return static_cast<int>(m_Playlist.items().size());
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
		case Qt::TextAlignmentRole:
		{
			switch (a_Index.column())
			{
				case colLength:
				case colDurationLimit:
				case colMeasuresPerMinute:
				{
					return Qt::AlignRight;
				}
			}
			return Qt::AlignLeft;
		}
		case Qt::DisplayRole:
		{
			if ((a_Index.row() < 0) || (a_Index.row() >= static_cast<int>(m_Playlist.items().size())))
			{
				return QVariant();
			}
			const auto & item = m_Playlist.items()[static_cast<size_t>(a_Index.row())];
			switch (a_Index.column())
			{
				case colGenre:             return item->displayGenre();
				case colLength:            return formatLength(item->displayLength());
				case colDurationLimit:     return formatDurationLimit(item->durationLimit());
				case colMeasuresPerMinute: return formatTempo(item->displayTempo());
				case colAuthor:            return item->displayAuthor();
				case colTitle:             return item->displayTitle();
				case colDisplayName:       return item->displayName();
			}
			return QVariant();
		}  // case Qt::DisplayRole

		case Qt::FontRole:
		{
			if (a_Index.row() != m_CurrentItemIdx)
			{
				return QVariant();
			}
			auto font = QApplication::font("QTableWidget");
			font.setBold(true);
			return font;
		}

		case Qt::DecorationRole:
		{
			if (
				(a_Index.row() == m_CurrentItemIdx) &&
				(a_Index.column() == colLength)
			)
			{
				return QIcon(":/img/play.png");
			}
			return QVariant();
		}
		case Qt::UserRole:
		{
			if (a_Index.column() != colReplace)
			{
				return QVariant();
			}
			if ((a_Index.row() < 0) || (a_Index.row() >= static_cast<int>(m_Playlist.items().size())))
			{
				return QVariant();
			}
			const auto & item = m_Playlist.items()[static_cast<size_t>(a_Index.row())];
			auto ps = std::dynamic_pointer_cast<PlaylistItemSong>(item);
			return (ps != nullptr) && (ps->templateItem() != nullptr);
		}
	}
	return QVariant();
}





QVariant PlaylistItemModel::headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const
{
	if (a_Orientation != Qt::Horizontal)
	{
		return QVariant();
	}
	switch (a_Role)
	{
		case Qt::DisplayRole:
		{
			switch (a_Section)
			{
				case colGenre:             return tr("Genre");
				case colLength:            return tr("Length");
				case colDurationLimit:     return tr("Dur", "Duration (limit)");
				case colMeasuresPerMinute: return tr("MPM");
				case colAuthor:            return tr("Author");
				case colTitle:             return tr("Title");
				case colDisplayName:       return tr("Name");
				case colReplace:           return tr("Rplc", "Replace");
			}
			return QVariant();
		}  // case Qt::DisplayRole

		case Qt::ToolTipRole:
		{
			switch (a_Section)
			{
				case colDurationLimit: return tr("Duration (limit)");
				case colReplace:       return tr("Click to replace the song with another one matching the template item");
			}
		}
	}
	return QVariant();
}





void PlaylistItemModel::playlistItemAdded(IPlaylistItem * a_Item)
{
	Q_UNUSED(a_Item);

	auto idx = static_cast<int>(m_Playlist.items().size()) - 1;
	beginInsertRows(QModelIndex(), idx, idx);
	endInsertRows();
}





void PlaylistItemModel::playlistItemDeleting(IPlaylistItem * a_Item, int a_Index)
{
	Q_UNUSED(a_Item);
	beginRemoveRows(QModelIndex(), a_Index, a_Index);
	endRemoveRows();
}





void PlaylistItemModel::playlistItemReplaced(int a_Index, IPlaylistItem * a_NewItem)
{
	Q_UNUSED(a_NewItem);
	emit dataChanged(index(a_Index, 0), index(a_Index, colMax));
}





void PlaylistItemModel::playlistItemInserted(int a_Index, IPlaylistItem * a_NewItem)
{
	Q_UNUSED(a_NewItem);

	beginInsertRows(QModelIndex(), a_Index, a_Index);
	endInsertRows();
}





void PlaylistItemModel::playlistCurrentChanged(int a_CurrentItemIdx)
{
	if (a_CurrentItemIdx == m_CurrentItemIdx)
	{
		return;
	}

	// Emit a change in both the old and new items:
	auto oldItemIdx = m_CurrentItemIdx;
	m_CurrentItemIdx = a_CurrentItemIdx;
	if (m_CurrentItemIdx >= 0)
	{
		auto topLeft = createIndex(m_CurrentItemIdx, 0);
		auto bottomRight = createIndex(m_CurrentItemIdx, colMax);
		emit dataChanged(topLeft, bottomRight);
	}
	if (oldItemIdx >= 0)
	{
		auto topLeft = createIndex(oldItemIdx, 0);
		auto bottomRight = createIndex(oldItemIdx, colMax);
		emit dataChanged(topLeft, bottomRight);
	}
}






static QIcon icoReplace()
{
	static QIcon icon(":/img/replace.png");
	return icon;
}





////////////////////////////////////////////////////////////////////////////////
// PlaylistItemDelegate:

void PlaylistItemDelegate::paint(
	QPainter * a_Painter,
	const QStyleOptionViewItem & a_Option,
	const QModelIndex & a_Index
) const
{
	switch (a_Index.column())
	{
		case PlaylistItemModel::colReplace:
		{
			Super::paint(a_Painter, a_Option, a_Index);
			if (a_Index.model()->data(a_Index, Qt::UserRole).toBool())
			{
				icoReplace().paint(a_Painter, a_Option.rect);
			}
			break;
		}
		default:
		{
			return Super::paint(a_Painter, a_Option, a_Index);
		}
	}
}





QSize PlaylistItemDelegate::sizeHint(const QStyleOptionViewItem & a_Option, const QModelIndex & a_Index) const
{
	switch (a_Index.column())
	{
		case PlaylistItemModel::colReplace:
		{
			return icoReplace().availableSizes()[0];
		}
		default:
		{
			return Super::sizeHint(a_Option, a_Index);
		}
	}
}





bool PlaylistItemDelegate::editorEvent(
	QEvent * a_Event,
	QAbstractItemModel * a_Model,
	const QStyleOptionViewItem & a_Option,
	const QModelIndex & a_Index
)
{
	if (
		(a_Event->type() != QEvent::MouseButtonRelease) ||
		(a_Index.column() != PlaylistItemModel::colReplace)
	)
	{
		return Super::editorEvent(a_Event, a_Model, a_Option, a_Index);
	}
	emit replaceSong(a_Index);
	return true;
}

#include "PlaylistItemModel.hpp"
#include <cassert>
#include <QMimeData>
#include <QDebug>
#include <QIcon>
#include <QFont>
#include <QApplication>
#include <QPainter>
#include "../Utils.hpp"
#include "../PlaylistItemSong.hpp"





#define MIME_TYPE "application/x-playlist-row"





static QString formatLength(double aLength)
{
	if (aLength < 0)
	{
		return PlaylistItemModel::tr("<unknown>", "Length");
	}
	auto len = static_cast<int>(floor(aLength + 0.5));
	return QString("%1:%2").arg(len / 60).arg(QString::number(len % 60), 2, '0');
}





static QString formatTempo(double aTempo)
{
	if (aTempo < 0)
	{
		return PlaylistItemModel::tr("", "Tempo");
	}
	return QString::number(aTempo, 'f', 1);
}





static QString formatDurationLimit(double aDurationLimit)
{
	if (aDurationLimit < 0)
	{
		return QString();
	}
	return Utils::formatTime(aDurationLimit);
}





////////////////////////////////////////////////////////////////////////////////
// PlaylistItemModel:

PlaylistItemModel::PlaylistItemModel(Playlist & aPlaylist):
	mPlaylist(aPlaylist)
{
	connect(&mPlaylist, &Playlist::itemAdded,          this, &PlaylistItemModel::playlistItemAdded);
	connect(&mPlaylist, &Playlist::itemDeleting,       this, &PlaylistItemModel::playlistItemDeleting);
	connect(&mPlaylist, &Playlist::itemReplaced,       this, &PlaylistItemModel::playlistItemReplaced);
	connect(&mPlaylist, &Playlist::itemInserted,       this, &PlaylistItemModel::playlistItemInserted);
	connect(&mPlaylist, &Playlist::currentItemChanged, this, &PlaylistItemModel::playlistCurrentChanged);
	connect(&mPlaylist, &Playlist::itemTimesChanged,   this, &PlaylistItemModel::playlistItemTimesChanged);
}





void PlaylistItemModel::trackWasModified(const IPlaylistItem & aItem)
{
	auto row = mPlaylist.indexFromItem(aItem);
	if (row < 0)
	{
		assert(!"Track notification received for a track not in the playlist");
		return;
	}
	emit dataChanged(index(row, 0), index(row, colMax));
}





Qt::ItemFlags PlaylistItemModel::flags(const QModelIndex & aIndex) const
{
	if (aIndex.isValid())
	{
		return Super::flags(aIndex) | Qt::ItemIsDragEnabled;
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





bool PlaylistItemModel::dropMimeData(const QMimeData * aData, Qt::DropAction aAction, int aRow, int aColumn, const QModelIndex & aParent)
{
	Q_UNUSED(aParent);

	if (!aData->hasFormat(MIME_TYPE))
	{
		return false;
	}
	if (aAction != Qt::MoveAction)
	{
		return true;
	}
	if (aRow < 0)
	{
		qDebug() << "dropMimeData: invalid destination: row " << aRow << ", column " << aColumn << ", parent: " << aParent;
		return true;
	}

	// Decode the indices from the mime data:
	QByteArray encodedData = aData->data(MIME_TYPE);
	std::vector<int> rows;
	auto numRows = static_cast<size_t>(encodedData.size() / 4);
	for (size_t i = 0; i < numRows; ++i)
	{
		int row = 0;
		memcpy(&row, encodedData.data() + i * 4, 4);
		rows.push_back(row);
	}
	std::sort(rows.begin(), rows.end());

	// Move the items:
	auto minRow = aRow;
	auto maxRow = aRow;
	while (!rows.empty())
	{
		auto row = rows.back();
		if ((row != aRow) && (row + 1 != aRow))
		{
			emit layoutAboutToBeChanged();
			beginMoveRows(QModelIndex(), row, row, QModelIndex(), aRow);
			mPlaylist.moveItem(row, aRow);
			endMoveRows();
		}
		if (minRow > row)
		{
			minRow = row;
		}
		if (maxRow < row)
		{
			maxRow = row;
		}
		rows.pop_back();
		if (row < aRow)
		{
			aRow -= 1;  // Moving an item down the list means the destination index moved as well.
		}

		// Move any source rows within the affected range:
		if (row > aRow)
		{
			// Source rows have moved down, add one to their index:
			std::transform(rows.begin(), rows.end(), rows.begin(),
				[row, aRow](int aListRow)
				{
					if ((aListRow > aRow) && (aListRow < row))
					{
						return aListRow + 1;
					}
					return aListRow;
				}
			);
		}
		else if (aRow > row)
		{
			// Source rows have moved up, subtract one from their index:
			std::transform(rows.begin(), rows.end(), rows.begin(),
				[row, aRow](int aListRow)
				{
					if ((aListRow > row) && (aListRow < aRow))
					{
						return aListRow - 1;
					}
					return aListRow;
				}
			);
		}
	}
	mPlaylist.updateItemTimesFromCurrent();
	return true;
}





QMimeData * PlaylistItemModel::mimeData(const QModelIndexList & aIndexes) const
{
	QMimeData * mimeData = new QMimeData();
	QByteArray encodedData;
	foreach (QModelIndex index, aIndexes)
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





int PlaylistItemModel::rowCount(const QModelIndex & aParent) const
{
	if (!aParent.isValid())
	{
		return static_cast<int>(mPlaylist.items().size());
	}
	return 0;
}





int PlaylistItemModel::columnCount(const QModelIndex & aParent) const
{
	Q_UNUSED(aParent);

	return colMax;
}





QVariant PlaylistItemModel::data(const QModelIndex & aIndex, int aRole) const
{
	if (!aIndex.isValid())
	{
		return QVariant();
	}
	switch (aRole)
	{
		case Qt::TextAlignmentRole:
		{
			switch (aIndex.column())
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
			if ((aIndex.row() < 0) || (aIndex.row() >= static_cast<int>(mPlaylist.items().size())))
			{
				return QVariant();
			}
			const auto & item = mPlaylist.items()[static_cast<size_t>(aIndex.row())];
			switch (aIndex.column())
			{
				case colGenre:             return item->displayGenre();
				case colLength:            return formatLength(item->displayLength());
				case colDurationLimit:     return formatDurationLimit(item->durationLimit());
				case colMeasuresPerMinute: return formatTempo(item->displayTempo());
				case colAuthor:            return item->displayAuthor();
				case colTitle:             return item->displayTitle();
				case colDisplayName:       return item->displayName();
				case colTimeStart:
				{
					if (!item->mPlaybackStarted.isNull())
					{
						return item->mPlaybackStarted.toLocalTime().time().toString(Qt::ISODate);
					}
					return QVariant();
				}
				case colTimeEnd:
				{
					if (!item->mPlaybackEnded.isNull())
					{
						return item->mPlaybackEnded.toLocalTime().time().toString(Qt::ISODate);
					}
					return QVariant();
				}
			}
			return QVariant();
		}  // case Qt::DisplayRole

		case Qt::FontRole:
		{
			if ((aIndex.row() < 0) || (aIndex.row() >= static_cast<int>(mPlaylist.items().size())))
			{
				return QVariant();
			}
			auto font = QApplication::font("QTableWidget");
			if (aIndex.row() == mCurrentItemIdx)
			{
				font.setBold(true);
			}
			const auto & item = mPlaylist.items()[static_cast<size_t>(aIndex.row())];
			if (item->isMarkedUnplayable())
			{
				font.setItalic(true);
			}
			return font;
		}

		case Qt::ToolTipRole:
		{
			if ((aIndex.row() < 0) || (aIndex.row() >= static_cast<int>(mPlaylist.items().size())))
			{
				return QVariant();
			}
			const auto & item = mPlaylist.items()[static_cast<size_t>(aIndex.row())];
			if (item->isMarkedUnplayable())
			{
				return QVariant(tr("This track failed to play"));
			}
			return QVariant();
		}

		case Qt::ForegroundRole:
		{
			if ((aIndex.row() < 0) || (aIndex.row() >= static_cast<int>(mPlaylist.items().size())))
			{
				return QVariant();
			}
			const auto & item = mPlaylist.items()[static_cast<size_t>(aIndex.row())];
			if (item->isMarkedUnplayable())
			{
				return QColor(127, 127, 127);
			}
			return QVariant();
		}

		case Qt::BackgroundRole:
		{
			if (aIndex.row() == mCurrentItemIdx)
			{
				return QBrush(QColor(0xdf, 0xdf, 0xdf));  // TODO: Some color from the platform style?
			}
			return QVariant();
		}

		case Qt::DecorationRole:
		{
			if (
				(aIndex.row() == mCurrentItemIdx) &&
				(aIndex.column() == colLength)
			)
			{
				return QIcon(":/img/play.png");
			}
			return QVariant();
		}

		case Qt::UserRole:
		{
			if (aIndex.column() != colReplace)
			{
				return QVariant();
			}
			if ((aIndex.row() < 0) || (aIndex.row() >= static_cast<int>(mPlaylist.items().size())))
			{
				return QVariant();
			}
			const auto & item = mPlaylist.items()[static_cast<size_t>(aIndex.row())];
			auto ps = std::dynamic_pointer_cast<PlaylistItemSong>(item);
			return (ps != nullptr) && (ps->filter() != nullptr);
		}
	}
	return QVariant();
}





QVariant PlaylistItemModel::headerData(int aSection, Qt::Orientation aOrientation, int aRole) const
{
	if (aOrientation != Qt::Horizontal)
	{
		return QVariant();
	}
	switch (aRole)
	{
		case Qt::DisplayRole:
		{
			switch (aSection)
			{
				case colGenre:             return tr("Genre");
				case colLength:            return tr("Length");
				case colDurationLimit:     return tr("Dur", "Duration (limit)");
				case colMeasuresPerMinute: return tr("MPM");
				case colAuthor:            return tr("Author");
				case colTitle:             return tr("Title");
				case colDisplayName:       return tr("Name");
				case colReplace:           return tr("Rplc", "Replace");
				case colTimeStart:         return tr("Start", "Time when playback started / will start");
				case colTimeEnd:           return tr("End", "Time when playback ended / will end");
			}
			return QVariant();
		}  // case Qt::DisplayRole

		case Qt::ToolTipRole:
		{
			switch (aSection)
			{
				case colDurationLimit: return tr("Duration (limit)");
				case colReplace:       return tr("Click to replace the song with another one matching the template item");
			}
		}
	}
	return QVariant();
}





void PlaylistItemModel::playlistItemAdded(IPlaylistItem * aItem)
{
	Q_UNUSED(aItem);

	auto idx = static_cast<int>(mPlaylist.items().size()) - 1;
	beginInsertRows(QModelIndex(), idx, idx);
	endInsertRows();
}





void PlaylistItemModel::playlistItemDeleting(IPlaylistItem * aItem, int aIndex)
{
	Q_UNUSED(aItem);
	beginRemoveRows(QModelIndex(), aIndex, aIndex);
	endRemoveRows();
}





void PlaylistItemModel::playlistItemReplaced(int aIndex, IPlaylistItem * aNewItem)
{
	Q_UNUSED(aNewItem);
	emit dataChanged(index(aIndex, 0), index(aIndex, colMax));
}





void PlaylistItemModel::playlistItemInserted(int aIndex, IPlaylistItem * aNewItem)
{
	Q_UNUSED(aNewItem);

	beginInsertRows(QModelIndex(), aIndex, aIndex);
	endInsertRows();
}





void PlaylistItemModel::playlistCurrentChanged(int aCurrentItemIdx)
{
	if (aCurrentItemIdx == mCurrentItemIdx)
	{
		return;
	}

	// Emit a change in both the old and new items:
	auto oldItemIdx = mCurrentItemIdx;
	mCurrentItemIdx = aCurrentItemIdx;
	if (mCurrentItemIdx >= 0)
	{
		auto topLeft = createIndex(mCurrentItemIdx, 0);
		auto bottomRight = createIndex(mCurrentItemIdx, colMax);
		emit dataChanged(topLeft, bottomRight);
	}
	if (oldItemIdx >= 0)
	{
		auto topLeft = createIndex(oldItemIdx, 0);
		auto bottomRight = createIndex(oldItemIdx, colMax);
		emit dataChanged(topLeft, bottomRight);
	}
}





void PlaylistItemModel::playlistItemTimesChanged(int aItemIdx, IPlaylistItem * aItem)
{
	Q_UNUSED(aItem);
	emit dataChanged(index(aItemIdx, 0), index(aItemIdx, colMax));
}






static QIcon icoReplace()
{
	static QIcon icon(":/img/replace.png");
	return icon;
}





////////////////////////////////////////////////////////////////////////////////
// PlaylistItemDelegate:

void PlaylistItemDelegate::paint(
	QPainter * aPainter,
	const QStyleOptionViewItem & aOption,
	const QModelIndex & aIndex
) const
{
	switch (aIndex.column())
	{
		case PlaylistItemModel::colReplace:
		{
			Super::paint(aPainter, aOption, aIndex);
			if (aIndex.model()->data(aIndex, Qt::UserRole).toBool())
			{
				icoReplace().paint(aPainter, aOption.rect);
			}
			break;
		}
		default:
		{
			return Super::paint(aPainter, aOption, aIndex);
		}
	}
}





QSize PlaylistItemDelegate::sizeHint(const QStyleOptionViewItem & aOption, const QModelIndex & aIndex) const
{
	switch (aIndex.column())
	{
		case PlaylistItemModel::colReplace:
		{
			return icoReplace().availableSizes()[0];
		}
		default:
		{
			return Super::sizeHint(aOption, aIndex);
		}
	}
}





bool PlaylistItemDelegate::editorEvent(
	QEvent * aEvent,
	QAbstractItemModel * aModel,
	const QStyleOptionViewItem & aOption,
	const QModelIndex & aIndex
)
{
	if (
		(aEvent->type() != QEvent::MouseButtonRelease) ||
		(aIndex.column() != PlaylistItemModel::colReplace)
	)
	{
		return Super::editorEvent(aEvent, aModel, aOption, aIndex);
	}
	emit replaceSong(aIndex);
	return true;
}

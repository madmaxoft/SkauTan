#include "SongModel.h"
#include <QDebug>
#include <QSize>
#include "SongDatabase.h"





/** Returns a string representation of the specified song's length.
a_Length is the length in seconds. */
static QString formatLength(const Song & a_Song)
{
	if (!a_Song.length().isValid())
	{
		return SongModel::tr("<unknown>", "Length");
	}
	auto len = static_cast<int>(floor(a_Song.length().toDouble() + 0.5));
	return QString("%1:%2").arg(len / 60).arg(len % 60);
}





static QString formatMPM(const Song & a_Song)
{
	if (!a_Song.measuresPerMinute().isValid())
	{
		return SongModel::tr("<unknown>", "MPM");
	}
	return QString::number(a_Song.measuresPerMinute().toDouble(), 'f', 1);
}





static QString formatLastPlayed(const Song & a_Song)
{
	if (!a_Song.lastPlayed().isValid())
	{
		return SongModel::tr("<never>", "LastPlayed");
	}
	return a_Song.lastPlayed().toDateTime().toString("yyyy-MM-dd HH:mm:ss");
}





static QString formatRating(const Song & a_Song)
{
	if (!a_Song.rating().isValid())
	{
		return SongModel::tr("<none>", "Rating");
	}
	return QString::number(a_Song.rating().toDouble(), 'f', 1);
}





////////////////////////////////////////////////////////////////////////////////
// SongModel:

SongModel::SongModel(SongDatabase & a_DB):
	m_DB(a_DB)
{
	connect(&m_DB, &SongDatabase::songFileAdded, this, &SongModel::addSongFile);
}





SongPtr SongModel::songFromIndex(const QModelIndex & a_Idx) const
{
	if (!a_Idx.isValid())
	{
		return nullptr;
	}
	auto row = a_Idx.row();
	if ((row < 0) || (static_cast<size_t>(row) >= m_DB.songs().size()))
	{
		return nullptr;
	}
	return m_DB.songs()[row];
}





int SongModel::rowCount(const QModelIndex & a_Parent) const
{
	Q_UNUSED(a_Parent);

	return static_cast<int>(m_DB.songs().size());
}





int SongModel::columnCount(const QModelIndex & a_Parent) const
{
	Q_UNUSED(a_Parent);

	return colMax;
}





QVariant SongModel::data(const QModelIndex & a_Index, int a_Role) const
{
	if (
		!a_Index.isValid() ||
		(a_Index.row() < 0) ||
		(a_Index.row() >= static_cast<int>(m_DB.songs().size()))
	)
	{
		return QVariant();
	}
	const auto & song = m_DB.songs()[a_Index.row()];
	switch (a_Role)
	{
		case Qt::DisplayRole:
		case Qt::EditRole:
		{
			switch (a_Index.column())
			{
				case colFileName:          return song->fileName();
				case colGenre:             return song->genre();
				case colLength:            return formatLength(*song);
				case colMeasuresPerMinute: return formatMPM(*song);
				case colLastPlayed:        return formatLastPlayed(*song);
				case colRating:            return formatRating(*song);
				case colAuthor:            return song->author();
				case colTitle:             return song->title();
			}
			break;
		}
	}
	return QVariant();
}





QVariant SongModel::headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const
{
	if (a_Orientation != Qt::Horizontal)
	{
		return QVariant();
	}
	if (a_Role != Qt::DisplayRole)
	{
		return QVariant();
	}
	switch (a_Section)
	{
		case colFileName:          return tr("File name");
		case colGenre:             return tr("Genre");
		case colLength:            return tr("Length");
		case colMeasuresPerMinute: return tr("MPM");
		case colLastPlayed:        return tr("Last played");
		case colRating:            return tr("Rating");
		case colAuthor:            return tr("Author");
		case colTitle:             return tr("Title");
	}
	return QVariant();
}





Qt::ItemFlags SongModel::flags(const QModelIndex & a_Index) const
{
	if (a_Index.isValid())
	{
		switch (a_Index.column())
		{
			case colAuthor:
			case colGenre:
			case colMeasuresPerMinute:
			case colTitle:
			{
				// Editable:
				return Super::flags(a_Index) | Qt::ItemIsEditable;
			}
			default:
			{
				// Not editable
				return Super::flags(a_Index);
			}
		}
	}
	else
	{
		return Super::flags(a_Index);
	}
}





bool SongModel::setData(const QModelIndex & a_Index, const QVariant & a_Value, int a_Role)
{
	if ((a_Role != Qt::DisplayRole) && (a_Role != Qt::EditRole))
	{
		return false;
	}
	if (
		!a_Index.isValid() ||
		(a_Index.row() < 0) ||
		(a_Index.row() >= static_cast<int>(m_DB.songs().size()))
	)
	{
		return false;
	}
	const auto & song = m_DB.songs()[a_Index.row()];
	switch (a_Index.column())
	{
		case colAuthor: song->setAuthor(a_Value.toString()); break;
		case colFileName: return false;
		case colGenre: song->setGenre(a_Value.toString()); break;
		case colLastPlayed: return false;
		case colLength: return false;
		case colMeasuresPerMinute: song->setMeasuresPerMinute(a_Value.toDouble()); break;
		case colRating: return false;
		case colTitle: song->setTitle(a_Value.toString()); break;
	}
	emit songEdited(song.get());
	emit dataChanged(a_Index, a_Index, {a_Role});
	return true;
}





void SongModel::addSongFile(Song * a_NewSong)
{
	Q_UNUSED(a_NewSong);

	auto lastIdx = static_cast<int>(m_DB.songs().size()) - 1;
	beginInsertRows(QModelIndex(), lastIdx, lastIdx + 1);
	endInsertRows();
}

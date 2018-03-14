#include "SongModel.h"
#include <assert.h>
#include <QDebug>
#include <QSize>
#include <QComboBox>
#include <QLineEdit>
#include "Database.h"





/** Returns a string representation of the specified song's length.
a_Length is the length in seconds. */
static QString formatLength(const Song & a_Song)
{
	if (!a_Song.length().isValid())
	{
		return SongModel::tr("<unknown>", "Length");
	}
	auto len = static_cast<int>(floor(a_Song.length().toDouble() + 0.5));
	return QString("%1:%2").arg(len / 60).arg(QString::number(len % 60), 2, QChar('0'));
}





static QString formatMPM(const Song & a_Song)
{
	if (!a_Song.measuresPerMinute().isValid())
	{
		return SongModel::tr("<unknown>", "MPM");
	}
	return QLocale().toString(a_Song.measuresPerMinute().toDouble(), 'f', 1);
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

SongModel::SongModel(Database & a_DB):
	m_DB(a_DB)
{
	connect(&m_DB,                   &Database::songFileAdded,      this, &SongModel::addSongFile);
	connect(&m_DB.metadataScanner(), &MetadataScanner::songScanned, this, &SongModel::songMetadataScanned);
}





SongPtr SongModel::songFromIndex(const QModelIndex & a_Idx) const
{
	if (!a_Idx.isValid())
	{
		return nullptr;
	}
	return songFromRow(a_Idx.row());
}





SongPtr SongModel::songFromRow(int a_Row) const
{
	if ((a_Row < 0) || (static_cast<size_t>(a_Row) >= m_DB.songs().size()))
	{
		return nullptr;
	}
	return m_DB.songs()[a_Row];
}





QModelIndex SongModel::indexFromSong(const Song * a_Song, int a_Column)
{
	const auto & songs = m_DB.songs();
	int row = 0;
	for (const auto & s: songs)
	{
		if (s.get() == a_Song)
		{
			return createIndex(row, a_Column);
		}
		row += 1;
	}
	assert(!"Song not found");
	return QModelIndex();
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





void SongModel::delSong(const Song * a_Song, size_t a_Index)
{
	Q_UNUSED(a_Song);

	auto idx = static_cast<int>(a_Index);
	beginRemoveRows(QModelIndex(), idx, idx);
	endRemoveRows();
}





void SongModel::songMetadataScanned(Song * a_Song)
{
	auto idx = indexFromSong(a_Song);
	if (!idx.isValid())
	{
		return;
	}
	auto idx2 = createIndex(idx.row(), colMax - 1);
	emit dataChanged(idx, idx2, {Qt::DisplayRole});
}





////////////////////////////////////////////////////////////////////////////////
// SongModelEditorDelegate:

SongModelEditorDelegate::SongModelEditorDelegate(QWidget * a_Parent):
	Super(a_Parent)
{
}





QWidget * SongModelEditorDelegate::createEditor(
	QWidget * a_Parent,
	const QStyleOptionViewItem & a_Option,
	const QModelIndex & a_Index
) const
{
	if (!a_Index.isValid())
	{
		return nullptr;
	}
	switch (a_Index.column())
	{
		case SongModel::colAuthor:
		case SongModel::colTitle:
		{
			// Default text editor:
			return Super::createEditor(a_Parent, a_Option, a_Index);
		}
		case SongModel::colMeasuresPerMinute:
		{
			// Text editor limited to entering numbers:
			auto res = new QLineEdit(a_Parent);
			res->setFrame(false);
			res->setValidator(new QDoubleValidator(res));
			return res;
		}
		case SongModel::colFileName:
		case SongModel::colLastPlayed:
		case SongModel::colLength:
		case SongModel::colRating:
		{
			// Not editable
			return nullptr;
		}
		case SongModel::colGenre:
		{
			// Special editor:
			auto res = new QComboBox(a_Parent);
			res->setFrame(false);
			res->addItems({"SW", "TG", "VW", "SF", "QS", "SB", "CH", "RU", "PD", "JI", "PO", "BL"});
			return res;
		}
	}
	assert(!"Unknown column");
	return nullptr;
}





void SongModelEditorDelegate::setEditorData(
	QWidget * a_Editor,
	const QModelIndex & a_Index
) const
{
	if (!a_Index.isValid())
	{
		return;
	}
	switch (a_Index.column())
	{
		case SongModel::colAuthor:
		case SongModel::colTitle:
		{
			// Default text editor:
			Super::setEditorData(a_Editor, a_Index);
			return;
		}
		case SongModel::colMeasuresPerMinute:
		{
			auto le = qobject_cast<QLineEdit *>(a_Editor);
			assert(le != nullptr);
			le->setText(a_Index.data().toString());
			return;
		}
		case SongModel::colFileName:
		case SongModel::colLastPlayed:
		case SongModel::colLength:
		case SongModel::colRating:
		{
			// Not editable
			assert(!"Attempting to edit an uneditable field");
			return;
		}
		case SongModel::colGenre:
		{
			// Special editor:
			auto cb = qobject_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			cb->setCurrentText(a_Index.data().toString());
			return;
		}
	}
	assert(!"Unknown column");
}





void SongModelEditorDelegate::setModelData(
	QWidget * a_Editor,
	QAbstractItemModel * a_Model,
	const QModelIndex & a_Index
) const
{
	if (!a_Index.isValid())
	{
		return;
	}
	switch (a_Index.column())
	{
		case SongModel::colAuthor:
		case SongModel::colTitle:
		{
			// Default text editor:
			Super::setModelData(a_Editor, a_Model, a_Index);
			return;
		}
		case SongModel::colMeasuresPerMinute:
		{
			auto le = qobject_cast<QLineEdit *>(a_Editor);
			assert(le != nullptr);
			a_Model->setData(a_Index, le->locale().toDouble(le->text()));
			return;
		}
		case SongModel::colFileName:
		case SongModel::colLastPlayed:
		case SongModel::colLength:
		case SongModel::colRating:
		{
			// Not editable
			return;
		}
		case SongModel::colGenre:
		{
			// Special editor:
			auto cb = qobject_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			a_Model->setData(a_Index, cb->currentText());
			return;
		}
	}
	assert(!"Unknown column");
}





void SongModelEditorDelegate::commitAndCloseEditor()
{
	auto editor = qobject_cast<QWidget *>(sender());
	emit commitData(editor);
	emit closeEditor(editor);
}


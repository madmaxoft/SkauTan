#include "SongModel.h"
#include <assert.h>
#include <QDebug>
#include <QSize>
#include <QComboBox>
#include <QLineEdit>
#include "Database.h"
#include "MetadataScanner.h"
#include "Stopwatch.h"





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





static QString formatMPM(const QVariant & a_SongMPM)
{
	if (!a_SongMPM.isValid())
	{
		return SongModel::tr("<unknown>", "MPM");
	}
	return QLocale().toString(a_SongMPM.toDouble(), 'f', 1);
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
	const auto & rating = a_Song.rating();
	if (!rating.isValid())
	{
		return SongModel::tr("<none>", "Rating");
	}
	return QString::number(rating.toDouble(), 'f', 1);
}





////////////////////////////////////////////////////////////////////////////////
// SongModel:

SongModel::SongModel(Database & a_DB):
	m_DB(a_DB)
{
	connect(&m_DB, &Database::songFileAdded, this, &SongModel::addSongFile);
	connect(&m_DB, &Database::songSaved,     this, &SongModel::songChanged);
	// TODO: Deleting songs from the DB should remove them from the model
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
	return m_DB.songs()[static_cast<size_t>(a_Row)];
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
	const auto & song = songFromRow(a_Index.row());
	switch (a_Role)
	{
		case Qt::DisplayRole:
		case Qt::EditRole:
		{
			switch (a_Index.column())
			{
				case colFileName:                  return song->fileName();
				case colLength:                    return formatLength(*song);
				case colLastPlayed:                return formatLastPlayed(*song);
				case colRating:                    return formatRating(*song);
				case colManualAuthor:              return song->tagManual().m_Author;
				case colManualTitle:               return song->tagManual().m_Title;
				case colManualGenre:               return song->tagManual().m_Genre;
				case colManualMeasuresPerMinute:   return formatMPM(song->tagManual().m_MeasuresPerMinute);
				case colFileNameAuthor:            return song->tagFileName().m_Author;
				case colFileNameTitle:             return song->tagFileName().m_Title;
				case colFileNameGenre:             return song->tagFileName().m_Genre;
				case colFileNameMeasuresPerMinute: return formatMPM(song->tagFileName().m_MeasuresPerMinute);
				case colId3Author:                 return song->tagId3().m_Author;
				case colId3Title:                  return song->tagId3().m_Title;
				case colId3Genre:                  return song->tagId3().m_Genre;
				case colId3MeasuresPerMinute:      return formatMPM(song->tagId3().m_MeasuresPerMinute);
				case colNumMatchingFilters:        return numMatchingFilters(song);
			}
			break;
		}
		case Qt::BackgroundColorRole:
		{
			if (!song->getWarnings().isEmpty())
			{
				return QColor(255, 192, 192);
			}
			return QVariant();
		}
		case Qt::ToolTipRole:
		{
			return song->getWarnings().join("\n");
		}
		case Qt::TextAlignmentRole:
		{
			switch (a_Index.column())
			{
				case colNumMatchingFilters:
				case colLength:
				case colManualMeasuresPerMinute:
				case colId3MeasuresPerMinute:
				case colFileNameMeasuresPerMinute:
				{
					return Qt::AlignRight;
				}
			}
			return Qt::AlignLeft;
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
		case colFileName:                  return tr("File name");
		case colLength:                    return tr("Length");
		case colLastPlayed:                return tr("Last played");
		case colRating:                    return tr("Rating");
		case colManualAuthor:              return tr("Author (M)", "Manual");
		case colManualTitle:               return tr("Title (M)", "Manual");
		case colManualGenre:               return tr("Genre (M)", "Manual");
		case colManualMeasuresPerMinute:   return tr("MPM (M)", "Manual");
		case colFileNameAuthor:            return tr("Author (F)", "FileName");
		case colFileNameTitle:             return tr("Title (F)", "FileName");
		case colFileNameGenre:             return tr("Genre (F)", "FileName");
		case colFileNameMeasuresPerMinute: return tr("MPM (F)", "FileName");
		case colId3Author:                 return tr("Author (T)", "ID3 Tag");
		case colId3Title:                  return tr("Title (T)", "ID3 Tag");
		case colId3Genre:                  return tr("Genre (T)", "ID3 Tag");
		case colId3MeasuresPerMinute:      return tr("MPM (T)", "ID3 Tag");
		case colNumMatchingFilters:        return tr("# flt", "Num matching favorite filters");
	}
	return QVariant();
}





Qt::ItemFlags SongModel::flags(const QModelIndex & a_Index) const
{
	if (a_Index.isValid())
	{
		switch (a_Index.column())
		{
			case colManualAuthor:
			case colManualGenre:
			case colManualMeasuresPerMinute:
			case colManualTitle:
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
	const auto & song = songFromRow(a_Index.row());
	switch (a_Index.column())
	{
		case colFileName: return false;
		case colLastPlayed: return false;
		case colLength: return false;
		case colRating: return false;
		case colManualAuthor: song->setAuthor(a_Value.toString()); break;
		case colManualTitle: song->setTitle(a_Value.toString()); break;
		case colManualGenre: song->setGenre(a_Value.toString()); break;
		case colManualMeasuresPerMinute: song->setMeasuresPerMinute(a_Value.toDouble()); break;
	}
	emit songEdited(song);
	emit dataChanged(a_Index, a_Index, {a_Role});
	return true;
}





qulonglong SongModel::numMatchingFilters(SongPtr a_Song) const
{
	qulonglong res = 0;
	for (const auto & item: m_DB.getFavoriteTemplateItems())
	{
		if (item->filter()->isSatisfiedBy(*a_Song))
		{
			res += 1;
		}
	}
	return res;
}





void SongModel::addSongFile(SongPtr a_NewSong)
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





void SongModel::songChanged(SongPtr a_Song)
{
	auto idx = indexFromSong(a_Song.get());
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
		case SongModel::colManualAuthor:
		case SongModel::colManualTitle:
		{
			// Default text editor:
			return Super::createEditor(a_Parent, a_Option, a_Index);
		}
		case SongModel::colManualGenre:
		{
			// Special editor:
			auto res = new QComboBox(a_Parent);
			res->setFrame(false);
			res->addItems({"SW", "TG", "VW", "SF", "QS", "SB", "CH", "RU", "PD", "JI", "PO", "BL", "SL", "RO"});
			res->setMaxVisibleItems(res->count());
			return res;
		}
		case SongModel::colManualMeasuresPerMinute:
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
		case SongModel::colFileNameAuthor:
		case SongModel::colFileNameTitle:
		case SongModel::colFileNameGenre:
		case SongModel::colFileNameMeasuresPerMinute:
		case SongModel::colId3Author:
		case SongModel::colId3Title:
		case SongModel::colId3Genre:
		case SongModel::colId3MeasuresPerMinute:
		{
			// Not editable
			return nullptr;
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
		case SongModel::colManualAuthor:
		case SongModel::colManualTitle:
		{
			// Default text editor:
			Super::setEditorData(a_Editor, a_Index);
			return;
		}
		case SongModel::colManualGenre:
		{
			// Special editor:
			auto cb = qobject_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			cb->setCurrentText(a_Index.data().toString());
			return;
		}
		case SongModel::colManualMeasuresPerMinute:
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
		case SongModel::colFileNameAuthor:
		case SongModel::colFileNameTitle:
		case SongModel::colFileNameGenre:
		case SongModel::colFileNameMeasuresPerMinute:
		case SongModel::colId3Author:
		case SongModel::colId3Title:
		case SongModel::colId3Genre:
		case SongModel::colId3MeasuresPerMinute:
		{
			// Not editable
			assert(!"Attempting to edit an uneditable field");
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
		case SongModel::colManualAuthor:
		case SongModel::colManualTitle:
		{
			// Default text editor:
			Super::setModelData(a_Editor, a_Model, a_Index);
			return;
		}
		case SongModel::colManualGenre:
		{
			// Special editor:
			auto cb = qobject_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			a_Model->setData(a_Index, cb->currentText());
			return;
		}
		case SongModel::colManualMeasuresPerMinute:
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
		case SongModel::colFileNameAuthor:
		case SongModel::colFileNameTitle:
		case SongModel::colFileNameGenre:
		case SongModel::colFileNameMeasuresPerMinute:
		case SongModel::colId3Author:
		case SongModel::colId3Title:
		case SongModel::colId3Genre:
		case SongModel::colId3MeasuresPerMinute:
		{
			// Not editable
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





////////////////////////////////////////////////////////////////////////////////
// SongModelFilter:

SongModelFilter::SongModelFilter(SongModel & a_ParentModel):
	m_ParentModel(a_ParentModel),
	m_Filter(fltNone),
	m_SearchString("")
{
	setSourceModel(&m_ParentModel);
}





void SongModelFilter::setFilter(SongModelFilter::EFilter a_Filter)
{
	if (m_Filter == a_Filter)
	{
		return;
	}
	m_Filter = a_Filter;
	STOPWATCH("setFilter");
	invalidateFilter();
}





void SongModelFilter::setSearchString(const QString & a_SearchString)
{
	m_SearchString.setPattern(a_SearchString);
	STOPWATCH("setSearchString");
	invalidateFilter();
}





void SongModelFilter::setFavoriteTemplateItems(const std::vector<Template::ItemPtr> & a_FavoriteTemplateItems)
{
	m_FavoriteTemplateItems = a_FavoriteTemplateItems;
	STOPWATCH("setFavoriteTemplateItems");
	invalidateFilter();
}





bool SongModelFilter::filterAcceptsRow(int a_SrcRow, const QModelIndex & a_SrcParent) const
{
	assert(!a_SrcParent.isValid());
	Q_UNUSED(a_SrcParent);  // For release builds

	auto song = m_ParentModel.songFromRow(a_SrcRow);
	switch (m_Filter)
	{
		case fltNone: break;
		case fltNoId3:
		{
			const auto & id3Tag = song->tagId3();
			if (
				(id3Tag.m_Author.isNull() || id3Tag.m_Author.toString().isEmpty()) &&
				(id3Tag.m_Title.isNull()  || id3Tag.m_Title.toString().isEmpty()) &&
				(id3Tag.m_Genre.isNull()  || id3Tag.m_Genre.toString().isEmpty()) &&
				(id3Tag.m_MeasuresPerMinute.isNull())
			)
			{
				break;
			}
			return false;  // Don't want songs with valid ID3 tag
		}  // case fltNoId3

		case fltNoGenre:
		{
			const auto & genre = song->primaryGenre();
			if (genre.isNull() || genre.toString().isEmpty())
			{
				break;
			}
			return false;  // Don't want songs with valid genre
		}  // case fltNoGenre

		case fltNoMeasuresPerMinute:
		{
			if (song->primaryMeasuresPerMinute().isNull())
			{
				break;
			}
			return false;
		}  // case fltNoMeasuresPerMinute

		case fltWarnings:
		{
			if (song->getWarnings().isEmpty())
			{
				return false;
			}
			break;
		}  // case fltWarnings

		case fltNoTemplateFilterMatch:
		{
			for (const auto & item: m_FavoriteTemplateItems)
			{
				if (item->filter()->isSatisfiedBy(*song))
				{
					return false;  // Don't want songs matching a template filter
				}
			}
			break;
		}
	}
	return songMatchesSearchString(song);
}





bool SongModelFilter::songMatchesSearchString(SongPtr a_Song) const
{
	if (
		m_SearchString.match(a_Song->tagManual().m_Author.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagManual().m_Title.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagManual().m_Genre.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagId3().m_Author.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagId3().m_Title.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagId3().m_Genre.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagFileName().m_Author.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagFileName().m_Title.toString()).hasMatch() ||
		m_SearchString.match(a_Song->tagFileName().m_Genre.toString()).hasMatch() ||
		m_SearchString.match(a_Song->fileName()).hasMatch()
	)
	{
		return true;
	}
	return false;
}

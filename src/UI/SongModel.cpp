#include "SongModel.hpp"
#include <cassert>
#include <QDebug>
#include <QSize>
#include <QComboBox>
#include <QLineEdit>
#include "../DB/Database.hpp"
#include "../MetadataScanner.hpp"
#include "../Stopwatch.hpp"
#include "../Utils.hpp"





/** Returns a string representation of the specified song's length.
a_Length is the length in seconds. */
static QString formatLength(const Song & aSong)
{
	if (!aSong.length().isPresent())
	{
		return QString();
	}
	auto len = static_cast<int>(floor(aSong.length().value() + 0.5));
	return QString("%1:%2").arg(len / 60).arg(QString::number(len % 60), 2, QChar('0'));
}





static QString formatMPM(const DatedOptional<double> & aSongMPM)
{
	if (!aSongMPM.isPresent())
	{
		return QString();
	}
	return QLocale().toString(aSongMPM.value(), 'f', 1);
}





static QString formatLastPlayed(const Song & aSong)
{
	if (!aSong.lastPlayed().isPresent())
	{
		return QString();
	}
	return aSong.lastPlayed().value().toString("yyyy-MM-dd HH:mm:ss");
}





/** Returns the number in the DatedOptional, if present, as a string, or an empty string if empty. */
template <typename T> static QString numberIfPresent(const DatedOptional<T> & aValue)
{
	if (aValue.isPresent())
	{
		return QLocale::system().toString(aValue.value(), 'f', 1);
	}
	else
	{
		return {};
	}
}





static QString formatRating(const Song & aSong)
{
	const auto & rating = aSong.rating();
	QString res;
	res.append(rating.mRhythmClarity.isPresent()   ? QString::number(rating.mRhythmClarity.value(),   'f', 1) : "--");
	res.append(" | ");
	res.append(rating.mGenreTypicality.isPresent() ? QString::number(rating.mGenreTypicality.value(), 'f', 1) : "--");
	res.append(" | ");
	res.append(rating.mPopularity.isPresent()      ? QString::number(rating.mPopularity.value(),      'f', 1) : "--");
	return res;
}





template <typename T> static QString formatLastModTooltip(const DatedOptional<T> & aValue)
{
	if (!aValue.lastModification().isValid())
	{
		return QString("%1").arg(aValue.valueOrDefault());  // Convert from non-strings to QString
	}
	else
	{
		return SongModel::tr("%1\nLast modified: %2")
			.arg(aValue.valueOrDefault())
			.arg(aValue.lastModification().toString(Qt::SystemLocaleLongDate));
	}
}





static QString formatSkipStart(const DatedOptional<double> & aSkipStart)
{
	if (!aSkipStart.isPresent())
	{
		return QString();
	}
	return Utils::formatFractionalTime(aSkipStart.value(), 3);
}





/** Returns true if the song's detected tempo is not empty and outside competition range. */
static bool isDetectedTempoOutsideCompetitionRange(SongPtr aSong)
{
	auto tempo = aSong->sharedData()->mDetectedTempo;
	if (!tempo.isPresent())
	{
		return false;
	}
	auto genre = aSong->primaryGenre().valueOrDefault();
	auto competitionRange = Song::competitionTempoRangeForGenre(genre);
	if (competitionRange.first <= 0)
	{
		// Unknown genre
		return false;
	}
	auto tempoVal = tempo.value();
	return (tempoVal < competitionRange.first) || (tempoVal > competitionRange.second);
}





////////////////////////////////////////////////////////////////////////////////
// SongModel:

SongModel::SongModel(Database & aDB):
	mDB(aDB)
{
	connect(&mDB, &Database::songFileAdded, this, &SongModel::addSongFile);
	connect(&mDB, &Database::songSaved,     this, &SongModel::songChanged);
	connect(&mDB, &Database::songRemoved,   this, &SongModel::delSong);
}





SongPtr SongModel::songFromIndex(const QModelIndex & aIdx) const
{
	if (!aIdx.isValid())
	{
		return nullptr;
	}
	return songFromRow(aIdx.row());
}





SongPtr SongModel::songFromRow(int aRow) const
{
	if ((aRow < 0) || (static_cast<size_t>(aRow) >= mDB.songs().size()))
	{
		return nullptr;
	}
	return mDB.songs()[static_cast<size_t>(aRow)];
}





QModelIndex SongModel::indexFromSong(const Song * aSong, int aColumn)
{
	const auto & songs = mDB.songs();
	int row = 0;
	for (const auto & s: songs)
	{
		if (s.get() == aSong)
		{
			return createIndex(row, aColumn);
		}
		row += 1;
	}
	assert(!"Song not found");
	return QModelIndex();
}





int SongModel::rowCount(const QModelIndex & aParent) const
{
	Q_UNUSED(aParent);

	return static_cast<int>(mDB.songs().size());
}





int SongModel::columnCount(const QModelIndex & aParent) const
{
	Q_UNUSED(aParent);

	return colMax;
}





QVariant SongModel::data(const QModelIndex & aIndex, int aRole) const
{
	if (
		!aIndex.isValid() ||
		(aIndex.row() < 0) ||
		(aIndex.row() >= static_cast<int>(mDB.songs().size()))
	)
	{
		return QVariant();
	}
	const auto & song = songFromRow(aIndex.row());
	switch (aRole)
	{
		case roleSongPtr: return QVariant::fromValue(song);
		case Qt::DisplayRole:
		case Qt::EditRole:
		{
			switch (aIndex.column())
			{
				case colFileName:                  return song->fileName();
				case colLength:                    return formatLength(*song);
				case colLastPlayed:                return formatLastPlayed(*song);
				case colLocalRating:               return numberIfPresent(song->rating().mLocal);
				case colRating:                    return formatRating(*song);
				case colManualAuthor:              return song->tagManual().mAuthor.valueOrDefault();
				case colManualTitle:               return song->tagManual().mTitle.valueOrDefault();
				case colManualGenre:               return song->tagManual().mGenre.valueOrDefault();
				case colManualMeasuresPerMinute:   return formatMPM(song->tagManual().mMeasuresPerMinute);
				case colFileNameAuthor:            return song->tagFileName().mAuthor.valueOrDefault();
				case colFileNameTitle:             return song->tagFileName().mTitle.valueOrDefault();
				case colFileNameGenre:             return song->tagFileName().mGenre.valueOrDefault();
				case colFileNameMeasuresPerMinute: return formatMPM(song->tagFileName().mMeasuresPerMinute);
				case colId3Author:                 return song->tagId3().mAuthor.valueOrDefault();
				case colId3Title:                  return song->tagId3().mTitle.valueOrDefault();
				case colId3Genre:                  return song->tagId3().mGenre.valueOrDefault();
				case colId3MeasuresPerMinute:      return formatMPM(song->tagId3().mMeasuresPerMinute);
				case colDetectedTempo:             return formatMPM(song->sharedData()->mDetectedTempo);
				case colNumMatchingFilters:        return numMatchingFilters(song);
				case colNumDuplicates:             return numDuplicates(song);
				case colSkipStart:                 return formatSkipStart(song->skipStart());
			}
			break;
		}
		case Qt::BackgroundColorRole:
		{
			switch (aIndex.column())
			{
				case colDetectedTempo:      return (isDetectedTempoOutsideCompetitionRange(song)) ? QColor(255, 255, 192) : QVariant();
				case colNumMatchingFilters: return (numMatchingFilters(song) > 0) ? QVariant() : QColor(255, 192, 192);
				case colNumDuplicates:      return (numDuplicates(song) < 2) ? QVariant() : QColor(255, 192, 192);
				case colSkipStart:          return (song->skipStart().valueOrDefault() > 0) ? QColor(255, 255, 192) : QVariant();
			}
			if (!song->getWarnings().isEmpty())
			{
				return QColor(255, 192, 192);
			}
			return QVariant();
		}
		case Qt::ToolTipRole:
		{
			switch (aIndex.column())
			{
				case colManualAuthor:            return formatLastModTooltip(song->tagManual().mAuthor);
				case colManualTitle:             return formatLastModTooltip(song->tagManual().mTitle);
				case colManualGenre:             return formatLastModTooltip(song->tagManual().mGenre);
				case colManualMeasuresPerMinute: return formatLastModTooltip(song->tagManual().mMeasuresPerMinute);
				case colDetectedTempo:
				{
					if (isDetectedTempoOutsideCompetitionRange(song))
					{
						auto range = Song::competitionTempoRangeForGenre(song->primaryGenre().valueOrDefault());
						return (
							tr("The detected tempo is suspicious: outside the competition range (%1 - %2).")
								.arg(range.first).arg(range.second) + "\n" +
							formatLastModTooltip(song->sharedData()->mDetectedTempo)
						);
					}
					else
					{
						return formatLastModTooltip(song->sharedData()->mDetectedTempo);
					}
				}
			}
			return song->getWarnings().join("\n");
		}
		case Qt::TextAlignmentRole:
		{
			switch (aIndex.column())
			{
				case colNumMatchingFilters:
				case colNumDuplicates:
				case colLength:
				case colManualMeasuresPerMinute:
				case colId3MeasuresPerMinute:
				case colFileNameMeasuresPerMinute:
				case colDetectedTempo:
				case colSkipStart:
				{
					return Qt::AlignRight;
				}
			}
			return Qt::AlignLeft;
		}
	}
	return QVariant();
}





QVariant SongModel::headerData(int aSection, Qt::Orientation aOrientation, int aRole) const
{
	if (aOrientation != Qt::Horizontal)
	{
		return QVariant();
	}
	if (aRole != Qt::DisplayRole)
	{
		return QVariant();
	}
	switch (aSection)
	{
		case colFileName:                  return tr("File name");
		case colLength:                    return tr("Length");
		case colLastPlayed:                return tr("Last played");
		case colLocalRating:               return tr("Local rating");
		case colRating:                    return tr("Community rating");
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
		case colDetectedTempo:             return tr("Detected tempo");
		case colNumMatchingFilters:        return tr("# flt", "Num matching favorite filters");
		case colNumDuplicates:             return tr("# dup", "Num duplicates");
		case colSkipStart:                 return tr("Skip-start");
	}
	return QVariant();
}





Qt::ItemFlags SongModel::flags(const QModelIndex & aIndex) const
{
	if (aIndex.isValid())
	{
		switch (aIndex.column())
		{
			case colLocalRating:
			case colManualAuthor:
			case colManualGenre:
			case colManualMeasuresPerMinute:
			case colManualTitle:
			{
				// Editable:
				return Super::flags(aIndex) | Qt::ItemIsEditable;
			}
			default:
			{
				// Not editable
				return Super::flags(aIndex);
			}
		}
	}
	else
	{
		return Super::flags(aIndex);
	}
}





bool SongModel::setData(const QModelIndex & aIndex, const QVariant & aValue, int aRole)
{
	if ((aRole != Qt::DisplayRole) && (aRole != Qt::EditRole))
	{
		return false;
	}
	if (
		!aIndex.isValid() ||
		(aIndex.row() < 0) ||
		(aIndex.row() >= static_cast<int>(mDB.songs().size()))
	)
	{
		return false;
	}
	const auto & song = songFromRow(aIndex.row());
	switch (aIndex.column())
	{
		case colFileName: return false;
		case colLastPlayed: return false;
		case colLength: return false;
		case colRating: return false;
		case colLocalRating: song->setLocalRating(Utils::clamp(aValue.toDouble(), 0.0, 5.0)); break;
		case colManualAuthor: song->setAuthor(aValue.toString()); break;
		case colManualTitle: song->setTitle(aValue.toString()); break;
		case colManualGenre: song->setGenre(aValue.toString()); break;
		case colManualMeasuresPerMinute: song->setMeasuresPerMinute(aValue.toDouble()); break;
	}
	emit songEdited(song);
	emit dataChanged(aIndex, aIndex, {aRole});
	return true;
}





qulonglong SongModel::numMatchingFilters(SongPtr aSong) const
{
	qulonglong res = 0;
	for (const auto & filter: mDB.getFavoriteFilters())
	{
		if (filter->rootNode()->isSatisfiedBy(*aSong))
		{
			res += 1;
		}
	}
	return res;
}





qulonglong SongModel::numDuplicates(SongPtr aSong) const
{
	auto sd = aSong->sharedData();
	if (sd == nullptr)
	{
		return 1;
	}
	return static_cast<qulonglong>(sd->duplicatesCount());
}





void SongModel::addSongFile(SongPtr aNewSong)
{
	Q_UNUSED(aNewSong);

	auto lastIdx = static_cast<int>(mDB.songs().size()) - 1;
	beginInsertRows(QModelIndex(), lastIdx, lastIdx);
	endInsertRows();
}





void SongModel::delSong(SongPtr aSong, size_t aIndex)
{
	Q_UNUSED(aSong);

	auto idx = static_cast<int>(aIndex);
	beginRemoveRows(QModelIndex(), idx, idx);
	endRemoveRows();
}





void SongModel::songChanged(SongPtr aSong)
{
	auto idx = indexFromSong(aSong.get());
	if (!idx.isValid())
	{
		return;
	}
	auto idx2 = createIndex(idx.row(), colMax - 1);
	emit dataChanged(idx, idx2, {Qt::DisplayRole});
}





////////////////////////////////////////////////////////////////////////////////
// SongModelEditorDelegate:

SongModelEditorDelegate::SongModelEditorDelegate(QWidget * aParent):
	Super(aParent)
{
}





QWidget * SongModelEditorDelegate::createEditor(
	QWidget * aParent,
	const QStyleOptionViewItem & aOption,
	const QModelIndex & aIndex
) const
{
	if (!aIndex.isValid())
	{
		return nullptr;
	}
	switch (aIndex.column())
	{
		case SongModel::colManualAuthor:
		case SongModel::colManualTitle:
		{
			// Default text editor:
			return Super::createEditor(aParent, aOption, aIndex);
		}
		case SongModel::colManualGenre:
		{
			// Special editor:
			auto res = new QComboBox(aParent);
			res->setFrame(false);
			res->addItems(Song::recognizedGenres());
			res->setMaxVisibleItems(res->count());
			return res;
		}
		case SongModel::colManualMeasuresPerMinute:
		case SongModel::colLocalRating:
		{
			// Text editor limited to entering numbers:
			auto res = new QLineEdit(aParent);
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
	QWidget * aEditor,
	const QModelIndex & aIndex
) const
{
	if (!aIndex.isValid())
	{
		return;
	}
	switch (aIndex.column())
	{
		case SongModel::colManualAuthor:
		case SongModel::colManualTitle:
		{
			// Default text editor:
			Super::setEditorData(aEditor, aIndex);
			return;
		}
		case SongModel::colManualGenre:
		{
			// Special editor:
			auto cb = qobject_cast<QComboBox *>(aEditor);
			assert(cb != nullptr);
			cb->setCurrentText(aIndex.data().toString());
			return;
		}
		case SongModel::colManualMeasuresPerMinute:
		case SongModel::colLocalRating:
		{
			auto le = qobject_cast<QLineEdit *>(aEditor);
			assert(le != nullptr);
			le->setText(aIndex.data().toString());
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
	QWidget * aEditor,
	QAbstractItemModel * aModel,
	const QModelIndex & aIndex
) const
{
	if (!aIndex.isValid())
	{
		return;
	}
	switch (aIndex.column())
	{
		case SongModel::colManualAuthor:
		case SongModel::colManualTitle:
		{
			// Default text editor:
			Super::setModelData(aEditor, aModel, aIndex);
			return;
		}
		case SongModel::colManualGenre:
		{
			// Special editor:
			auto cb = qobject_cast<QComboBox *>(aEditor);
			assert(cb != nullptr);
			aModel->setData(aIndex, cb->currentText());
			return;
		}
		case SongModel::colManualMeasuresPerMinute:
		case SongModel::colLocalRating:
		{
			auto le = qobject_cast<QLineEdit *>(aEditor);
			assert(le != nullptr);
			aModel->setData(aIndex, le->locale().toDouble(le->text()));
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

SongModelFilter::SongModelFilter(SongModel & aParentModel):
	mParentModel(aParentModel),
	mFilter(fltNone),
	mSearchString("", QRegularExpression::CaseInsensitiveOption)
{
	setSourceModel(&mParentModel);
}





void SongModelFilter::setFilter(SongModelFilter::EFilter aFilter)
{
	if (mFilter == aFilter)
	{
		return;
	}
	mFilter = aFilter;
	STOPWATCH("setFilter");
	invalidateFilter();
}





void SongModelFilter::setSearchString(const QString & aSearchString)
{
	mSearchString.setPattern(aSearchString);
	STOPWATCH("setSearchString");
	invalidateFilter();
}





void SongModelFilter::setFavoriteFilters(const std::vector<FilterPtr> & aFavoriteFilters)
{
	mFavoriteFilters = aFavoriteFilters;
	STOPWATCH("setFavoriteTemplateItems");
	invalidateFilter();
}





bool SongModelFilter::filterAcceptsRow(int aSrcRow, const QModelIndex & aSrcParent) const
{
	assert(!aSrcParent.isValid());
	Q_UNUSED(aSrcParent);  // For release builds

	auto song = mParentModel.songFromRow(aSrcRow);
	if (song == nullptr)
	{
		qWarning() << "Got a row without an assigned song: " << aSrcRow;
		assert(!"Got a row without an assigned song");
		return false;
	}
	switch (mFilter)
	{
		case fltNone: break;
		case fltNoId3:
		{
			const auto & id3Tag = song->tagId3();
			if (
				(id3Tag.mAuthor.isEmpty()) &&
				(id3Tag.mTitle.isEmpty()) &&
				(id3Tag.mGenre.isEmpty()) &&
				!id3Tag.mMeasuresPerMinute.isPresent()
			)
			{
				break;
			}
			return false;  // Don't want songs with valid ID3 tag
		}  // case fltNoId3

		case fltNoGenre:
		{
			const auto & genre = song->primaryGenre();
			if (genre.isEmpty())
			{
				break;
			}
			return false;  // Don't want songs with valid genre
		}  // case fltNoGenre

		case fltNoMeasuresPerMinute:
		{
			if (!song->primaryMeasuresPerMinute().isPresent())
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

		case fltNoFilterMatch:
		{
			for (const auto & filter: mFavoriteFilters)
			{
				if (filter->rootNode()->isSatisfiedBy(*song))
				{
					return false;  // Don't want songs matching a template filter
				}
			}
			break;
		}  // case fltNoTemplateFilterMatch

		case fltDuplicates:
		{
			if (song->duplicates().size() <= 1)
			{
				return false;  // Don't want duplicates
			}
			break;
		}  // case fltDuplicates

		case fltSkipStart:
		{
			if (!song->skipStart().isPresent())
			{
				return false;  // Don't want without skip-start
			}
			break;
		}  // case fltSkipStart

		case fltFailedTempoDetect:
		{
			if (
				!song->detectedTempo().isPresent() ||          // Tempo not detected yet
				!song->primaryMeasuresPerMinute().isPresent()  // Tempo not present in tag
			)
			{
				return false;  // Cannot compare tempo, don't show such song
			}
			auto tagTempo = song->primaryMeasuresPerMinute().value();
			if (std::abs(tagTempo - song->detectedTempo().value()) < 1)
			{
				return false;  // Don't want songs with matching tempo
			}
		}  // case fltFailedTempoDetect
	}
	return songMatchesSearchString(song);
}





bool SongModelFilter::songMatchesSearchString(SongPtr aSong) const
{
	if (
		mSearchString.match(aSong->tagManual().mAuthor.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagManual().mTitle.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagManual().mGenre.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagId3().mAuthor.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagId3().mTitle.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagId3().mGenre.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagFileName().mAuthor.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagFileName().mTitle.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->tagFileName().mGenre.valueOrDefault()).hasMatch() ||
		mSearchString.match(aSong->fileName()).hasMatch()
	)
	{
		return true;
	}
	return false;
}

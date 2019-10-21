#include "Filter.hpp"
#include <cassert>
#include <QCryptographicHash>
#include "Song.hpp"





/** The difference that is still considered equal when comparing two numerical property values in a Filter. */
static const double EPS = 0.000001;





////////////////////////////////////////////////////////////////////////////////
// Filter::Node:

Filter::Node::Node(
	Filter::Node::SongProperty aSongProperty,
	Filter::Node::Comparison aComparison,
	QVariant aValue
):
	mParent(nullptr),
	mKind(nkComparison),
	mSongProperty(aSongProperty),
	mComparison(aComparison),
	mValue(aValue)
{
}





Filter::Node::Node(
	Filter::Node::Kind aCombination,
	const std::vector<Filter::NodePtr> aSubFilters
):
	mParent(nullptr),
	mKind(aCombination),
	mChildren(aSubFilters)
{
	assert(canHaveChildren());  // This constructor can only create children-able filters
}





Filter::NodePtr Filter::Node::clone() const
{
	if (canHaveChildren())
	{
		auto res = std::make_shared<Filter::Node>(mKind, std::vector<Filter::NodePtr>());
		for (const auto & ch: mChildren)
		{
			res->addChild(ch->clone());
		}
		return res;
	}
	else
	{
		return std::make_shared<Filter::Node>(mSongProperty, mComparison, mValue);
	}
}





const std::vector<Filter::NodePtr> & Filter::Node::children() const
{
	assert(canHaveChildren());
	return mChildren;
}





Filter::Node::SongProperty Filter::Node::songProperty() const
{
	assert(mKind == nkComparison);
	return mSongProperty;
}





Filter::Node::Comparison Filter::Node::comparison() const
{
	assert(mKind == nkComparison);
	return mComparison;
}





QVariant Filter::Node::value() const
{
	assert(mKind == nkComparison);
	return mValue;
}





void Filter::Node::setKind(Filter::Node::Kind aKind)
{
	if (mKind == aKind)
	{
		return;
	}
	mKind = aKind;
	if (!canHaveChildren())
	{
		mChildren.clear();
	}
}





void Filter::Node::setSongProperty(Filter::Node::SongProperty aSongProperty)
{
	assert(!canHaveChildren());
	mSongProperty = aSongProperty;
}





void Filter::Node::setComparison(Filter::Node::Comparison aComparison)
{
	assert(!canHaveChildren());
	mComparison = aComparison;
}





void Filter::Node::setValue(QVariant aValue)
{
	assert(!canHaveChildren());
	mValue = aValue;
}





bool Filter::Node::isSatisfiedBy(const Song & aSong) const
{
	switch (mKind)
	{
		case nkAnd:
		{
			for (const auto & ch: mChildren)
			{
				if (!ch->isSatisfiedBy(aSong))
				{
					return false;
				}
			}
			return true;
		}

		case nkOr:
		{
			for (const auto & ch: mChildren)
			{
				if (ch->isSatisfiedBy(aSong))
				{
					return true;
				}
			}
			return false;
		}

		case nkComparison:
		{
			return isComparisonSatisfiedBy(aSong);
		}
	}
	assert(!"Unknown filter kind");
	return false;
}





bool Filter::Node::canHaveChildren() const
{
	return ((mKind == nkAnd) || (mKind == nkOr));
}





void Filter::Node::addChild(Filter::NodePtr aChild)
{
	assert(canHaveChildren());
	assert(aChild != nullptr);
	mChildren.push_back(aChild);
	aChild->mParent = this;
}





void Filter::Node::replaceChild(Filter::Node * aExistingChild, Filter::NodePtr aNewChild)
{
	assert(canHaveChildren());
	assert(aNewChild != nullptr);
	for (auto itr = mChildren.begin(), end = mChildren.end(); itr != end; ++itr)
	{
		if (itr->get() == aExistingChild)
		{
			aExistingChild->setParent(nullptr);
			*itr = aNewChild;
			aNewChild->setParent(this);
			return;
		}
	}
	assert(!"Attempted to replace a non-existent child.");
}





void Filter::Node::removeChild(const Filter::Node * aExistingChild)
{
	for (auto itr = mChildren.cbegin(), end = mChildren.cend(); itr != end; ++itr)
	{
		if (itr->get() == aExistingChild)
		{
			(*itr)->setParent(nullptr);
			mChildren.erase(itr);
			return;
		}
	}
	assert(!"Attempted to remove a non-existent child.");
}





void Filter::Node::checkConsistency() const
{
	if (!canHaveChildren())
	{
		return;
	}
	for (const auto & ch: children())
	{
		assert(ch->parent() == this);
		ch->checkConsistency();
	}
}





QString Filter::Node::getCaption() const
{
	switch (mKind)
	{
		case nkAnd: return Filter::tr("And", "FilterCaption");
		case nkOr:  return Filter::tr("Or",  "FilterCaption");
		case nkComparison:
		{
			return QString("%1 %2 %3").arg(
				songPropertyCaption(mSongProperty),
				comparisonCaption(mComparison),
				mValue.toString()
			);
		}
	}
	assert(!"Unknown node kind");
	return QString("<invalid node kind: %1>").arg(mKind);
}





QString Filter::Node::getDescription() const
{
	switch (mKind)
	{
		case nkAnd: return concatChildrenDescriptions(Filter::tr(" and ", "FilterConcatString"));
		case nkOr:  return concatChildrenDescriptions(Filter::tr(" or ",  "FilterConcatString"));
		case nkComparison: return QString("(%1)").arg(getCaption());
	}
	assert(!"Unknown node kind");
	return QString("<invalid node kind: %1>").arg(mKind);
}





Filter::Node::Kind Filter::Node::intToKind(int aKind)
{
	switch (aKind)
	{
		case nkComparison: return nkComparison;
		case nkAnd:        return nkAnd;
		case nkOr:         return nkOr;
	}
	throw std::runtime_error(QString("Unknown node Kind: %1").arg(aKind).toUtf8().constData());
}





Filter::Node::Comparison Filter::Node::intToComparison(int aComparison)
{
	switch (aComparison)
	{
		case ncEqual:              return ncEqual;
		case ncNotEqual:           return ncNotEqual;
		case ncContains:           return ncContains;
		case ncNotContains:        return ncNotContains;
		case ncGreaterThan:        return ncGreaterThan;
		case ncGreaterThanOrEqual: return ncGreaterThanOrEqual;
		case ncLowerThan:          return ncLowerThan;
		case ncLowerThanOrEqual:   return ncLowerThanOrEqual;
	}
	throw std::runtime_error(QString("Unknown node Comparison: %1").arg(aComparison).toUtf8().constData());
}





Filter::Node::SongProperty Filter::Node::intToSongProperty(int aSongProperty)
{
	switch (aSongProperty)
	{
		case nspAuthor:                    return nspAuthor;
		case nspTitle:                     return nspTitle;
		case nspGenre:                     return nspGenre;
		case nspLength:                    return nspLength;
		case nspMeasuresPerMinute:         return nspMeasuresPerMinute;
		case nspLocalRating:               return nspLocalRating;
		case nspLastPlayed:                return nspLastPlayed;
		case nspManualAuthor:              return nspManualAuthor;
		case nspManualTitle:               return nspManualTitle;
		case nspManualGenre:               return nspManualGenre;
		case nspManualMeasuresPerMinute:   return nspManualMeasuresPerMinute;
		case nspFileNameAuthor:            return nspFileNameAuthor;
		case nspFileNameTitle:             return nspFileNameTitle;
		case nspFileNameGenre:             return nspFileNameGenre;
		case nspFileNameMeasuresPerMinute: return nspFileNameMeasuresPerMinute;
		case nspId3Author:                 return nspId3Author;
		case nspId3Title:                  return nspId3Title;
		case nspId3Genre:                  return nspId3Genre;
		case nspId3MeasuresPerMinute:      return nspId3MeasuresPerMinute;
		case nspPrimaryAuthor:             return nspPrimaryAuthor;
		case nspPrimaryTitle:              return nspPrimaryTitle;
		case nspPrimaryGenre:              return nspPrimaryGenre;
		case nspPrimaryMeasuresPerMinute:  return nspPrimaryMeasuresPerMinute;
		case nspWarningCount:              return nspWarningCount;
		case nspRatingRhythmClarity:       return nspRatingRhythmClarity;
		case nspRatingGenreTypicality:     return nspRatingGenreTypicality;
		case nspRatingPopularity:          return nspRatingPopularity;
		case nspNotes:                     return nspNotes;
		case nspDetectedTempo:             return nspDetectedTempo;
	}
	throw std::runtime_error(QString("Unknown node SongProperty: %1").arg(aSongProperty).toUtf8().constData());
}





QString Filter::Node::songPropertyCaption(Filter::Node::SongProperty aProp)
{
	switch (aProp)
	{
		case nspAuthor:                    return Filter::tr("Author [any]",            "SongPropertyCaption");
		case nspTitle:                     return Filter::tr("Title [any]",             "SongPropertyCaption");
		case nspGenre:                     return Filter::tr("Genre [any]",             "SongPropertyCaption");
		case nspLength:                    return Filter::tr("Length",                  "SongPropertyCaption");
		case nspMeasuresPerMinute:         return Filter::tr("MPM [any]",               "SongPropertyCaption");
		case nspLocalRating:               return Filter::tr("Local Rating",            "SongPropertyCaption");
		case nspLastPlayed:                return Filter::tr("LastPlayed",              "SongPropertyCaption");
		case nspManualAuthor:              return Filter::tr("Author [M]",              "SongPropertyCaption");
		case nspManualTitle:               return Filter::tr("Title [M]",               "SongPropertyCaption");
		case nspManualGenre:               return Filter::tr("Genre [M]",               "SongPropertyCaption");
		case nspManualMeasuresPerMinute:   return Filter::tr("MPM [M]",                 "SongPropertyCaption");
		case nspFileNameAuthor:            return Filter::tr("Author [F]",              "SongPropertyCaption");
		case nspFileNameTitle:             return Filter::tr("Title [F]",               "SongPropertyCaption");
		case nspFileNameGenre:             return Filter::tr("Genre [F]",               "SongPropertyCaption");
		case nspFileNameMeasuresPerMinute: return Filter::tr("MPM [F]",                 "SongPropertyCaption");
		case nspId3Author:                 return Filter::tr("Author [T]",              "SongPropertyCaption");
		case nspId3Title:                  return Filter::tr("Title [T]",               "SongPropertyCaption");
		case nspId3Genre:                  return Filter::tr("Genre [T]",               "SongPropertyCaption");
		case nspId3MeasuresPerMinute:      return Filter::tr("MPM [T]",                 "SongPropertyCaption");
		case nspPrimaryAuthor:             return Filter::tr("Author [Primary]",        "SongPropertyCaption");
		case nspPrimaryTitle:              return Filter::tr("Title [Primary]",         "SongPropertyCaption");
		case nspPrimaryGenre:              return Filter::tr("Genre [Primary]",         "SongPropertyCaption");
		case nspPrimaryMeasuresPerMinute:  return Filter::tr("MPM [Primary]",           "SongPropertyCaption");
		case nspWarningCount:              return Filter::tr("Warning count",           "SongPropertyCaption");
		case nspRatingRhythmClarity:       return Filter::tr("Rhythm clarity rating",   "SongPropertyCaption");
		case nspRatingGenreTypicality:     return Filter::tr("Genre typicality rating", "SongPropertyCaption");
		case nspRatingPopularity:          return Filter::tr("Popularity rating",       "SongPropertyCaption");
		case nspNotes:                     return Filter::tr("Notes",                   "SongPropertyCaption");
		case nspDetectedTempo:             return Filter::tr("Detected tempo",          "SongPropertyCaption");
	}
	assert(!"Unknown node SongProperty");
	return QString();
}





QString Filter::Node::comparisonCaption(Filter::Node::Comparison aComparison)
{
	switch (aComparison)
	{
		case ncEqual:              return Filter::tr("==",              "Comparison");
		case ncNotEqual:           return Filter::tr("!=",              "Comparison");
		case ncContains:           return Filter::tr("contains",        "Comparison");
		case ncNotContains:        return Filter::tr("doesn't contain", "Comparison");
		case ncGreaterThan:        return Filter::tr(">",               "Comparison");
		case ncGreaterThanOrEqual: return Filter::tr(">=",              "Comparison");
		case ncLowerThan:          return Filter::tr("<",               "Comparison");
		case ncLowerThanOrEqual:   return Filter::tr("<=",              "Comparison");
	}
	assert(!"Unknown node Comparison");
	return QString();
}





QString Filter::Node::concatChildrenDescriptions(const QString & aSeparator) const
{
	// Special case: single children get direct description without the parenthesis:
	if (mChildren.size() == 1)
	{
		return mChildren[0]->getDescription();
	}

	// Concat the children descriptions:
	QString res("(");
	bool isFirst = true;
	for (const auto & ch: mChildren)
	{
		if (!isFirst)
		{
			res.append(aSeparator);
		}
		isFirst = false;
		res.append(ch->getDescription());
	}
	res.append(")");
	return res;
}





bool Filter::Node::isComparisonSatisfiedBy(const Song & aSong) const
{
	assert(mKind == nkComparison);
	switch (mSongProperty)
	{
		case nspAuthor:
		{
			return (
				isStringComparisonSatisfiedBy(aSong.tagManual().mAuthor) ||
				isStringComparisonSatisfiedBy(aSong.tagFileName().mAuthor) ||
				isStringComparisonSatisfiedBy(aSong.tagId3().mAuthor)
			);
		}
		case nspTitle:
		{
			return (
				isStringComparisonSatisfiedBy(aSong.tagManual().mTitle) ||
				isStringComparisonSatisfiedBy(aSong.tagFileName().mTitle) ||
				isStringComparisonSatisfiedBy(aSong.tagId3().mTitle)
			);
		}
		case nspGenre:
		{
			return (
				isStringComparisonSatisfiedBy(aSong.tagManual().mGenre) ||
				isStringComparisonSatisfiedBy(aSong.tagFileName().mGenre) ||
				isStringComparisonSatisfiedBy(aSong.tagId3().mGenre)
			);
		}
		case nspMeasuresPerMinute:
		{
			return (
				isNumberComparisonSatisfiedBy(aSong.tagManual().mMeasuresPerMinute) ||
				isNumberComparisonSatisfiedBy(aSong.tagFileName().mMeasuresPerMinute) ||
				isNumberComparisonSatisfiedBy(aSong.tagId3().mMeasuresPerMinute) ||
				isNumberComparisonSatisfiedBy(aSong.detectedTempo())
			);
		}
		case nspManualAuthor:              return isStringComparisonSatisfiedBy(aSong.tagManual().mAuthor);
		case nspManualTitle:               return isStringComparisonSatisfiedBy(aSong.tagManual().mTitle);
		case nspManualGenre:               return isStringComparisonSatisfiedBy(aSong.tagManual().mGenre);
		case nspManualMeasuresPerMinute:   return isNumberComparisonSatisfiedBy(aSong.tagManual().mMeasuresPerMinute);
		case nspFileNameAuthor:            return isStringComparisonSatisfiedBy(aSong.tagFileName().mAuthor);
		case nspFileNameTitle:             return isStringComparisonSatisfiedBy(aSong.tagFileName().mTitle);
		case nspFileNameGenre:             return isStringComparisonSatisfiedBy(aSong.tagFileName().mGenre);
		case nspFileNameMeasuresPerMinute: return isNumberComparisonSatisfiedBy(aSong.tagFileName().mMeasuresPerMinute);
		case nspId3Author:                 return isStringComparisonSatisfiedBy(aSong.tagId3().mAuthor);
		case nspId3Title:                  return isStringComparisonSatisfiedBy(aSong.tagId3().mTitle);
		case nspId3Genre:                  return isStringComparisonSatisfiedBy(aSong.tagId3().mGenre);
		case nspId3MeasuresPerMinute:      return isNumberComparisonSatisfiedBy(aSong.tagId3().mMeasuresPerMinute);
		case nspLastPlayed:                return isDateComparisonSatisfiedBy(aSong.lastPlayed().valueOrDefault());
		case nspLength:                    return isNumberComparisonSatisfiedBy(aSong.length());
		case nspLocalRating:               return isNumberComparisonSatisfiedBy(aSong.rating().mLocal);
		case nspPrimaryAuthor:             return isStringComparisonSatisfiedBy(aSong.primaryAuthor());
		case nspPrimaryTitle:              return isStringComparisonSatisfiedBy(aSong.primaryTitle());
		case nspPrimaryGenre:              return isStringComparisonSatisfiedBy(aSong.primaryGenre());
		case nspPrimaryMeasuresPerMinute:
		{
			auto mpm = aSong.primaryMeasuresPerMinute();
			if (!mpm.isPresent())
			{
				mpm = aSong.detectedTempo();
			}
			return isNumberComparisonSatisfiedBy(mpm);
		}
		case nspWarningCount:              return isValidNumberComparisonSatisfiedBy(aSong.getWarnings().count());
		case nspRatingRhythmClarity:       return isNumberComparisonSatisfiedBy(aSong.rating().mRhythmClarity);
		case nspRatingGenreTypicality:     return isNumberComparisonSatisfiedBy(aSong.rating().mGenreTypicality);
		case nspRatingPopularity:          return isNumberComparisonSatisfiedBy(aSong.rating().mPopularity);
		case nspNotes:                     return isStringComparisonSatisfiedBy(aSong.notes());
		case nspDetectedTempo:             return isNumberComparisonSatisfiedBy(aSong.detectedTempo());
	}
	assert(!"Unknown song property in comparison");
	return false;
}





bool Filter::Node::isStringComparisonSatisfiedBy(const DatedOptional<QString> & aValue) const
{
	assert(mKind == nkComparison);

	// Empty strings satisfy only the fcNotContains criterion:
	if (aValue.isEmpty())
	{
		return (mComparison == ncNotContains);
	}

	// String is valid, compare:
	switch (mComparison)
	{
		case ncContains:           return  aValue.value().contains(mValue.toString(), Qt::CaseInsensitive);
		case ncNotContains:        return !aValue.value().contains(mValue.toString(), Qt::CaseInsensitive);
		case ncEqual:              return  (aValue.value().compare(mValue.toString(), Qt::CaseInsensitive) == 0);
		case ncNotEqual:           return  (aValue.value().compare(mValue.toString(), Qt::CaseInsensitive) != 0);
		case ncGreaterThan:        return  (aValue.value().compare(mValue.toString(), Qt::CaseInsensitive) >  0);
		case ncGreaterThanOrEqual: return  (aValue.value().compare(mValue.toString(), Qt::CaseInsensitive) >= 0);
		case ncLowerThan:          return  (aValue.value().compare(mValue.toString(), Qt::CaseInsensitive) <  0);
		case ncLowerThanOrEqual:   return  (aValue.value().compare(mValue.toString(), Qt::CaseInsensitive) <= 0);
	}
	assert(!"Unknown comparison");
	return false;
}





bool Filter::Node::isNumberComparisonSatisfiedBy(const DatedOptional<double> & aValue) const
{
	assert(mKind == nkComparison);

	// Check if the passed value is a valid number:
	if (!aValue.isPresent())
	{
		return false;
	}

	// Check with the regular valid number logic:
	auto value = aValue.value();
	return isValidNumberComparisonSatisfiedBy(value);
}





bool Filter::Node::isNumberComparisonSatisfiedBy(const QVariant & aValue) const
{
	assert(mKind == nkComparison);

	// Check if the passed value is a valid number:
	bool isOK;
	auto value = aValue.toDouble(&isOK);
	if (!isOK)
	{
		return false;
	}

	// Check with the regular valid number logic:
	return isValidNumberComparisonSatisfiedBy(value);
}




bool Filter::Node::isValidNumberComparisonSatisfiedBy(double aValue) const
{
	assert(mKind == nkComparison);

	// For string-based comparison, use the filter value as a string:
	switch (mComparison)
	{
		case ncContains:           return  QString::number(aValue).contains(mValue.toString(), Qt::CaseInsensitive);
		case ncNotContains:        return !QString::number(aValue).contains(mValue.toString(), Qt::CaseInsensitive);
		default: break;
	}

	// For number-based comparisons, compare to a number; fail if NaN:
	bool isOk;
	auto cmpVal = mValue.toDouble(&isOk);
	if (!isOk)
	{
		return false;
	}
	switch (mComparison)
	{
		case ncEqual:              return (std::abs(aValue - cmpVal) < EPS);
		case ncNotEqual:           return (std::abs(aValue - cmpVal) >= EPS);
		case ncGreaterThan:        return (aValue >  cmpVal);
		case ncGreaterThanOrEqual: return (aValue >= cmpVal);
		case ncLowerThan:          return (aValue <  cmpVal);
		case ncLowerThanOrEqual:   return (aValue <= cmpVal);
		default: break;
	}
	assert(!"Unknown comparison");
	return false;
}





bool Filter::Node::isDateComparisonSatisfiedBy(QDateTime aValue) const
{
	assert(mKind == nkComparison);
	switch (mComparison)
	{
		case ncContains:           return  aValue.toString(QLocale().dateTimeFormat()).contains(mValue.toString(), Qt::CaseInsensitive);
		case ncNotContains:        return !aValue.toString(QLocale().dateTimeFormat()).contains(mValue.toString(), Qt::CaseInsensitive);
		case ncEqual:              return (aValue == mValue.toDateTime());
		case ncNotEqual:           return (aValue != mValue.toDateTime());
		case ncGreaterThan:        return (aValue >  mValue.toDateTime());
		case ncGreaterThanOrEqual: return (aValue >= mValue.toDateTime());
		case ncLowerThan:          return (aValue <  mValue.toDateTime());
		case ncLowerThanOrEqual:   return (aValue <= mValue.toDateTime());
	}
	assert(!"Unknown comparison");
	return false;
}





QByteArray Filter::Node::hash() const
{
	QCryptographicHash h(QCryptographicHash::Sha1);
	h.addData(reinterpret_cast<const char *>(&mKind), sizeof(mKind));
	switch (mKind)
	{
		case nkComparison:
		{
			h.addData(reinterpret_cast<const char *>(&mSongProperty), sizeof(mSongProperty));
			h.addData(reinterpret_cast<const char *>(&mComparison), sizeof(mComparison));
			h.addData(mValue.toString().toUtf8());
			break;
		}
		case nkAnd:
		case nkOr:
		{
			for (const auto & ch: mChildren)
			{
				h.addData(ch->hash());
			}
			break;
		}
	}
	return h.result();
}





////////////////////////////////////////////////////////////////////////////////
// Filter:

Filter::Filter():
	mDbRowId(-1),
	mIsFavorite(false),
	mBgColor(255, 255, 255)
{
	setNoopFilter();
}





Filter::Filter(
	qlonglong aDbRowId,
	const QString & aDisplayName,
	const QString & aNotes,
	bool aIsFavorite,
	QColor aBgColor,
	const DatedOptional<double> & aDurationLimit
):
	mDbRowId(aDbRowId),
	mDisplayName(aDisplayName),
	mNotes(aNotes),
	mIsFavorite(aIsFavorite),
	mBgColor(aBgColor),
	mDurationLimit(aDurationLimit)
{
	setNoopFilter();
}






Filter::Filter(const Filter & aCopyFrom):
	QObject(),
	std::enable_shared_from_this<Filter>(),
	mDbRowId(-1),
	mDisplayName(aCopyFrom.mDisplayName),
	mNotes(aCopyFrom.mNotes),
	mIsFavorite(aCopyFrom.mIsFavorite),
	mRootNode(aCopyFrom.mRootNode->clone()),
	mBgColor(aCopyFrom.mBgColor),
	mDurationLimit(aCopyFrom.mDurationLimit)
{
}





void Filter::setNoopFilter()
{
	mRootNode.reset(new Node(
		Filter::Node::nspLength,
		Filter::Node::ncGreaterThanOrEqual,
		0
	));
}





void Filter::checkNodeConsistency() const
{
	if (mRootNode == nullptr)
	{
		return;
	}
	assert(mRootNode->parent() == nullptr);
	mRootNode->checkConsistency();
}





QByteArray Filter::hash() const
{
	QCryptographicHash h(QCryptographicHash::Sha1);
	h.addData(mDisplayName.toUtf8());
	h.addData(mNotes.toUtf8());
	h.addData(mIsFavorite ? "F" : "N", 1);
	h.addData(mRootNode->hash());
	h.addData(mBgColor.name().toUtf8());
	h.addData(mDurationLimit.isPresent() ? "L" : "N", 1);
	if (mDurationLimit.isPresent())
	{
		auto lim = mDurationLimit.value();
		h.addData(reinterpret_cast<const char *>(&lim), sizeof(lim));
	}
	return h.result();
}





void Filter::setDbRowId(qlonglong aDbRowId)
{
	assert(mDbRowId == -1);
	mDbRowId = aDbRowId;
}






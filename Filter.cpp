#include "Filter.h"
#include <cassert>
#include <QCryptographicHash>
#include "Song.h"





/** The difference that is still considered equal when comparing two numerical property values in a Filter. */
static const double EPS = 0.000001;





////////////////////////////////////////////////////////////////////////////////
// Filter::Node:

Filter::Node::Node(
	Filter::Node::SongProperty a_SongProperty,
	Filter::Node::Comparison a_Comparison,
	QVariant a_Value
):
	m_Parent(nullptr),
	m_Kind(nkComparison),
	m_SongProperty(a_SongProperty),
	m_Comparison(a_Comparison),
	m_Value(a_Value)
{
}





Filter::Node::Node(
	Filter::Node::Kind a_Combination,
	const std::vector<Filter::NodePtr> a_SubFilters
):
	m_Parent(nullptr),
	m_Kind(a_Combination),
	m_Children(a_SubFilters)
{
	assert(canHaveChildren());  // This constructor can only create children-able filters
}





Filter::NodePtr Filter::Node::clone() const
{
	if (canHaveChildren())
	{
		auto res = std::make_shared<Filter::Node>(m_Kind, std::vector<Filter::NodePtr>());
		for (const auto & ch: m_Children)
		{
			res->addChild(ch->clone());
		}
		return res;
	}
	else
	{
		return std::make_shared<Filter::Node>(m_SongProperty, m_Comparison, m_Value);
	}
}





const std::vector<Filter::NodePtr> & Filter::Node::children() const
{
	assert(canHaveChildren());
	return m_Children;
}





Filter::Node::SongProperty Filter::Node::songProperty() const
{
	assert(m_Kind == nkComparison);
	return m_SongProperty;
}





Filter::Node::Comparison Filter::Node::comparison() const
{
	assert(m_Kind == nkComparison);
	return m_Comparison;
}





QVariant Filter::Node::value() const
{
	assert(m_Kind == nkComparison);
	return m_Value;
}





void Filter::Node::setKind(Filter::Node::Kind a_Kind)
{
	if (m_Kind == a_Kind)
	{
		return;
	}
	m_Kind = a_Kind;
	if (!canHaveChildren())
	{
		m_Children.clear();
	}
}





void Filter::Node::setSongProperty(Filter::Node::SongProperty a_SongProperty)
{
	assert(!canHaveChildren());
	m_SongProperty = a_SongProperty;
}





void Filter::Node::setComparison(Filter::Node::Comparison a_Comparison)
{
	assert(!canHaveChildren());
	m_Comparison = a_Comparison;
}





void Filter::Node::setValue(QVariant a_Value)
{
	assert(!canHaveChildren());
	m_Value = a_Value;
}





bool Filter::Node::isSatisfiedBy(const Song & a_Song) const
{
	switch (m_Kind)
	{
		case nkAnd:
		{
			for (const auto & ch: m_Children)
			{
				if (!ch->isSatisfiedBy(a_Song))
				{
					return false;
				}
			}
			return true;
		}

		case nkOr:
		{
			for (const auto & ch: m_Children)
			{
				if (ch->isSatisfiedBy(a_Song))
				{
					return true;
				}
			}
			return false;
		}

		case nkComparison:
		{
			return isComparisonSatisfiedBy(a_Song);
		}
	}
	assert(!"Unknown filter kind");
	return false;
}





bool Filter::Node::canHaveChildren() const
{
	return ((m_Kind == nkAnd) || (m_Kind == nkOr));
}





void Filter::Node::addChild(Filter::NodePtr a_Child)
{
	assert(canHaveChildren());
	assert(a_Child != nullptr);
	m_Children.push_back(a_Child);
	a_Child->m_Parent = this;
}





void Filter::Node::replaceChild(Filter::Node * a_ExistingChild, Filter::NodePtr a_NewChild)
{
	assert(canHaveChildren());
	assert(a_NewChild != nullptr);
	for (auto itr = m_Children.begin(), end = m_Children.end(); itr != end; ++itr)
	{
		if (itr->get() == a_ExistingChild)
		{
			a_ExistingChild->setParent(nullptr);
			*itr = a_NewChild;
			a_NewChild->setParent(this);
			return;
		}
	}
	assert(!"Attempted to replace a non-existent child.");
}





void Filter::Node::removeChild(const Filter::Node * a_ExistingChild)
{
	for (auto itr = m_Children.cbegin(), end = m_Children.cend(); itr != end; ++itr)
	{
		if (itr->get() == a_ExistingChild)
		{
			(*itr)->setParent(nullptr);
			m_Children.erase(itr);
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
	switch (m_Kind)
	{
		case nkAnd: return Filter::tr("And", "FilterCaption");
		case nkOr:  return Filter::tr("Or",  "FilterCaption");
		case nkComparison:
		{
			return QString("%1 %2 %3").arg(
				songPropertyCaption(m_SongProperty),
				comparisonCaption(m_Comparison),
				m_Value.toString()
			);
		}
	}
	assert(!"Unknown node kind");
	return QString("<invalid node kind: %1>").arg(m_Kind);
}





QString Filter::Node::getDescription() const
{
	switch (m_Kind)
	{
		case nkAnd: return concatChildrenDescriptions(Filter::tr(" and ", "FilterConcatString"));
		case nkOr:  return concatChildrenDescriptions(Filter::tr(" or ",  "FilterConcatString"));
		case nkComparison: return QString("(%1)").arg(getCaption());
	}
	assert(!"Unknown node kind");
	return QString("<invalid node kind: %1>").arg(m_Kind);
}





Filter::Node::Kind Filter::Node::intToKind(int a_Kind)
{
	switch (a_Kind)
	{
		case nkComparison: return nkComparison;
		case nkAnd:        return nkAnd;
		case nkOr:         return nkOr;
	}
	throw std::runtime_error(QString("Unknown node Kind: %1").arg(a_Kind).toUtf8().constData());
}





Filter::Node::Comparison Filter::Node::intToComparison(int a_Comparison)
{
	switch (a_Comparison)
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
	throw std::runtime_error(QString("Unknown node Comparison: %1").arg(a_Comparison).toUtf8().constData());
}





Filter::Node::SongProperty Filter::Node::intToSongProperty(int a_SongProperty)
{
	switch (a_SongProperty)
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
	}
	throw std::runtime_error(QString("Unknown node SongProperty: %1").arg(a_SongProperty).toUtf8().constData());
}





QString Filter::Node::songPropertyCaption(Filter::Node::SongProperty a_Prop)
{
	switch (a_Prop)
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
	}
	assert(!"Unknown node SongProperty");
	return QString();
}





QString Filter::Node::comparisonCaption(Filter::Node::Comparison a_Comparison)
{
	switch (a_Comparison)
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





QString Filter::Node::concatChildrenDescriptions(const QString & a_Separator) const
{
	// Special case: single children get direct description without the parenthesis:
	if (m_Children.size() == 1)
	{
		return m_Children[0]->getDescription();
	}

	// Concat the children descriptions:
	QString res("(");
	bool isFirst = true;
	for (const auto & ch: m_Children)
	{
		if (!isFirst)
		{
			res.append(a_Separator);
		}
		isFirst = false;
		res.append(ch->getDescription());
	}
	res.append(")");
	return res;
}





bool Filter::Node::isComparisonSatisfiedBy(const Song & a_Song) const
{
	assert(m_Kind == nkComparison);
	switch (m_SongProperty)
	{
		case nspAuthor:
		{
			return (
				isStringComparisonSatisfiedBy(a_Song.tagManual().m_Author) ||
				isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Author) ||
				isStringComparisonSatisfiedBy(a_Song.tagId3().m_Author)
			);
		}
		case nspTitle:
		{
			return (
				isStringComparisonSatisfiedBy(a_Song.tagManual().m_Title) ||
				isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Title) ||
				isStringComparisonSatisfiedBy(a_Song.tagId3().m_Title)
			);
		}
		case nspGenre:
		{
			return (
				isStringComparisonSatisfiedBy(a_Song.tagManual().m_Genre) ||
				isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Genre) ||
				isStringComparisonSatisfiedBy(a_Song.tagId3().m_Genre)
			);
		}
		case nspMeasuresPerMinute:
		{
			return (
				isNumberComparisonSatisfiedBy(a_Song.tagManual().m_MeasuresPerMinute) ||
				isNumberComparisonSatisfiedBy(a_Song.tagFileName().m_MeasuresPerMinute) ||
				isNumberComparisonSatisfiedBy(a_Song.tagId3().m_MeasuresPerMinute)
			);
		}
		case nspManualAuthor:              return isStringComparisonSatisfiedBy(a_Song.tagManual().m_Author);
		case nspManualTitle:               return isStringComparisonSatisfiedBy(a_Song.tagManual().m_Title);
		case nspManualGenre:               return isStringComparisonSatisfiedBy(a_Song.tagManual().m_Genre);
		case nspManualMeasuresPerMinute:   return isNumberComparisonSatisfiedBy(a_Song.tagManual().m_MeasuresPerMinute);
		case nspFileNameAuthor:            return isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Author);
		case nspFileNameTitle:             return isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Title);
		case nspFileNameGenre:             return isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Genre);
		case nspFileNameMeasuresPerMinute: return isNumberComparisonSatisfiedBy(a_Song.tagFileName().m_MeasuresPerMinute);
		case nspId3Author:                 return isStringComparisonSatisfiedBy(a_Song.tagId3().m_Author);
		case nspId3Title:                  return isStringComparisonSatisfiedBy(a_Song.tagId3().m_Title);
		case nspId3Genre:                  return isStringComparisonSatisfiedBy(a_Song.tagId3().m_Genre);
		case nspId3MeasuresPerMinute:      return isNumberComparisonSatisfiedBy(a_Song.tagId3().m_MeasuresPerMinute);
		case nspLastPlayed:                return isDateComparisonSatisfiedBy(a_Song.lastPlayed().valueOrDefault());
		case nspLength:                    return isNumberComparisonSatisfiedBy(a_Song.length());
		case nspLocalRating:               return isNumberComparisonSatisfiedBy(a_Song.rating().m_Local);
		case nspPrimaryAuthor:             return isStringComparisonSatisfiedBy(a_Song.primaryAuthor());
		case nspPrimaryTitle:              return isStringComparisonSatisfiedBy(a_Song.primaryTitle());
		case nspPrimaryGenre:              return isStringComparisonSatisfiedBy(a_Song.primaryGenre());
		case nspPrimaryMeasuresPerMinute:  return isNumberComparisonSatisfiedBy(a_Song.primaryMeasuresPerMinute());
		case nspWarningCount:              return isValidNumberComparisonSatisfiedBy(a_Song.getWarnings().count());
		case nspRatingRhythmClarity:       return isNumberComparisonSatisfiedBy(a_Song.rating().m_RhythmClarity);
		case nspRatingGenreTypicality:     return isNumberComparisonSatisfiedBy(a_Song.rating().m_GenreTypicality);
		case nspRatingPopularity:          return isNumberComparisonSatisfiedBy(a_Song.rating().m_Popularity);
		case nspNotes:                     return isStringComparisonSatisfiedBy(a_Song.notes());
	}
	assert(!"Unknown song property in comparison");
	return false;
}





bool Filter::Node::isStringComparisonSatisfiedBy(const DatedOptional<QString> & a_Value) const
{
	assert(m_Kind == nkComparison);

	// Empty strings satisfy only the fcNotContains criterion:
	if (a_Value.isEmpty())
	{
		return (m_Comparison == ncNotContains);
	}

	// String is valid, compare:
	switch (m_Comparison)
	{
		case ncContains:           return  a_Value.value().contains(m_Value.toString(), Qt::CaseInsensitive);
		case ncNotContains:        return !a_Value.value().contains(m_Value.toString(), Qt::CaseInsensitive);
		case ncEqual:              return  (a_Value.value().compare(m_Value.toString(), Qt::CaseInsensitive) == 0);
		case ncNotEqual:           return  (a_Value.value().compare(m_Value.toString(), Qt::CaseInsensitive) != 0);
		case ncGreaterThan:        return  (a_Value.value().compare(m_Value.toString(), Qt::CaseInsensitive) >  0);
		case ncGreaterThanOrEqual: return  (a_Value.value().compare(m_Value.toString(), Qt::CaseInsensitive) >= 0);
		case ncLowerThan:          return  (a_Value.value().compare(m_Value.toString(), Qt::CaseInsensitive) <  0);
		case ncLowerThanOrEqual:   return  (a_Value.value().compare(m_Value.toString(), Qt::CaseInsensitive) <= 0);
	}
	assert(!"Unknown comparison");
	return false;
}





bool Filter::Node::isNumberComparisonSatisfiedBy(const DatedOptional<double> & a_Value) const
{
	assert(m_Kind == nkComparison);

	// Check if the passed value is a valid number:
	if (!a_Value.isPresent())
	{
		return false;
	}

	// Check with the regular valid number logic:
	auto value = a_Value.value();
	return isValidNumberComparisonSatisfiedBy(value);
}





bool Filter::Node::isNumberComparisonSatisfiedBy(const QVariant & a_Value) const
{
	assert(m_Kind == nkComparison);

	// Check if the passed value is a valid number:
	bool isOK;
	auto value = a_Value.toDouble(&isOK);
	if (!isOK)
	{
		return false;
	}

	// Check with the regular valid number logic:
	return isValidNumberComparisonSatisfiedBy(value);
}




bool Filter::Node::isValidNumberComparisonSatisfiedBy(double a_Value) const
{
	assert(m_Kind == nkComparison);

	// For string-based comparison, use the filter value as a string:
	switch (m_Comparison)
	{
		case ncContains:           return  QString::number(a_Value).contains(m_Value.toString(), Qt::CaseInsensitive);
		case ncNotContains:        return !QString::number(a_Value).contains(m_Value.toString(), Qt::CaseInsensitive);
		default: break;
	}

	// For number-based comparisons, compare to a number; fail if NaN:
	bool isOk;
	auto cmpVal = m_Value.toDouble(&isOk);
	if (!isOk)
	{
		return false;
	}
	switch (m_Comparison)
	{
		case ncEqual:              return (std::abs(a_Value - cmpVal) < EPS);
		case ncNotEqual:           return (std::abs(a_Value - cmpVal) >= EPS);
		case ncGreaterThan:        return (a_Value >  cmpVal);
		case ncGreaterThanOrEqual: return (a_Value >= cmpVal);
		case ncLowerThan:          return (a_Value <  cmpVal);
		case ncLowerThanOrEqual:   return (a_Value <= cmpVal);
		default: break;
	}
	assert(!"Unknown comparison");
	return false;
}





bool Filter::Node::isDateComparisonSatisfiedBy(QDateTime a_Value) const
{
	assert(m_Kind == nkComparison);
	switch (m_Comparison)
	{
		case ncContains:           return  a_Value.toString(QLocale().dateTimeFormat()).contains(m_Value.toString(), Qt::CaseInsensitive);
		case ncNotContains:        return !a_Value.toString(QLocale().dateTimeFormat()).contains(m_Value.toString(), Qt::CaseInsensitive);
		case ncEqual:              return (a_Value == m_Value.toDateTime());
		case ncNotEqual:           return (a_Value != m_Value.toDateTime());
		case ncGreaterThan:        return (a_Value >  m_Value.toDateTime());
		case ncGreaterThanOrEqual: return (a_Value >= m_Value.toDateTime());
		case ncLowerThan:          return (a_Value <  m_Value.toDateTime());
		case ncLowerThanOrEqual:   return (a_Value <= m_Value.toDateTime());
	}
	assert(!"Unknown comparison");
	return false;
}





QByteArray Filter::Node::hash() const
{
	QCryptographicHash h(QCryptographicHash::Sha1);
	h.addData(reinterpret_cast<const char *>(&m_Kind), sizeof(m_Kind));
	switch (m_Kind)
	{
		case nkComparison:
		{
			h.addData(reinterpret_cast<const char *>(&m_SongProperty), sizeof(m_SongProperty));
			h.addData(reinterpret_cast<const char *>(&m_Comparison), sizeof(m_Comparison));
			h.addData(m_Value.toString().toUtf8());
			break;
		}
		case nkAnd:
		case nkOr:
		{
			for (const auto & ch: m_Children)
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

Filter::Filter(
	qlonglong a_DbRowId,
	const QString & a_DisplayName,
	const QString & a_Notes,
	bool a_IsFavorite,
	QColor a_BgColor,
	const DatedOptional<double> & a_DurationLimit
):
	m_DbRowId(a_DbRowId),
	m_DisplayName(a_DisplayName),
	m_Notes(a_Notes),
	m_IsFavorite(a_IsFavorite),
	m_BgColor(a_BgColor),
	m_DurationLimit(a_DurationLimit)
{
	setNoopFilter();
}






Filter::Filter(const Filter & a_CopyFrom):
	m_DbRowId(-1),
	m_DisplayName(a_CopyFrom.m_DisplayName),
	m_Notes(a_CopyFrom.m_Notes),
	m_IsFavorite(a_CopyFrom.m_IsFavorite),
	m_RootNode(a_CopyFrom.m_RootNode->clone()),
	m_BgColor(a_CopyFrom.m_BgColor),
	m_DurationLimit(a_CopyFrom.m_DurationLimit)
{
}





void Filter::setNoopFilter()
{
	m_RootNode.reset(new Node(
		Filter::Node::nspLength,
		Filter::Node::ncGreaterThanOrEqual,
		0
	));
}





void Filter::checkNodeConsistency() const
{
	if (m_RootNode == nullptr)
	{
		return;
	}
	assert(m_RootNode->parent() == nullptr);
	m_RootNode->checkConsistency();
}





QByteArray Filter::hash() const
{
	QCryptographicHash h(QCryptographicHash::Sha1);
	h.addData(m_DisplayName.toUtf8());
	h.addData(m_Notes.toUtf8());
	h.addData(m_IsFavorite ? "F" : "N", 1);
	h.addData(m_RootNode->hash());
	h.addData(m_BgColor.name().toUtf8());
	h.addData(m_DurationLimit.isPresent() ? "L" : "N", 1);
	if (m_DurationLimit.isPresent())
	{
		auto lim = m_DurationLimit.value();
		h.addData(reinterpret_cast<const char *>(&lim), sizeof(lim));
	}
	return h.result();
}





void Filter::setDbRowId(qlonglong a_DbRowId)
{
	assert(m_DbRowId == -1);
	m_DbRowId = a_DbRowId;
}






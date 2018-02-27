#include "Template.h"
#include <assert.h>





////////////////////////////////////////////////////////////////////////////////
// Template::Filter:

Template::Filter::Filter(Template::Filter::SongProperty a_SongProperty,
	Template::Filter::Comparison a_Comparison,
	QVariant a_Value
):
	m_Parent(nullptr),
	m_Kind(fkComparison),
	m_SongProperty(a_SongProperty),
	m_Comparison(a_Comparison),
	m_Value(a_Value)
{
}





Template::Filter::Filter(
	Template::Filter::Kind a_Combination,
	const std::vector<Template::FilterPtr> a_SubFilters
):
	m_Parent(nullptr),
	m_Kind(a_Combination),
	m_Children(a_SubFilters)
{
	assert(canHaveChildren());  // This constructor can only create children-able filters
}





const std::vector<Template::FilterPtr> &Template::Filter::children() const
{
	assert(canHaveChildren());
	return m_Children;
}





Template::Filter::SongProperty Template::Filter::songProperty() const
{
	assert(m_Kind == fkComparison);
	return m_SongProperty;
}





Template::Filter::Comparison Template::Filter::comparison() const
{
	assert(m_Kind == fkComparison);
	return m_Comparison;
}





QVariant Template::Filter::value() const
{
	assert(m_Kind == fkComparison);
	return m_Value;
}





void Template::Filter::setKind(Template::Filter::Kind a_Kind)
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





void Template::Filter::setSongProperty(Template::Filter::SongProperty a_SongProperty)
{
	assert(!canHaveChildren());
	m_SongProperty = a_SongProperty;
}





void Template::Filter::setComparison(Template::Filter::Comparison a_Comparison)
{
	assert(!canHaveChildren());
	m_Comparison = a_Comparison;
}





void Template::Filter::setValue(QVariant a_Value)
{
	assert(!canHaveChildren());
	m_Value = a_Value;
}





bool Template::Filter::canHaveChildren() const
{
	return ((m_Kind == fkAnd) || (m_Kind == fkOr));
}





void Template::Filter::addChild(Template::FilterPtr a_Child)
{
	assert(canHaveChildren());
	assert(a_Child != nullptr);
	m_Children.push_back(a_Child);
	a_Child->m_Parent = this;
}





void Template::Filter::replaceChild(Template::Filter * a_ExistingChild, Template::FilterPtr a_NewChild)
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





void Template::Filter::removeChild(const Template::Filter * a_ExistingChild)
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





void Template::Filter::checkConsistency() const
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





QString Template::Filter::getCaption() const
{
	switch (m_Kind)
	{
		case fkAnd: return Template::tr("And", "FilterCaption");
		case fkOr:  return Template::tr("Or",  "FilterCaption");
		case fkComparison:
		{
			return QString("%1 %2 %3").arg(
				songPropertyCaption(m_SongProperty),
				comparisonCaption(m_Comparison),
				m_Value.toString()
			);
		}
	}
	assert(!"Unknown filter kind");
	return QString("<invalid filter kind: %1>").arg(m_Kind);
}





QString Template::Filter::getDescription() const
{
	switch (m_Kind)
	{
		case fkAnd: return concatChildrenDescriptions(Template::tr(" and ", "FilterConcatString"));
		case fkOr:  return concatChildrenDescriptions(Template::tr(" or ",  "FilterConcatString"));
		case fkComparison: return QString("(%1)").arg(getCaption());
	}
	assert(!"Unknown filter kind");
	return QString("<invalid filter kind: %1>").arg(m_Kind);
}





Template::Filter::Kind Template::Filter::intToKind(int a_Kind)
{
	switch (a_Kind)
	{
		case fkComparison: return fkComparison;
		case fkAnd:        return fkAnd;
		case fkOr:         return fkOr;
	}
	throw std::runtime_error(QString("Unknown filter Kind: %1").arg(a_Kind).toUtf8().constData());
}





Template::Filter::Comparison Template::Filter::intToComparison(int a_Comparison)
{
	switch (a_Comparison)
	{
		case fcEqual:              return fcEqual;
		case fcNotEqual:           return fcNotEqual;
		case fcContains:           return fcContains;
		case fcNotContains:        return fcNotContains;
		case fcGreaterThan:        return fcGreaterThan;
		case fcGreaterThanOrEqual: return fcGreaterThanOrEqual;
		case fcLowerThan:          return fcLowerThan;
		case fcLowerThanOrEqual:   return fcLowerThanOrEqual;
	}
	throw std::runtime_error(QString("Unknown filter Comparison: %1").arg(a_Comparison).toUtf8().constData());
}





Template::Filter::SongProperty Template::Filter::intToSongProperty(int a_SongProperty)
{
	switch (a_SongProperty)
	{
		case fspAuthor:            return fspAuthor;
		case fspTitle:             return fspTitle;
		case fspGenre:             return fspGenre;
		case fspLength:            return fspLength;
		case fspMeasuresPerMinute: return fspMeasuresPerMinute;
		case fspRating:            return fspRating;
		case fspLastPlayed:        return fspLastPlayed;
	}
	throw std::runtime_error(QString("Unknown filter SongProperty: %1").arg(a_SongProperty).toUtf8().constData());
}





QString Template::Filter::songPropertyCaption(Template::Filter::SongProperty a_Prop)
{
	switch (a_Prop)
	{
		case fspAuthor:            return Template::tr("Author",     "SongPropertyCaption");
		case fspTitle:             return Template::tr("Title",      "SongPropertyCaption");
		case fspGenre:             return Template::tr("Genre",      "SongPropertyCaption");
		case fspLength:            return Template::tr("Length",     "SongPropertyCaption");
		case fspMeasuresPerMinute: return Template::tr("MPM",        "SongPropertyCaption");
		case fspRating:            return Template::tr("Rating",     "SongPropertyCaption");
		case fspLastPlayed:        return Template::tr("LastPlayed", "SongPropertyCaption");
	}
	assert(!"Unknown filter SongProperty");
	return QString();
}





QString Template::Filter::comparisonCaption(Template::Filter::Comparison a_Comparison)
{
	switch (a_Comparison)
	{
		case fcEqual:              return Template::tr("==",              "Comparison");
		case fcNotEqual:           return Template::tr("!=",              "Comparison");
		case fcContains:           return Template::tr("contains",        "Comparison");
		case fcNotContains:        return Template::tr("doesn't contain", "Comparison");
		case fcGreaterThan:        return Template::tr(">",               "Comparison");
		case fcGreaterThanOrEqual: return Template::tr(">=",              "Comparison");
		case fcLowerThan:          return Template::tr("<",               "Comparison");
		case fcLowerThanOrEqual:   return Template::tr("<=",              "Comparison");
	}
	assert(!"Unknown filter Comparison");
	return QString();
}





QString Template::Filter::concatChildrenDescriptions(const QString & a_Separator) const
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





////////////////////////////////////////////////////////////////////////////////
// Template::Item:

Template::Item::Item(
	const QString & a_DisplayName,
	const QString & a_Notes,
	bool a_IsFavorite
):
	m_DisplayName(a_DisplayName),
	m_Notes(a_Notes),
	m_IsFavorite(a_IsFavorite)
{
	setNoopFilter();
}





void Template::Item::setNoopFilter()
{
	m_Filter.reset(new Template::Filter(
		Template::Filter::fspLength,
		Template::Filter::fcGreaterThanOrEqual,
		0
	));
}





void Template::Item::checkFilterConsistency() const
{
	if (m_Filter == nullptr)
	{
		return;
	}
	assert(m_Filter->parent() == nullptr);
	m_Filter->checkConsistency();
}





////////////////////////////////////////////////////////////////////////////////
// Template:

Template::Template(qlonglong a_DbRowId):
	m_DbRowId(a_DbRowId)
{
}





Template::Template(qlonglong a_DbRowId, QString && a_DisplayName, QString && a_Notes):
	m_DbRowId(a_DbRowId),
	m_DisplayName(std::move(a_DisplayName)),
	m_Notes(std::move(a_Notes))
{
}





Template::ItemPtr Template::addItem(
	const QString & a_DisplayName,
	const QString & a_Notes,
	bool a_IsFavorite,
	int a_DstIndex
)
{
	auto res = std::make_shared<Template::Item>(a_DisplayName, a_Notes, a_IsFavorite);
	if (a_DstIndex < 0)
	{
		// Append:
		m_Items.push_back(res);
		return res;
	}
	assert(a_DstIndex <= static_cast<int>(m_Items.size()));
	m_Items.insert(m_Items.begin() + static_cast<size_t>(a_DstIndex), res);
	return res;
}



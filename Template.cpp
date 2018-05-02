#include "Template.h"
#include <assert.h>
#include <QLocale>
#include <QDebug>
#include "Song.h"





/** The difference that is still considered equal when comparing two numerical property values in a Filter. */
static const double EPS = 0.000001;





////////////////////////////////////////////////////////////////////////////////
// Template::Filter:

Template::Filter::Filter(
	Template::Filter::SongProperty a_SongProperty,
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





Template::FilterPtr Template::Filter::clone() const
{
	if (canHaveChildren())
	{
		auto res = std::make_shared<Template::Filter>(m_Kind, std::vector<Template::FilterPtr>());
		for (const auto & ch: m_Children)
		{
			res->addChild(ch->clone());
		}
		return res;
	}
	else
	{
		return std::make_shared<Template::Filter>(m_SongProperty, m_Comparison, m_Value);
	}
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





bool Template::Filter::isSatisfiedBy(const Song & a_Song) const
{
	switch (m_Kind)
	{
		case fkAnd:
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

		case fkOr:
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

		case fkComparison:
		{
			return isComparisonSatisfiedBy(a_Song);
		}
	}
	assert(!"Unknown filter kind");
	return false;
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
		case fspAuthor:                    return Template::tr("Author (any)", "SongPropertyCaption");
		case fspTitle:                     return Template::tr("Title (any)",  "SongPropertyCaption");
		case fspGenre:                     return Template::tr("Genre (any)",  "SongPropertyCaption");
		case fspLength:                    return Template::tr("Length",       "SongPropertyCaption");
		case fspMeasuresPerMinute:         return Template::tr("MPM (any)",    "SongPropertyCaption");
		case fspRating:                    return Template::tr("Rating",       "SongPropertyCaption");
		case fspLastPlayed:                return Template::tr("LastPlayed",   "SongPropertyCaption");
		case fspManualAuthor:              return Template::tr("Author (M)",   "SongPropertyCaption");
		case fspManualTitle:               return Template::tr("Title (M)",    "SongPropertyCaption");
		case fspManualGenre:               return Template::tr("Genre (M)",    "SongPropertyCaption");
		case fspManualMeasuresPerMinute:   return Template::tr("MPM (M)",      "SongPropertyCaption");
		case fspFileNameAuthor:            return Template::tr("Author (F)",   "SongPropertyCaption");
		case fspFileNameTitle:             return Template::tr("Title (F)",    "SongPropertyCaption");
		case fspFileNameGenre:             return Template::tr("Genre (F)",    "SongPropertyCaption");
		case fspFileNameMeasuresPerMinute: return Template::tr("MPM (F)",      "SongPropertyCaption");
		case fspId3Author:                 return Template::tr("Author (T)",   "SongPropertyCaption");
		case fspId3Title:                  return Template::tr("Title (T)",    "SongPropertyCaption");
		case fspId3Genre:                  return Template::tr("Genre (T)",    "SongPropertyCaption");
		case fspId3MeasuresPerMinute:      return Template::tr("MPM (T)",      "SongPropertyCaption");
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





bool Template::Filter::isComparisonSatisfiedBy(const Song & a_Song) const
{
	assert(m_Kind == fkComparison);
	switch (m_SongProperty)
	{
		case fspAuthor:
		{
			return (
				isStringComparisonSatisfiedBy(a_Song.tagManual().m_Author.toString()) ||
				isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Author.toString()) ||
				isStringComparisonSatisfiedBy(a_Song.tagId3().m_Author.toString())
			);
		}
		case fspTitle:
		{
			return (
				isStringComparisonSatisfiedBy(a_Song.tagManual().m_Title.toString()) ||
				isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Title.toString()) ||
				isStringComparisonSatisfiedBy(a_Song.tagId3().m_Title.toString())
			);
		}
		case fspGenre:
		{
			return (
				isStringComparisonSatisfiedBy(a_Song.tagManual().m_Genre.toString()) ||
				isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Genre.toString()) ||
				isStringComparisonSatisfiedBy(a_Song.tagId3().m_Genre.toString())
			);
		}
		case fspMeasuresPerMinute:
		{
			return (
				isNumberComparisonSatisfiedBy(a_Song.tagManual().m_MeasuresPerMinute.toDouble()) ||
				isNumberComparisonSatisfiedBy(a_Song.tagFileName().m_MeasuresPerMinute.toDouble()) ||
				isNumberComparisonSatisfiedBy(a_Song.tagId3().m_MeasuresPerMinute.toDouble())
			);
		}
		case fspManualAuthor:              return isStringComparisonSatisfiedBy(a_Song.tagManual().m_Author.toString());
		case fspManualTitle:               return isStringComparisonSatisfiedBy(a_Song.tagManual().m_Title.toString());
		case fspManualGenre:               return isStringComparisonSatisfiedBy(a_Song.tagManual().m_Genre.toString());
		case fspManualMeasuresPerMinute:   return isNumberComparisonSatisfiedBy(a_Song.tagManual().m_MeasuresPerMinute.toDouble());
		case fspFileNameAuthor:            return isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Author.toString());
		case fspFileNameTitle:             return isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Title.toString());
		case fspFileNameGenre:             return isStringComparisonSatisfiedBy(a_Song.tagFileName().m_Genre.toString());
		case fspFileNameMeasuresPerMinute: return isNumberComparisonSatisfiedBy(a_Song.tagFileName().m_MeasuresPerMinute.toDouble());
		case fspId3Author:                 return isStringComparisonSatisfiedBy(a_Song.tagId3().m_Author.toString());
		case fspId3Title:                  return isStringComparisonSatisfiedBy(a_Song.tagId3().m_Title.toString());
		case fspId3Genre:                  return isStringComparisonSatisfiedBy(a_Song.tagId3().m_Genre.toString());
		case fspId3MeasuresPerMinute:      return isNumberComparisonSatisfiedBy(a_Song.tagId3().m_MeasuresPerMinute.toDouble());
		case fspLastPlayed:                return isDateComparisonSatisfiedBy(a_Song.lastPlayed().toDateTime());
		case fspLength:                    return isNumberComparisonSatisfiedBy(a_Song.length().toDouble());
		case fspRating:                    return isNumberComparisonSatisfiedBy(a_Song.rating().toDouble());
	}
	assert(!"Unknown song property in comparison");
	return false;
}





bool Template::Filter::isStringComparisonSatisfiedBy(const QString & a_Value) const
{
	assert(m_Kind == fkComparison);
	switch (m_Comparison)
	{
		case fcContains:           return  a_Value.contains(m_Value.toString(), Qt::CaseInsensitive);
		case fcNotContains:        return !a_Value.contains(m_Value.toString(), Qt::CaseInsensitive);
		case fcEqual:              return  (a_Value.compare(m_Value.toString(), Qt::CaseInsensitive) == 0);
		case fcNotEqual:           return  (a_Value.compare(m_Value.toString(), Qt::CaseInsensitive) != 0);
		case fcGreaterThan:        return  (a_Value.compare(m_Value.toString(), Qt::CaseInsensitive) >  0);
		case fcGreaterThanOrEqual: return  (a_Value.compare(m_Value.toString(), Qt::CaseInsensitive) >= 0);
		case fcLowerThan:          return  (a_Value.compare(m_Value.toString(), Qt::CaseInsensitive) <  0);
		case fcLowerThanOrEqual:   return  (a_Value.compare(m_Value.toString(), Qt::CaseInsensitive) <= 0);
	}
	assert(!"Unknown comparison");
	return false;
}





bool Template::Filter::isNumberComparisonSatisfiedBy(double a_Value) const
{
	assert(m_Kind == fkComparison);
	switch (m_Comparison)
	{
		case fcContains:           return  QString::number(a_Value).contains(m_Value.toString(), Qt::CaseInsensitive);
		case fcNotContains:        return !QString::number(a_Value).contains(m_Value.toString(), Qt::CaseInsensitive);
		case fcEqual:              return (std::abs(a_Value - m_Value.toDouble()) < EPS);
		case fcNotEqual:           return (std::abs(a_Value - m_Value.toDouble()) >= EPS);
		case fcGreaterThan:        return (a_Value >  m_Value.toDouble());
		case fcGreaterThanOrEqual: return (a_Value >= m_Value.toDouble());
		case fcLowerThan:          return (a_Value <  m_Value.toDouble());
		case fcLowerThanOrEqual:   return (a_Value <= m_Value.toDouble());
	}
	assert(!"Unknown comparison");
	return false;
}





bool Template::Filter::isDateComparisonSatisfiedBy(QDateTime a_Value) const
{
	assert(m_Kind == fkComparison);
	switch (m_Comparison)
	{
		case fcContains:           return  a_Value.toString(QLocale().dateTimeFormat()).contains(m_Value.toString(), Qt::CaseInsensitive);
		case fcNotContains:        return !a_Value.toString(QLocale().dateTimeFormat()).contains(m_Value.toString(), Qt::CaseInsensitive);
		case fcEqual:              return (a_Value == m_Value.toDateTime());
		case fcNotEqual:           return (a_Value != m_Value.toDateTime());
		case fcGreaterThan:        return (a_Value >  m_Value.toDateTime());
		case fcGreaterThanOrEqual: return (a_Value >= m_Value.toDateTime());
		case fcLowerThan:          return (a_Value <  m_Value.toDateTime());
		case fcLowerThanOrEqual:   return (a_Value <= m_Value.toDateTime());
	}
	assert(!"Unknown comparison");
	return false;
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
	m_IsFavorite(a_IsFavorite),
	m_BgColor(255, 255, 255)
{
	setNoopFilter();
}





Template::ItemPtr Template::Item::clone() const
{
	auto res = std::make_shared<Template::Item>(m_DisplayName, m_Notes, false);
	res->setFilter(m_Filter->clone());
	return res;
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
	m_DbRowId(a_DbRowId),
	m_BgColor(255, 255, 255)
{
}





Template::Template(qlonglong a_DbRowId, QString && a_DisplayName, QString && a_Notes):
	m_DbRowId(a_DbRowId),
	m_DisplayName(std::move(a_DisplayName)),
	m_Notes(std::move(a_Notes)),
	m_BgColor(255, 255, 255)
{
}





void Template::setDbRowId(qlonglong a_DbRowId)
{
	assert(m_DbRowId == -1);  // Cannot already have a RowID
	m_DbRowId = a_DbRowId;
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
	m_Items.insert(m_Items.begin() + a_DstIndex, res);
	return res;
}





void Template::appendExistingItem(Template::ItemPtr a_Item)
{
	m_Items.push_back(a_Item);
}





void Template::delItem(int a_Index)
{
	assert(a_Index >= 0);
	assert(static_cast<size_t>(a_Index) < m_Items.size());
	m_Items.erase(m_Items.begin() + a_Index);
}






////////////////////////////////////////////////////////////////////////////////
// TemplateXmlExport:

QByteArray TemplateXmlExport::run(const std::vector<TemplatePtr> & a_Templates)
{
	TemplateXmlExport exporter;
	return exporter.exportAll(a_Templates);
}





TemplateXmlExport::TemplateXmlExport()
{
	m_Document.appendChild(m_Document.createComment("These are templates exported from SkauTan. https://github.com/madmaxoft/SkauTan"));
}





QByteArray TemplateXmlExport::exportAll(const std::vector<TemplatePtr> & a_Templates)
{
	auto root = m_Document.createElement("SkauTanTemplates");
	m_Document.appendChild(root);
	for (const auto & tmpl: a_Templates)
	{
		auto templateElement = m_Document.createElement("SkauTanTemplate");
		templateElement.setAttribute("name", tmpl->displayName());
		templateElement.setAttribute("notes", tmpl->notes());
		templateElement.setAttribute("bgColor", tmpl->bgColor().name());
		auto itemsElement = m_Document.createElement("items");
		for (const auto & item: tmpl->items())
		{
			auto itemElement = m_Document.createElement("item");
			itemElement.setAttribute("name", item->displayName());
			itemElement.setAttribute("notes", item->notes());
			itemElement.setAttribute("bgColor", item->bgColor().name());
			if (item->isFavorite())
			{
				itemElement.setAttribute("isFavorite", 1);
			}
			auto filterElement = m_Document.createElement("filter");
			addFilterToDom(item->filter(), filterElement);
			itemElement.appendChild(filterElement);
			itemsElement.appendChild(itemElement);
		}
		templateElement.appendChild(itemsElement);
		root.appendChild(templateElement);
	}

	return m_Document.toByteArray();
}





void TemplateXmlExport::addFilterToDom(const Template::FilterPtr && a_Filter, QDomElement & a_ParentElement)
{
	QString elementName;
	switch (a_Filter->kind())
	{
		case Template::Filter::fkComparison:
		{
			auto cmpElement = m_Document.createElement("comparison");
			a_ParentElement.appendChild(cmpElement);
			cmpElement.setAttribute("songProperty", a_Filter->songProperty());
			cmpElement.setAttribute("comparison", a_Filter->comparison());
			cmpElement.setAttribute("value", a_Filter->value().toString());
			return;
		}
		case Template::Filter::fkAnd:
		{
			elementName = "and";
			break;
		}
		case Template::Filter::fkOr:
		{
			elementName = "or";
			break;
		}
	}
	assert(!elementName.isEmpty());
	auto combinationElement = m_Document.createElement(elementName);
	a_ParentElement.appendChild(combinationElement);
	for (const auto & ch: a_Filter->children())
	{
		addFilterToDom(std::move(ch), combinationElement);
	}
}






////////////////////////////////////////////////////////////////////////////////
// TemplateXmlImport:

std::vector<TemplatePtr> TemplateXmlImport::run(const QByteArray & a_XmlData)
{
	TemplateXmlImport import;
	return import.importAll(a_XmlData);
}





TemplateXmlImport::TemplateXmlImport()
{
}





std::vector<TemplatePtr> TemplateXmlImport::importAll(const QByteArray & a_XmlData)
{
	std::vector<TemplatePtr> res;
	QString errMsg;
	int errLine, errColumn;
	if (!m_Document.setContent(a_XmlData, false, &errMsg, &errLine, &errColumn))
	{
		qWarning() << __FUNCTION__ << ": Failed to parse the XML: Line "
			<< errLine << ", column " << errColumn << ", msg: " << errMsg;
		return res;
	}
	auto docElement = m_Document.documentElement();
	if (docElement.tagName() != "SkauTanTemplates")
	{
		qWarning() << __FUNCTION__ << ": The root element is not <SkauTanTemplates>. Got " << docElement.tagName();
		return res;
	}
	for (auto te = docElement.firstChildElement("SkauTanTemplate"); !te.isNull(); te = te.nextSiblingElement("SkauTanTemplate"))
	{
		auto tmpl = readTemplate(te);
		if (tmpl != nullptr)
		{
			res.push_back(tmpl);
		}
	}
	return res;
}





TemplatePtr TemplateXmlImport::readTemplate(const QDomElement & a_TemplateXmlElement)
{
	auto res = std::make_shared<Template>(
		-1,
		a_TemplateXmlElement.attribute("name"),
		a_TemplateXmlElement.attribute("notes")
	);
	if (res == nullptr)
	{
		return nullptr;
	}

	// Read the template's bgColor:
	QColor c(a_TemplateXmlElement.attribute("bgColor"));
	if (c.isValid())
	{
		res->setBgColor(c);
	}

	// Read the template's items:
	auto items = a_TemplateXmlElement.firstChildElement("items");
	if (items.isNull())
	{
		qDebug() << __FUNCTION__ << ": Template item " << res->displayName() << " contains no <items>.";
		return res;
	}
	for (auto ie = items.firstChildElement("item"); !ie.isNull(); ie = ie.nextSiblingElement("item"))
	{
		auto item = readTemplateItem(ie);
		if (item != nullptr)
		{
			res->appendExistingItem(item);
		}
	}
	return res;
}





Template::ItemPtr TemplateXmlImport::readTemplateItem(const QDomElement & a_ItemXmlElement)
{
	auto res = std::make_shared<Template::Item>(
		a_ItemXmlElement.attribute("name"),
		a_ItemXmlElement.attribute("notes"),
		(a_ItemXmlElement.attribute("isFavorite", "0") == "1")
	);
	if (res == nullptr)
	{
		return nullptr;
	}

	// Read the item's bgColor:
	QColor c(a_ItemXmlElement.attribute("bgColor"));
	if (c.isValid())
	{
		res->setBgColor(c);
	}

	// Read the item's filter:
	auto fe = a_ItemXmlElement.firstChildElement("filter");
	if (fe.isNull())
	{
		qWarning() << __FUNCTION__ << ": There's no <filter> element in template item " << res->displayName();
		return nullptr;
	}
	auto fc = fe.firstChildElement();
	if (fc.isNull())
	{
		qWarning() << __FUNCTION__ << ": The <filter> element contains no filter in template item " << res->displayName();
		return nullptr;
	}
	auto filter = readTemplateFilter(fe.firstChildElement());
	if (filter == nullptr)
	{
		qWarning() << __FUNCTION__ << ": Cannot read the filter for template item " << res->displayName();
		return nullptr;
	}
	res->setFilter(filter);
	return res;
}





Template::FilterPtr TemplateXmlImport::readTemplateFilter(const QDomElement & a_FilterXmlElement)
{
	// Read a comparison filter:
	using TF = Template::Filter;
	const auto & tagName = a_FilterXmlElement.tagName();
	if (tagName == "comparison")
	{
		try
		{
			auto songProperty = TF::intToSongProperty(a_FilterXmlElement.attribute("songProperty").toInt());
			auto comparison   = TF::intToComparison  (a_FilterXmlElement.attribute("comparison"  ).toInt());
			auto value = a_FilterXmlElement.attribute("value");
			return std::make_shared<Template::Filter>(songProperty, comparison, value);
		}
		catch (const std::exception & exc)
		{
			qWarning() << __FUNCTION__ << ": Cannot read filter comparison: " << exc.what();
			return nullptr;
		}
	}

	// Read a combinator filter:
	TF::Kind kind;
	if (tagName == "and")
	{
		kind = TF::fkAnd;
	}
	else if (tagName == "or")
	{
		kind = TF::fkOr;
	}
	else
	{
		qWarning() << __FUNCTION__ << ": Unknown filter kind: " << tagName;
		return nullptr;
	}
	auto res = std::make_shared<Template::Filter>(kind);

	// Read the children filters:
	for (auto ce = a_FilterXmlElement.firstChildElement(); !ce.isNull(); ce = ce.nextSiblingElement())
	{
		auto child = readTemplateFilter(ce);
		if (child == nullptr)
		{
			qWarning() << __FUNCTION__ << ": Cannot read child filter, line " << ce.lineNumber();
			return nullptr;
		}
		res->addChild(child);
	}
	return res;
}

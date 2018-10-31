#include "Template.hpp"
#include <cassert>
#include <QLocale>
#include <QCryptographicHash>
#include "Exception.hpp"
#include "Song.hpp"





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





void Template::appendItem(FilterPtr a_Filter)
{
	m_Items.push_back(a_Filter);
}





void Template::insertItem(FilterPtr a_Item, int a_DstIndex)
{
	m_Items.insert(m_Items.begin() + a_DstIndex, a_Item);
}





void Template::delItem(int a_Index)
{
	assert(a_Index >= 0);
	assert(static_cast<size_t>(a_Index) < m_Items.size());
	m_Items.erase(m_Items.begin() + a_Index);
}





bool Template::removeAllFilterRefs(FilterPtr a_Filter)
{
	auto itr = std::remove_if(m_Items.begin(), m_Items.end(),
		[a_Filter](FilterPtr a_CBFilter)
		{
			return (a_Filter == a_CBFilter);
		}
	);
	if (itr == m_Items.end())
	{
		return false;
	}
	m_Items.erase(itr, m_Items.end());
	return true;
}





void Template::replaceSameFilters(const std::vector<FilterPtr> & a_KnownFilters)
{
	// Build a hash-map of the known filters:
	std::map<QByteArray, FilterPtr> knownFiltersByHash;
	for (const auto & filter: a_KnownFilters)
	{
		knownFiltersByHash[filter->hash()] = filter;
	}

	// Replace:
	for (auto itr = m_Items.begin(), end = m_Items.end(); itr != end; ++itr)
	{
		auto knownItr = knownFiltersByHash.find((*itr)->hash());
		if (knownItr != knownFiltersByHash.end())
		{
			*itr = knownItr->second;
		}
	}
}





bool Template::replaceFilter(const Filter & a_From, Filter & a_To)
{
	bool hasReplaced = false;
	for (auto itr = m_Items.begin(), end = m_Items.end(); itr != end; ++itr)
	{
		if (itr->get() == &a_From)
		{
			*itr = a_To.shared_from_this();
			hasReplaced = true;
		}
	}
	return hasReplaced;
}





void Template::swapItemsByIdx(size_t a_Index1, size_t a_Index2)
{
	assert(a_Index1 < m_Items.size());
	assert(a_Index2 < m_Items.size());

	std::swap(m_Items[a_Index1], m_Items[a_Index2]);
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
			if (item->durationLimit().isPresent())
			{
				itemElement.setAttribute("durationLimit", item->durationLimit().value());
			}
			auto filterElement = m_Document.createElement("filter");
			addNodeToDom(item->rootNode(), filterElement);
			itemElement.appendChild(filterElement);
			itemsElement.appendChild(itemElement);
		}
		templateElement.appendChild(itemsElement);
		root.appendChild(templateElement);
	}

	return m_Document.toByteArray();
}





void TemplateXmlExport::addNodeToDom(const Filter::NodePtr && a_Node, QDomElement & a_ParentElement)
{
	QString elementName;
	switch (a_Node->kind())
	{
		case Filter::Node::nkComparison:
		{
			auto cmpElement = m_Document.createElement("comparison");
			a_ParentElement.appendChild(cmpElement);
			cmpElement.setAttribute("songProperty", a_Node->songProperty());
			cmpElement.setAttribute("comparison", a_Node->comparison());
			cmpElement.setAttribute("value", a_Node->value().toString());
			return;
		}
		case Filter::Node::nkAnd:
		{
			elementName = "and";
			break;
		}
		case Filter::Node::nkOr:
		{
			elementName = "or";
			break;
		}
	}
	assert(!elementName.isEmpty());
	auto combinationElement = m_Document.createElement(elementName);
	a_ParentElement.appendChild(combinationElement);
	for (const auto & ch: a_Node->children())
	{
		addNodeToDom(std::move(ch), combinationElement);
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
		qWarning() << "Failed to parse the XML: Line "
			<< errLine << ", column " << errColumn << ", msg: " << errMsg;
		return res;
	}
	auto docElement = m_Document.documentElement();
	if (docElement.tagName() != "SkauTanTemplates")
	{
		qWarning() << "The root element is not <SkauTanTemplates>. Got " << docElement.tagName();
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
		qDebug() << "Template item " << res->displayName() << " contains no <items>.";
		return res;
	}
	for (auto ie = items.firstChildElement("item"); !ie.isNull(); ie = ie.nextSiblingElement("item"))
	{
		auto item = readTemplateItem(ie);
		if (item != nullptr)
		{
			res->appendItem(item);
		}
	}
	return res;
}





FilterPtr TemplateXmlImport::readTemplateItem(const QDomElement & a_ItemXmlElement)
{
	auto res = std::make_shared<Filter>(
		-1,
		a_ItemXmlElement.attribute("name"),
		a_ItemXmlElement.attribute("notes"),
		(a_ItemXmlElement.attribute("isFavorite", "0") == "1"),
		QColor(),
		DatedOptional<double>()
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

	// Read the item's durationLimit:
	auto durationLimitStr = a_ItemXmlElement.attribute("durationLimit");
	bool isOK;
	auto durationLimit = durationLimitStr.toDouble(&isOK);
	if (isOK)
	{
		res->setDurationLimit(durationLimit);
	}

	// Read the item's filter nodes:
	auto fe = a_ItemXmlElement.firstChildElement("filter");
	if (fe.isNull())
	{
		qWarning() << "There's no <filter> element in filter " << res->displayName();
		return nullptr;
	}
	auto fc = fe.firstChildElement();
	if (fc.isNull())
	{
		qWarning() << "The <filter> element contains no filter in filter " << res->displayName();
		return nullptr;
	}
	auto node = readTemplateFilterNode(fe.firstChildElement());
	if (node == nullptr)
	{
		qWarning() << "Cannot read the filter root node for filter " << res->displayName();
		return nullptr;
	}
	res->setRootNode(node);
	return res;
}





Filter::NodePtr TemplateXmlImport::readTemplateFilterNode(const QDomElement & a_FilterXmlElement)
{
	// Read a comparison filter node:
	const auto & tagName = a_FilterXmlElement.tagName();
	if (tagName == "comparison")
	{
		try
		{
			auto songProperty = Filter::Node::intToSongProperty(a_FilterXmlElement.attribute("songProperty").toInt());
			auto comparison   = Filter::Node::intToComparison  (a_FilterXmlElement.attribute("comparison"  ).toInt());
			auto value = a_FilterXmlElement.attribute("value");
			return std::make_shared<Filter::Node>(songProperty, comparison, value);
		}
		catch (const std::exception & exc)
		{
			qWarning() << "Cannot read filter comparison: " << exc.what();
			return nullptr;
		}
	}

	// Read a combinator filter node:
	Filter::Node::Kind kind;
	if (tagName == "and")
	{
		kind = Filter::Node::nkAnd;
	}
	else if (tagName == "or")
	{
		kind = Filter::Node::nkOr;
	}
	else
	{
		qWarning() << "Unknown filter kind: " << tagName;
		return nullptr;
	}
	auto res = std::make_shared<Filter::Node>(kind);

	// Read the children nodes:
	for (auto ce = a_FilterXmlElement.firstChildElement(); !ce.isNull(); ce = ce.nextSiblingElement())
	{
		auto child = readTemplateFilterNode(ce);
		if (child == nullptr)
		{
			qWarning() << "Cannot read child filter node, line " << ce.lineNumber();
			return nullptr;
		}
		res->addChild(child);
	}
	return res;
}

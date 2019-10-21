#include "Template.hpp"
#include <cassert>
#include <QLocale>
#include <QCryptographicHash>
#include "Exception.hpp"
#include "Song.hpp"





////////////////////////////////////////////////////////////////////////////////
// Template:

Template::Template(qlonglong aDbRowId):
	mDbRowId(aDbRowId),
	mBgColor(255, 255, 255)
{
}





Template::Template(qlonglong aDbRowId, QString && aDisplayName, QString && aNotes):
	mDbRowId(aDbRowId),
	mDisplayName(std::move(aDisplayName)),
	mNotes(std::move(aNotes)),
	mBgColor(255, 255, 255)
{
}





void Template::setDbRowId(qlonglong aDbRowId)
{
	assert(mDbRowId == -1);  // Cannot already have a RowID
	mDbRowId = aDbRowId;
}





void Template::appendItem(FilterPtr aFilter)
{
	mItems.push_back(aFilter);
}





void Template::insertItem(FilterPtr aItem, int aDstIndex)
{
	mItems.insert(mItems.begin() + aDstIndex, aItem);
}





void Template::delItem(int aIndex)
{
	assert(aIndex >= 0);
	assert(static_cast<size_t>(aIndex) < mItems.size());
	mItems.erase(mItems.begin() + aIndex);
}





bool Template::removeAllFilterRefs(FilterPtr aFilter)
{
	auto itr = std::remove_if(mItems.begin(), mItems.end(),
		[aFilter](FilterPtr aCBFilter)
		{
			return (aFilter == aCBFilter);
		}
	);
	if (itr == mItems.end())
	{
		return false;
	}
	mItems.erase(itr, mItems.end());
	return true;
}





void Template::replaceSameFilters(const std::vector<FilterPtr> & aKnownFilters)
{
	// Build a hash-map of the known filters:
	std::map<QByteArray, FilterPtr> knownFiltersByHash;
	for (const auto & filter: aKnownFilters)
	{
		knownFiltersByHash[filter->hash()] = filter;
	}

	// Replace:
	for (auto itr = mItems.begin(), end = mItems.end(); itr != end; ++itr)
	{
		auto knownItr = knownFiltersByHash.find((*itr)->hash());
		if (knownItr != knownFiltersByHash.end())
		{
			*itr = knownItr->second;
		}
	}
}





bool Template::replaceFilter(const Filter & aFrom, Filter & aTo)
{
	bool hasReplaced = false;
	for (auto itr = mItems.begin(), end = mItems.end(); itr != end; ++itr)
	{
		if (itr->get() == &aFrom)
		{
			*itr = aTo.shared_from_this();
			hasReplaced = true;
		}
	}
	return hasReplaced;
}





void Template::swapItemsByIdx(size_t aIndex1, size_t aIndex2)
{
	assert(aIndex1 < mItems.size());
	assert(aIndex2 < mItems.size());

	std::swap(mItems[aIndex1], mItems[aIndex2]);
}






////////////////////////////////////////////////////////////////////////////////
// TemplateXmlExport:

QByteArray TemplateXmlExport::run(const std::vector<TemplatePtr> & aTemplates)
{
	TemplateXmlExport exporter;
	return exporter.exportAll(aTemplates);
}





TemplateXmlExport::TemplateXmlExport()
{
	mDocument.appendChild(mDocument.createComment("These are templates exported from SkauTan. https://github.com/madmaxoft/SkauTan"));
}





QByteArray TemplateXmlExport::exportAll(const std::vector<TemplatePtr> & aTemplates)
{
	auto root = mDocument.createElement("SkauTanTemplates");
	mDocument.appendChild(root);
	for (const auto & tmpl: aTemplates)
	{
		auto templateElement = mDocument.createElement("SkauTanTemplate");
		templateElement.setAttribute("name", tmpl->displayName());
		templateElement.setAttribute("notes", tmpl->notes());
		templateElement.setAttribute("bgColor", tmpl->bgColor().name());
		auto itemsElement = mDocument.createElement("items");
		for (const auto & item: tmpl->items())
		{
			auto itemElement = mDocument.createElement("item");
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
			auto filterElement = mDocument.createElement("filter");
			addNodeToDom(item->rootNode(), filterElement);
			itemElement.appendChild(filterElement);
			itemsElement.appendChild(itemElement);
		}
		templateElement.appendChild(itemsElement);
		root.appendChild(templateElement);
	}

	return mDocument.toByteArray();
}





void TemplateXmlExport::addNodeToDom(const Filter::NodePtr && aNode, QDomElement & aParentElement)
{
	QString elementName;
	switch (aNode->kind())
	{
		case Filter::Node::nkComparison:
		{
			auto cmpElement = mDocument.createElement("comparison");
			aParentElement.appendChild(cmpElement);
			cmpElement.setAttribute("songProperty", aNode->songProperty());
			cmpElement.setAttribute("comparison", aNode->comparison());
			cmpElement.setAttribute("value", aNode->value().toString());
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
	auto combinationElement = mDocument.createElement(elementName);
	aParentElement.appendChild(combinationElement);
	for (const auto & ch: aNode->children())
	{
		addNodeToDom(std::move(ch), combinationElement);
	}
}






////////////////////////////////////////////////////////////////////////////////
// TemplateXmlImport:

std::vector<TemplatePtr> TemplateXmlImport::run(const QByteArray & aXmlData)
{
	TemplateXmlImport import;
	return import.importAll(aXmlData);
}





TemplateXmlImport::TemplateXmlImport()
{
}





std::vector<TemplatePtr> TemplateXmlImport::importAll(const QByteArray & aXmlData)
{
	std::vector<TemplatePtr> res;
	QString errMsg;
	int errLine, errColumn;
	if (!mDocument.setContent(aXmlData, false, &errMsg, &errLine, &errColumn))
	{
		qWarning() << "Failed to parse the XML: Line "
			<< errLine << ", column " << errColumn << ", msg: " << errMsg;
		return res;
	}
	auto docElement = mDocument.documentElement();
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





TemplatePtr TemplateXmlImport::readTemplate(const QDomElement & aTemplateXmlElement)
{
	auto res = std::make_shared<Template>(
		-1,
		aTemplateXmlElement.attribute("name"),
		aTemplateXmlElement.attribute("notes")
	);
	if (res == nullptr)
	{
		return nullptr;
	}

	// Read the template's bgColor:
	QColor c(aTemplateXmlElement.attribute("bgColor"));
	if (c.isValid())
	{
		res->setBgColor(c);
	}

	// Read the template's items:
	auto items = aTemplateXmlElement.firstChildElement("items");
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





FilterPtr TemplateXmlImport::readTemplateItem(const QDomElement & aItemXmlElement)
{
	auto res = std::make_shared<Filter>(
		-1,
		aItemXmlElement.attribute("name"),
		aItemXmlElement.attribute("notes"),
		(aItemXmlElement.attribute("isFavorite", "0") == "1"),
		QColor(),
		DatedOptional<double>()
	);
	if (res == nullptr)
	{
		return nullptr;
	}

	// Read the item's bgColor:
	QColor c(aItemXmlElement.attribute("bgColor"));
	if (c.isValid())
	{
		res->setBgColor(c);
	}

	// Read the item's durationLimit:
	auto durationLimitStr = aItemXmlElement.attribute("durationLimit");
	bool isOK;
	auto durationLimit = durationLimitStr.toDouble(&isOK);
	if (isOK)
	{
		res->setDurationLimit(durationLimit);
	}

	// Read the item's filter nodes:
	auto fe = aItemXmlElement.firstChildElement("filter");
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





Filter::NodePtr TemplateXmlImport::readTemplateFilterNode(const QDomElement & aFilterXmlElement)
{
	// Read a comparison filter node:
	const auto & tagName = aFilterXmlElement.tagName();
	if (tagName == "comparison")
	{
		try
		{
			auto songProperty = Filter::Node::intToSongProperty(aFilterXmlElement.attribute("songProperty").toInt());
			auto comparison   = Filter::Node::intToComparison  (aFilterXmlElement.attribute("comparison"  ).toInt());
			auto value = aFilterXmlElement.attribute("value");
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
	for (auto ce = aFilterXmlElement.firstChildElement(); !ce.isNull(); ce = ce.nextSiblingElement())
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

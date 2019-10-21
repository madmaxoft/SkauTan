#include "TagImportExport.hpp"
#include <QFile>
#include "../Exception.hpp"
#include "../DatedOptional.hpp"
#include "Database.hpp"





/** If the value is present, sets the element's attribute to its value. */
static void setAttribOptional(QDomElement & aElement, const QString & aName, const DatedOptional<QString> & aValue)
{
	if (aValue.isEmpty())
	{
		return;
	}
	aElement.setAttribute(aName, aValue.value());
}





/** If the value is present, sets the element's attribute to its value. */
static void setAttribOptional(QDomElement & aElement, const QString & aName, const DatedOptional<double> & aValue)
{
	if (aValue.isEmpty())
	{
		return;
	}
	aElement.setAttribute(aName, QString::number(aValue.value(), 'g', 2));
}





/** Converts from an attribute value into a specified kind of DatedOptional. */
template <typename T>
static DatedOptional<T> attribToOptional(const QString & aAttribValue);




/** Converts from an attribute value into the QString DatedOptional. */
template <>
DatedOptional<QString> attribToOptional<QString>(const QString & aAttribValue)
{
	if (aAttribValue.isEmpty())
	{
		return DatedOptional<QString>();
	}
	return DatedOptional<QString>(aAttribValue, QDateTime::currentDateTimeUtc());
}





/** Converts from an attribute value into the double DatedOptional. */
template <>
DatedOptional<double> attribToOptional<double>(const QString & aAttribValue)
{
	if (aAttribValue.isEmpty())
	{
		return DatedOptional<double>();
	}
	bool isOK;
	double value = aAttribValue.toDouble(&isOK);
	if (!isOK)
	{
		return DatedOptional<double>();
	}
	return DatedOptional<double>(value, QDateTime::currentDateTimeUtc());
}





////////////////////////////////////////////////////////////////////////////////
// TagImportExport:

void TagImportExport::doExport(const Database & aDB, const QString & aFileName)
{
	// Open the output file:
	QFile f(aFileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		throw Exception(tr("Cannot open file %1"), aFileName);
	}

	QDomDocument doc;
	doc.appendChild(doc.createComment("These are tags exported from SkauTan. https://github.com/madmaxoft/SkauTan"));
	auto root = doc.createElement("SkauTanTags");
	doc.appendChild(root);
	auto & sdm = aDB.songSharedDataMap();
	for (const auto & sd: sdm)
	{
		if (sd.second->duplicatesCount() == 0)
		{
			continue;
		}
		const auto song = sd.second->duplicates()[0];
		auto elm = doc.createElement("song");
		elm.setAttribute("hash", QString::fromUtf8(song->hash().toHex()));
		setAttribOptional(elm, "author", song->primaryAuthor());
		setAttribOptional(elm, "title",  song->primaryTitle());
		setAttribOptional(elm, "genre",  song->primaryGenre());
		setAttribOptional(elm, "mpm",    song->primaryMeasuresPerMinute());
		root.appendChild(elm);
	}
	auto baDoc = doc.toByteArray();
	auto numWritten = f.write(baDoc);
	if (numWritten != baDoc.size())
	{
		throw Exception("Cannot write entire file %1.", aFileName);
	}
	f.close();
}





void TagImportExport::doImport(Database & aDB, const QString & aFileName)
{
	// Find the expected hash length:
	auto & sdm = aDB.songSharedDataMap();
	if (sdm.empty())
	{
		// No songs in the DB -> no import needed
		return;
	}
	auto expectedHashLength = sdm.begin()->first.length();

	// Open the input file:
	QFile f(aFileName);
	if (!f.open(QIODevice::ReadOnly))
	{
		throw Exception("Cannot open file %1", aFileName);
	}
	auto xml = f.readAll();
	f.close();

	// Parse the XML:
	QDomDocument doc;
	QString errMsg;
	int errLine, errColumn;
	if (!doc.setContent(xml, false, &errMsg, &errLine, &errColumn))
	{
		throw Exception(tr("The XML data is invalid: %1 at %2:%3"), errMsg, errLine, errColumn);
	}

	// Parse the XML DOM into data:
	std::map<QByteArray, Song::Tag> tags;
	auto docElement = doc.documentElement();
	if (docElement.tagName() != "SkauTanTags")
	{
		throw Exception(tr("The root element is not <SkauTanTags>. Got <%1> instead."), docElement.tagName());
	}
	for (auto se = docElement.firstChildElement("song"); !se.isNull(); se = se.nextSiblingElement("song"))
	{
		auto hashAttr = se.attributeNode("hash");
		if (hashAttr.isNull())
		{
			qWarning() << "<song> element has no \"hash\" attribute.";
			continue;
		}
		auto hash = QByteArray::fromHex(hashAttr.value().toUtf8());
		if (hash.length() != expectedHashLength)
		{
			qWarning() << "<song> element has invalid \"hash\" attribute length: "
				<< hash.length() << " instead of " << expectedHashLength
				<< " (value: " << hashAttr.value() << ")";
			continue;
		}
		auto & tag = tags[hash];
		tag.mAuthor            = attribToOptional<QString>(se.attribute("author"));
		tag.mTitle             = attribToOptional<QString>(se.attribute("title"));
		tag.mGenre             = attribToOptional<QString>(se.attribute("genre"));
		tag.mMeasuresPerMinute = attribToOptional<double> (se.attribute("mpm"));
	}

	// Fill in the data:
	aDB.addToSharedDataManualTags(tags);
}

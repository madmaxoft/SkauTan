#pragma once

#include <memory>
#include <vector>
#include <QVariant>
#include <QDomDocument>
#include <QColor>
#include "DatedOptional.hpp"
#include "Filter.hpp"





// fwd:
class Playlist;
class Song;
class Database;





/** Represents a single template for adding multiple tracks to the playlist, based on various criteria. */
class Template:
	public QObject
{
	using Super = QObject;
	Q_OBJECT


public:


	/** Creates a new empty template. */
	Template(qlonglong aDbRowId);

	/** Creates a new empty template with the specified values. */
	Template(qlonglong aDbRowId, QString && aDisplayName, QString && aNotes);

	// Simple getters:
	qlonglong dbRowId() const { return mDbRowId; }
	const QString & displayName() const { return mDisplayName; }
	const QString & notes() const { return mNotes; }
	const std::vector<FilterPtr> & items() const { return mItems; }
	const QColor & bgColor() const { return mBgColor; }

	// Simple setters:
	void setDisplayName(const QString & aDisplayName) { mDisplayName = aDisplayName; }
	void setNotes(const QString & aNotes) { mNotes = aNotes; }
	void setBgColor(const QColor & aBgColor) { mBgColor = aBgColor; }

	/** Sets the DB RowID for the template.
	Checks that a RowID hasn't been assigned yet. */
	void setDbRowId(qlonglong aDbRowId);

	/** Adds the specified filter at the end of the item list. */
	void appendItem(FilterPtr aItem);

	/** Inserts the specified filter at the specified position in the item list. */
	void insertItem(FilterPtr aItem, int aDstIndex);

	/** Moves an item from the specified source index to the specified destination index.
	Asserts if the indices are not valid. */
	void moveItem(int aSrcIndex, int aDstIndex);

	/** Deletes the item at the specified index. */
	void delItem(int aIndex);

	/** Removes all references to the specified filter from the template.
	Returns true if there were any references, false if not. */
	bool removeAllFilterRefs(FilterPtr aFilter);

	/** Appends the items to the specified playlist.
	aSongsDB is the song database from which to choose songs. */
	void appendToPlaylist(Playlist & aDst, Database & aSongsDB);

	/** Replaces items (filters) in this templates with filters from aKnownFilters that have the same hash.
	If an item has no corresponding filter in aKnownFilters, it is left untouched.
	This is used mainly when importing templates, to unify with existing filters.
	If there are multiple filters in aKnownFilters with the same hash, (random) one of those is picked. */
	void replaceSameFilters(const std::vector<FilterPtr> & aKnownFilters);

	/** Replaces all instances of the specified filter in mItems with the other filter.
	Returns true if at least one instance was replaced. */
	bool replaceFilter(const Filter & aFrom, Filter & aTo);

	/** Swaps the two specified items.
	Asserts that the indices are valid. */
	void swapItemsByIdx(size_t aIndex1, size_t aIndex2);


protected:

	/** The RowID in the DB. -1 if not assigned yet. */
	qlonglong mDbRowId;

	QString mDisplayName;
	QString mNotes;
	std::vector<FilterPtr> mItems;

	/** The background color to use when displaying this template. */
	QColor mBgColor;
};

using TemplatePtr = std::shared_ptr<Template>;

Q_DECLARE_METATYPE(TemplatePtr);





/** Helper class that implements exporting multiple templates into an XML format. */
class TemplateXmlExport
{
public:

	/** Exports the specified templates to a XML format.
	Returns the exported data as a byte array. */
	static QByteArray run(const std::vector<TemplatePtr> & aTemplates);


protected:

	/** The DOM document representation used for building the XML. */
	QDomDocument mDocument;


	/** Creates a new instance of this class, initializes the internal state. */
	TemplateXmlExport();

	/** Exports the specified templates to XML format, returns the data as a byte array. */
	QByteArray exportAll(const std::vector<TemplatePtr> & aTemplates);

	/** Adds the specified filter node to mDocument as a child of the specified DOM element. */
	void addNodeToDom(const Filter::NodePtr && aNode, QDomElement & aParentElement);
};





/** Helper class that implements importing templates from an XML format. */
class TemplateXmlImport
{
public:

	/** Imports the templates from the specified XML-formatted data.
	Returns an empty vector on error.
	Note that the caller should unify the contained filters with their own filters prior to merging
	the returned templates into their Database. */
	static std::vector<TemplatePtr> run(const QByteArray & aXmlData);


protected:

	/** The DOM document representation of the input data. */
	QDomDocument mDocument;


	/** Creates a new instance of this class. */
	TemplateXmlImport();

	/** Imports the templates from the specified XML-formatted data.
	Returns an empty vector on error. */
	std::vector<TemplatePtr> importAll(const QByteArray & aXmlData);

	/** Reads and returns a Template from the specified XML element.
	aTemplateXmlElement is supposed to represent the <SkauTanTemplate> element in the XML. */
	TemplatePtr readTemplate(const QDomElement & aTemplateXmlElement);

	/** Reads and returns a FilterPtr from the specified XML element.
	aItemXmlElement is supposed to represent the <item> element in the XML.
	The returned filter is always a new unique object. */
	FilterPtr readTemplateItem(const QDomElement & aItemXmlElement);

	/** Reads and returns a Node from the specified XML element.
	aFilterXmlElement is supposed to represent the <and>, <or> or <comparison> element in the XML. */
	Filter::NodePtr readTemplateFilterNode(const QDomElement & aNodeXmlElement);
};

#ifndef TEMPLATE_H
#define TEMPLATE_H





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
	Template(qlonglong a_DbRowId);

	/** Creates a new empty template with the specified values. */
	Template(qlonglong a_DbRowId, QString && a_DisplayName, QString && a_Notes);

	// Simple getters:
	qlonglong dbRowId() const { return m_DbRowId; }
	const QString & displayName() const { return m_DisplayName; }
	const QString & notes() const { return m_Notes; }
	const std::vector<FilterPtr> & items() const { return m_Items; }
	const QColor & bgColor() const { return m_BgColor; }

	// Simple setters:
	void setDisplayName(const QString & a_DisplayName) { m_DisplayName = a_DisplayName; }
	void setNotes(const QString & a_Notes) { m_Notes = a_Notes; }
	void setBgColor(const QColor & a_BgColor) { m_BgColor = a_BgColor; }

	/** Sets the DB RowID for the template.
	Checks that a RowID hasn't been assigned yet. */
	void setDbRowId(qlonglong a_DbRowId);

	/** Adds the specified filter at the end of the item list. */
	void appendItem(FilterPtr a_Item);

	/** Inserts the specified filter at the specified position in the item list. */
	void insertItem(FilterPtr a_Item, int a_DstIndex);

	/** Moves an item from the specified source index to the specified destination index.
	Asserts if the indices are not valid. */
	void moveItem(int a_SrcIndex, int a_DstIndex);

	/** Deletes the item at the specified index. */
	void delItem(int a_Index);

	/** Removes all references to the specified filter from the template.
	Returns true if there were any references, false if not. */
	bool removeAllFilterRefs(FilterPtr a_Filter);

	/** Appends the items to the specified playlist.
	a_SongsDB is the song database from which to choose songs. */
	void appendToPlaylist(Playlist & a_Dst, Database & a_SongsDB);

	/** Replaces items (filters) in this templates with filters from a_KnownFilters that have the same hash.
	If an item has no corresponding filter in a_KnownFilters, it is left untouched.
	This is used mainly when importing templates, to unify with existing filters.
	If there are multiple filters in a_KnownFilters with the same hash, (random) one of those is picked. */
	void replaceSameFilters(const std::vector<FilterPtr> & a_KnownFilters);

	/** Replaces all instances of the specified filter in m_Items with the other filter.
	Returns true if at least one instance was replaced. */
	bool replaceFilter(const Filter & a_From, Filter & a_To);

	/** Swaps the two specified items.
	Asserts that the indices are valid. */
	void swapItemsByIdx(size_t a_Index1, size_t a_Index2);


protected:

	/** The RowID in the DB. -1 if not assigned yet. */
	qlonglong m_DbRowId;

	QString m_DisplayName;
	QString m_Notes;
	std::vector<FilterPtr> m_Items;

	/** The background color to use when displaying this template. */
	QColor m_BgColor;
};

using TemplatePtr = std::shared_ptr<Template>;

Q_DECLARE_METATYPE(TemplatePtr);





/** Helper class that implements exporting multiple templates into an XML format. */
class TemplateXmlExport
{
public:

	/** Exports the specified templates to a XML format.
	Returns the exported data as a byte array. */
	static QByteArray run(const std::vector<TemplatePtr> & a_Templates);


protected:

	/** The DOM document representation used for building the XML. */
	QDomDocument m_Document;


	/** Creates a new instance of this class, initializes the internal state. */
	TemplateXmlExport();

	/** Exports the specified templates to XML format, returns the data as a byte array. */
	QByteArray exportAll(const std::vector<TemplatePtr> & a_Templates);

	/** Adds the specified filter node to m_Document as a child of the specified DOM element. */
	void addNodeToDom(const Filter::NodePtr && a_Node, QDomElement & a_ParentElement);
};





/** Helper class that implements importing templates from an XML format. */
class TemplateXmlImport
{
public:

	/** Imports the templates from the specified XML-formatted data.
	Returns an empty vector on error.
	Note that the caller should unify the contained filters with their own filters prior to merging
	the returned templates into their Database. */
	static std::vector<TemplatePtr> run(const QByteArray & a_XmlData);


protected:

	/** The DOM document representation of the input data. */
	QDomDocument m_Document;


	/** Creates a new instance of this class. */
	TemplateXmlImport();

	/** Imports the templates from the specified XML-formatted data.
	Returns an empty vector on error. */
	std::vector<TemplatePtr> importAll(const QByteArray & a_XmlData);

	/** Reads and returns a Template from the specified XML element.
	a_TemplateXmlElement is supposed to represent the <SkauTanTemplate> element in the XML. */
	TemplatePtr readTemplate(const QDomElement & a_TemplateXmlElement);

	/** Reads and returns a FilterPtr from the specified XML element.
	a_ItemXmlElement is supposed to represent the <item> element in the XML.
	The returned filter is always a new unique object. */
	FilterPtr readTemplateItem(const QDomElement & a_ItemXmlElement);

	/** Reads and returns a Node from the specified XML element.
	a_FilterXmlElement is supposed to represent the <and>, <or> or <comparison> element in the XML. */
	Filter::NodePtr readTemplateFilterNode(const QDomElement & a_NodeXmlElement);
};





#endif // TEMPLATE_H

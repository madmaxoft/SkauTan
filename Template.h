#ifndef TEMPLATE_H
#define TEMPLATE_H





#include <memory>
#include <vector>
#include <QVariant>





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

	// fwd:
	class Filter;
	class Item;
	using FilterPtr = std::shared_ptr<Filter>;
	using ItemPtr = std::shared_ptr<Item>;


	/** Represents a filter, which a song must satisfy in order to be considered
	for an Item when adding the template item to playlist.
	A filter is either a comparison of a single song property to a fixed value,
	or a logical operation over multiple sub-filters. */
	class Filter:
		public std::enable_shared_from_this<Filter>
	{
	public:

		/** Kind of the filter - either a comparison or a logical combination.
		Stored in the DB as a number, must keep numeric values across DB versions. */
		enum Kind
		{
			fkComparison = 0,  // Direct comparison to a Song's parameter
			fkAnd        = 1,
			fkOr         = 2,
		};

		/** The comparison between the song property and the fixed value.
		Stored in the DB as a number, must keep numeric values across DB versions. */
		enum Comparison
		{
			fcEqual              = 0,
			fcNotEqual           = 1,
			fcContains           = 2,
			fcNotContains        = 3,
			fcGreaterThan        = 4,
			fcGreaterThanOrEqual = 5,
			fcLowerThan          = 6,
			fcLowerThanOrEqual   = 7,
		};

		/** The property of the Song object which is used for the comparison (fkComparison only)
		Stored in the DB as a number, must keep numeric values across DB versions. */
		enum SongProperty
		{
			fspAuthor            = 0,
			fspTitle             = 1,
			fspGenre             = 2,
			fspLength            = 3,
			fspMeasuresPerMinute = 4,
			fspRating            = 5,
			fspLastPlayed        = 6,
		};


		/** Creates a "match-all" filter: [fkComparison, fcGreaterThanOrEqual, fpLength, 0] */
		Filter();

		/** Creates a "comparison" filter, with the specified comparison pre-set. */
		Filter(SongProperty a_SongProperty, Comparison a_Comparison, QVariant a_Value);

		/** Create a "combination" filter over the specified sub-filters. */
		Filter(Kind a_Combination, const std::vector<FilterPtr> a_SubFilters = {});

		/** Returns a clone of this filter, including a clone of its subtree. */
		FilterPtr clone() const;

		Filter * parent() const { return m_Parent; }
		Kind kind() const { return m_Kind; }

		/** Returns all direct sub-filters of this filter.
		Only valid with fkAnd or fkOr filters. */
		const std::vector<FilterPtr> & children() const;

		/** Returns the song property that is to be compared.
		Only valid for fkComparison filters. */
		SongProperty songProperty() const;

		/** Returns the comparison to be performed on the song property.
		Only valid for fkComparison filters. */
		Comparison comparison() const;

		/** Returns the value against which the song property is compared.
		Only valid for fkComparison filters. */
		QVariant value() const;

		/** Updates the filter kind.
		If moving from a combinator to a comparator, clears the children. */
		void setKind(Kind a_Kind);

		// Setters for the comparators; assert when not a comparator:
		void setSongProperty(SongProperty a_SongProperty);
		void setComparison(Comparison a_Comparison);
		void setValue(QVariant a_Value);

		/** Returns true if the specified song satisfies this filter. */
		bool isSatisfiedBy(const Song & a_Song) const;

		/** Returns true if this filter can have children (based on its m_Kind). */
		bool canHaveChildren() const;

		/** Adds the specified sub-filter to m_Children.
		Only valid with fkAnd and fkOr filters. */
		void addChild(FilterPtr a_Child);

		/** Replaces the specified existing child with the specified new child.
		The old child first gets its parent reset to nullptr, then its SharedPtr is dropped.
		The new child's parent is set to this.
		Only valid with fkOr and fkAnd filters. */
		void replaceChild(Filter * a_ExistingChild, FilterPtr a_NewChild);

		/** Removes the specified child.
		The child first gets its parent reset to nullptr, then its SharedPtr is dropped.
		Only valid with fkOr and fkAnd filters. */
		void removeChild(const Filter * a_ExistingChild);

		/** Checks that this filter's children have their parent set to this, and recurses into them. */
		void checkConsistency() const;

		/** Returns a human-readable caption for this specific filter (not considering sub-filters).
		Returns either the comparison as "Length >= 0", or the combinator as "And" etc. */
		QString getCaption() const;

		/** Returns a human-readable description of the filter, including its possible sub-filters. */
		QString getDescription() const;

		/** Returns the Kind that matches the specified integer value.
		A std::runtime_error is thrown for unrecognized values. */
		static Kind intToKind(int a_Kind);

		/** Returns the Comparison that matches the specified integer value.
		A std::runtime_error is thrown for unrecognized values. */
		static Comparison intToComparison(int a_Comparison);

		/** Returns the Comparison that matches the specified integer value.
		A std::runtime_error is thrown for unrecognized values. */
		static SongProperty intToSongProperty(int a_SongProperty);

		/** Returns the user-visible string representation of the specified SongProperty. */
		static QString songPropertyCaption(SongProperty a_Prop);

		/** Returns the user-visible string representation of the specified Comparison. */
		static QString comparisonCaption(Comparison a_Comparison);


	protected:

		/** Template::Item may access the setParent() function*/
		friend class Template::Item;

		/** Sets the parent of this filter.
		Only available to close relative class - Template::Item. */
		void setParent(Filter * a_Parent) { m_Parent = a_Parent; }

		/** Returns the concatenated descriptions of the children, using the specified separator.
		Assumes that the filter is fkAnd or fkOr. */
		QString concatChildrenDescriptions(const QString & a_Separator) const;

		/** Returns true if the comparison in this filter is satisfied by the song.
		Asserts if this filter is not a fkComparison. */
		bool isComparisonSatisfiedBy(const Song & a_Song) const;

		/** Returns true if the comparison in this filter is satisfied by the specified value.
		Asserts if this filter is not a fkComparison. */
		bool isStringComparisonSatisfiedBy(const QString & a_Value) const;

		/** Returns true if the comparison in this filter is satisfied by the specified value.
		Asserts if this filter is not a fkComparison. */
		bool isNumberComparisonSatisfiedBy(double a_Value) const;

		/** Returns true if the comparison in this filter is satisfied by the specified value.
		Asserts if this filter is not a fkComparison. */
		bool isDateComparisonSatisfiedBy(QDateTime a_Value) const;


	private:

		/** The parent of this filter. */
		Filter * m_Parent;

		Kind m_Kind;

		// For combination of subfilters (fkAnd, fkOr):
		std::vector<FilterPtr> m_Children;

		// For direct comparison (fkComparison):
		SongProperty m_SongProperty;
		Comparison m_Comparison;
		QVariant m_Value;
	};



	/** Represents  a single item in the template.
	Based on the description in this class, one item will be added to the playlist upon request. */
	class Item
	{
	public:
		Item(const QString & a_DisplayName, const QString & a_Notes, bool a_IsFavorite);

		/** Returns a clone of this item, with IsFavorite set to false. */
		ItemPtr clone() const;

		const QString & displayName() const { return m_DisplayName; }
		const QString & notes() const { return m_Notes; }
		bool isFavorite() const { return m_IsFavorite; }
		FilterPtr filter() const { return m_Filter; }

		/** Returns the user-visible description of the entire filter. */
		QString getFilterDescription() const { return m_Filter->getDescription(); }

		void setDisplayName(const QString & a_DisplayName) { m_DisplayName = a_DisplayName; }
		void setNotes(const QString & a_Notes) { m_Notes = a_Notes; }
		void setIsFavorite(bool a_IsFavorite) { m_IsFavorite = a_IsFavorite; }
		void setFilter(FilterPtr a_Filter) { m_Filter = a_Filter; m_Filter->setParent(nullptr); }

		/** Sets the filter to a noop filter.
		Used when loader detects invalid filter. */
		void setNoopFilter();

		/** Checks the entire filter chain for consistency in its parent fields. */
		void checkFilterConsistency() const;


	protected:

		/** The display name assigned to this item, if any. */
		QString m_DisplayName;

		/** User notes for the item. */
		QString m_Notes;

		/** Items that are marked as Favorite are added to the "add favorite" menu. */
		bool m_IsFavorite;

		/** The filter on which this item is filled. */
		FilterPtr m_Filter;
	};



	/** Creates a new empty template. */
	Template(qlonglong a_DbRowId);

	/** Creates a new empty template with the specified values. */
	Template(qlonglong a_DbRowId, QString && a_DisplayName, QString && a_Notes);

	// Simple getters:
	qlonglong dbRowId() const { return m_DbRowId; }
	const QString & displayName() const { return m_DisplayName; }
	const QString & notes() const { return m_Notes; }
	const std::vector<ItemPtr> & items() const { return m_Items; }

	// Simple setters:
	void setDisplayName(const QString & a_DisplayName) { m_DisplayName = a_DisplayName; }
	void setNotes(const QString & a_Notes) { m_Notes = a_Notes; }

	/** Adds a new item at the specified index (-1 = at the end). */
	ItemPtr addItem(
		const QString & a_DisplayName,
		const QString & a_Notes,
		bool a_IsFavorite,
		int a_DstIndex = -1
	);

	/** Adds the specified item at the end of the item list. */
	void appendExistingItem(ItemPtr a_Item);

	/** Moves an item from the specified source index to the specified destination index.
	Asserts if the indices are not valid. */
	void moveItem(int a_SrcIndex, int a_DstIndex);

	/** Deletes the item at the specified index. */
	void delItem(int a_Index);

	/** Adds the items to the specified playlist. a_Songs is the song database from which to choose songs. */
	void addToPlaylist(Playlist & a_Dst, Database & a_Songs);

protected:

	/** The RowID in the DB. */
	qlonglong m_DbRowId;

	QString m_DisplayName;
	QString m_Notes;
	std::vector<ItemPtr> m_Items;
};

using TemplatePtr = std::shared_ptr<Template>;





#endif // TEMPLATE_H
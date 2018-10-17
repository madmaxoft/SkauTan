#ifndef FILTER_H
#define FILTER_H





#include <memory>
#include <QVariant>
#include <QColor>
#include "DatedOptional.h"





// fwd:
class Filter;
class Song;
using FilterPtr = std::shared_ptr<Filter>;





/** Represents  a single filter for creating templates.
Based on the description in this class, one item will be added to the playlist upon request. */
class Filter:
	public QObject,
	public std::enable_shared_from_this<Filter>
{
public:


	// fwd:
	class Node;
	using NodePtr = std::shared_ptr<Node>;


	/** Represents a filter node, out of which a tree is built for filtering the songs.
	A node is either a comparison of a single song property to a fixed value,
	or a logical operation over multiple sub-nodes. */
	class Node:
		public std::enable_shared_from_this<Node>
	{
	public:

		/** Kind of the filter - either a comparison or a logical combination.
		Stored in the DB as a number, must keep numeric values across DB versions. */
		enum Kind
		{
			nkComparison = 0,  // Direct comparison to a Song's parameter
			nkAnd        = 1,
			nkOr         = 2,
		};

		/** The comparison between the song property and the fixed value.
		Stored in the DB as a number, must keep numeric values across DB versions. */
		enum Comparison
		{
			ncEqual              = 0,
			ncNotEqual           = 1,
			ncContains           = 2,
			ncNotContains        = 3,
			ncGreaterThan        = 4,
			ncGreaterThanOrEqual = 5,
			ncLowerThan          = 6,
			ncLowerThanOrEqual   = 7,
		};

		/** The property of the Song object which is used for the comparison (nkComparison only)
		Stored in the DB and XML as a number, must keep numeric values across DB versions. */
		enum SongProperty
		{
			nspAuthor                    = 0,  // ANY author tag
			nspTitle                     = 1,  // ANY title tag
			nspGenre                     = 2,  // ANY genre tag
			nspLength                    = 3,
			nspMeasuresPerMinute         = 4,  // ANY mpm tag
			nspLocalRating               = 5,
			nspLastPlayed                = 6,
			nspManualAuthor              = 7,
			nspManualTitle               = 8,
			nspManualGenre               = 9,
			nspManualMeasuresPerMinute   = 10,
			nspFileNameAuthor            = 11,
			nspFileNameTitle             = 12,
			nspFileNameGenre             = 13,
			nspFileNameMeasuresPerMinute = 14,
			nspId3Author                 = 15,
			nspId3Title                  = 16,
			nspId3Genre                  = 17,
			nspId3MeasuresPerMinute      = 18,
			nspPrimaryAuthor             = 19,  // The first of Manual-, Id3-, FileName-Author that is non-null
			nspPrimaryTitle              = 20,  // The first of Manual-, Id3-, FileName-Title that is non-null
			nspPrimaryGenre              = 21,  // The first of Manual-, Id3-, FileName-Genre that is non-null
			nspPrimaryMeasuresPerMinute  = 22,  // The first of Manual-, Id3-, FileName-MPM that is non-null
			nspWarningCount              = 23,
			nspRatingRhythmClarity       = 24,
			nspRatingGenreTypicality     = 25,
			nspRatingPopularity          = 26,
			nspNotes                     = 27,
		};


		/** Creates a "match-all" node: [nkComparison, ncGreaterThanOrEqual, nspLength, 0] */
		Node();

		/** Creates a "comparison" node, with the specified comparison pre-set. */
		Node(SongProperty a_SongProperty, Comparison a_Comparison, QVariant a_Value);

		/** Create a "combination" node over the specified sub-nodes. */
		Node(Kind a_Combination, const std::vector<NodePtr> a_SubFilters = {});

		/** Returns a clone of this node, including a clone of its subtree. */
		NodePtr clone() const;

		Node * parent() const { return m_Parent; }
		Kind kind() const { return m_Kind; }

		/** Returns all direct sub-nodes of this node.
		Only valid with nkAnd or nkOr nodes. */
		const std::vector<NodePtr> & children() const;

		/** Returns the song property that is to be compared.
		Only valid for nkComparison nodes. */
		SongProperty songProperty() const;

		/** Returns the comparison to be performed on the song property.
		Only valid for nkComparison nodes. */
		Comparison comparison() const;

		/** Returns the value against which the song property is compared.
		Only valid for nkComparison nodes. */
		QVariant value() const;

		/** Updates the node kind.
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
		Only valid with nkAnd and nkOr nodes. */
		void addChild(NodePtr a_Child);

		/** Replaces the specified existing child with the specified new child.
		The old child first gets its parent reset to nullptr, then its SharedPtr is dropped.
		The new child's parent is set to this.
		Only valid with nkOr and nkAnd nodes. */
		void replaceChild(Node * a_ExistingChild, NodePtr a_NewChild);

		/** Removes the specified child.
		The child first gets its parent reset to nullptr, then its SharedPtr is dropped.
		Only valid with nkOr and nkAnd nodes. */
		void removeChild(const Node * a_ExistingChild);

		/** Checks that this node's children have their parent set to this, and recurses into them. */
		void checkConsistency() const;

		/** Returns a human-readable caption for this specific node (not considering sub-nodes).
		Returns either the comparison as "Length >= 0", or the combinator as "And" etc. */
		QString getCaption() const;

		/** Returns a human-readable description of the node, including its possible sub-nodes. */
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

		friend class Filter;

		/** Sets the parent of this filter.
		Only accessible to the Filter parent class. */
		void setParent(Node * a_Parent) { m_Parent = a_Parent; }

		/** Returns the concatenated descriptions of the children, using the specified separator.
		Assumes that the filter is nkAnd or nkOr. */
		QString concatChildrenDescriptions(const QString & a_Separator) const;

		/** Returns true if the comparison in this node is satisfied by the song.
		Asserts if this node is not a nkComparison. */
		bool isComparisonSatisfiedBy(const Song & a_Song) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isStringComparisonSatisfiedBy(const DatedOptional<QString> & a_Value) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isNumberComparisonSatisfiedBy(const DatedOptional<double> & a_Value) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isNumberComparisonSatisfiedBy(const QVariant & a_Value) const;

		/** Returns true if the comparison in this node is satistied by the specified number.
		Asserts is this node is not a nkComparison. */
		bool isValidNumberComparisonSatisfiedBy(double a_Value) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isDateComparisonSatisfiedBy(QDateTime a_Value) const;

		/** Returns the hash of the node (including children),
		trying to identify the node uniquely (-enough). */
		QByteArray hash() const;


	private:

		/** The parent of this node. */
		Node * m_Parent;

		Kind m_Kind;

		// For combination of sub-nodes (nkAnd, nkOr):
		std::vector<NodePtr> m_Children;

		// For direct comparison (nkComparison):
		SongProperty m_SongProperty;
		Comparison m_Comparison;
		QVariant m_Value;
	};



	/** Creates an empty filter. */
	Filter():
		m_DbRowId(-1),
		m_IsFavorite(false)
	{
	}


	/** Creates an empty filter with all the fields set up. */
	Filter(
		qlonglong a_DbRowId,
		const QString & a_DisplayName,
		const QString & a_Notes,
		bool a_IsFavorite,
		QColor a_BgColor,
		const DatedOptional<double> & a_DurationLimit
	);

	qlonglong dbRowId() const { return m_DbRowId; }
	const QString & displayName() const { return m_DisplayName; }
	const QString & notes() const { return m_Notes; }
	bool isFavorite() const { return m_IsFavorite; }
	NodePtr rootNode() const { return m_RootNode; }
	const QColor & bgColor() const { return m_BgColor; }
	const DatedOptional<double> durationLimit() const { return m_DurationLimit; }

	/** Returns the user-visible description of the entire filter. */
	QString getFilterDescription() const { return m_RootNode->getDescription(); }

	void setDisplayName(const QString & a_DisplayName) { m_DisplayName = a_DisplayName; }
	void setNotes(const QString & a_Notes) { m_Notes = a_Notes; }
	void setIsFavorite(bool a_IsFavorite) { m_IsFavorite = a_IsFavorite; }
	void setRootNode(NodePtr a_RootNode) { m_RootNode = a_RootNode; m_RootNode->setParent(nullptr); }
	void setBgColor(const QColor & a_BgColor) { m_BgColor = a_BgColor; }
	void setDurationLimit(double a_Seconds) { m_DurationLimit = a_Seconds; }

	/** Remove the duration limit. */
	void resetDurationLimit() { m_DurationLimit.reset(); }

	/** Sets the filter to a noop root Node.
	Used when loader detects invalid filter. */
	void setNoopFilter();

	/** Checks the entire node chain for consistency in its parent fields. */
	void checkNodeConsistency() const;

	/** Returns the hash of the filter, trying to identify the filter uniquely (-enough). */
	QByteArray hash() const;


protected:

	// Allow access to setDbRowId()
	friend class Database;


	/** The DB RowId where this filter is stored. */
	qlonglong m_DbRowId;

	/** The display name assigned to this filter. */
	QString m_DisplayName;

	/** User notes for the filter. */
	QString m_Notes;

	/** Items that are marked as Favorite are added to the quick-add list. */
	bool m_IsFavorite;

	/** The root Node on which this filter operates. */
	NodePtr m_RootNode;

	/** The color to use for background when displaying this filter. */
	QColor m_BgColor;

	/** An optional limit for duration.
	If set, the playlist item created from this will inherit the limit. */
	DatedOptional<double> m_DurationLimit;


	/** Sets the DB RowID.
	Asserts that the RowID hasn't been set before (can only be called once).
	Only to be called from within Database. */
	void setDbRowId(qlonglong a_DbRowId);
};

Q_DECLARE_METATYPE(FilterPtr);





#endif  // FILTER_H

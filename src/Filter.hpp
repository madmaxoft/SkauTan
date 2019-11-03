#pragma once

#include <memory>
#include <QVariant>
#include <QColor>
#include "DatedOptional.hpp"





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
	Q_OBJECT

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
			nspPrimaryMeasuresPerMinute  = 22,  // The first of Manual-, Id3-, FileName- or Detected-MPM that is non-null
			nspWarningCount              = 23,
			nspRatingRhythmClarity       = 24,
			nspRatingGenreTypicality     = 25,
			nspRatingPopularity          = 26,
			nspNotes                     = 27,
			nspDetectedTempo             = 28,
		};


		/** Creates a "match-all" node: [nkComparison, ncGreaterThanOrEqual, nspLength, 0] */
		Node();

		/** Creates a "comparison" node, with the specified comparison pre-set. */
		Node(SongProperty aSongProperty, Comparison aComparison, QVariant aValue);

		/** Create a "combination" node over the specified sub-nodes. */
		Node(Kind aCombination, const std::vector<NodePtr> aSubFilters = {});

		/** Returns a clone of this node, including a clone of its subtree. */
		NodePtr clone() const;

		Node * parent() const { return mParent; }
		Kind kind() const { return mKind; }

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
		void setKind(Kind aKind);

		// Setters for the comparators; assert when not a comparator:
		void setSongProperty(SongProperty aSongProperty);
		void setComparison(Comparison aComparison);
		void setValue(QVariant aValue);

		/** Returns true if the specified song satisfies this filter. */
		bool isSatisfiedBy(const Song & aSong) const;

		/** Returns true if this filter can have children (based on its mKind). */
		bool canHaveChildren() const;

		/** Adds the specified sub-filter to mChildren.
		Only valid with nkAnd and nkOr nodes. */
		void addChild(NodePtr aChild);

		/** Replaces the specified existing child with the specified new child.
		The old child first gets its parent reset to nullptr, then its SharedPtr is dropped.
		The new child's parent is set to this.
		Only valid with nkOr and nkAnd nodes. */
		void replaceChild(Node * aExistingChild, NodePtr aNewChild);

		/** Removes the specified child.
		The child first gets its parent reset to nullptr, then its SharedPtr is dropped.
		Only valid with nkOr and nkAnd nodes. */
		void removeChild(const Node * aExistingChild);

		/** Checks that this node's children have their parent set to this, and recurses into them. */
		void checkConsistency() const;

		/** Returns a human-readable caption for this specific node (not considering sub-nodes).
		Returns either the comparison as "Length >= 0", or the combinator as "And" etc. */
		QString getCaption() const;

		/** Returns a human-readable description of the node, including its possible sub-nodes. */
		QString getDescription() const;

		/** Returns the Kind that matches the specified integer value.
		A std::runtime_error is thrown for unrecognized values. */
		static Kind intToKind(int aKind);

		/** Returns the Comparison that matches the specified integer value.
		A std::runtime_error is thrown for unrecognized values. */
		static Comparison intToComparison(int aComparison);

		/** Returns the Comparison that matches the specified integer value.
		A std::runtime_error is thrown for unrecognized values. */
		static SongProperty intToSongProperty(int aSongProperty);

		/** Returns the user-visible string representation of the specified SongProperty. */
		static QString songPropertyCaption(SongProperty aProp);

		/** Returns the user-visible string representation of the specified Comparison. */
		static QString comparisonCaption(Comparison aComparison);


	protected:

		friend class Filter;

		/** Sets the parent of this filter.
		Only accessible to the Filter parent class. */
		void setParent(Node * aParent) { mParent = aParent; }

		/** Returns the concatenated descriptions of the children, using the specified separator.
		Assumes that the filter is nkAnd or nkOr. */
		QString concatChildrenDescriptions(const QString & aSeparator) const;

		/** Returns true if the comparison in this node is satisfied by the song.
		Asserts if this node is not a nkComparison. */
		bool isComparisonSatisfiedBy(const Song & aSong) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isStringComparisonSatisfiedBy(const DatedOptional<QString> & aValue) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isNumberComparisonSatisfiedBy(const DatedOptional<double> & aValue) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isNumberComparisonSatisfiedBy(const QVariant & aValue) const;

		/** Returns true if the comparison in this node is satistied by the specified number.
		Asserts is this node is not a nkComparison. */
		bool isValidNumberComparisonSatisfiedBy(double aValue) const;

		/** Returns true if the comparison in this node is satisfied by the specified value.
		Asserts if this node is not a nkComparison. */
		bool isDateComparisonSatisfiedBy(QDateTime aValue) const;

		/** Returns the hash of the node (including children),
		trying to identify the node uniquely (-enough). */
		QByteArray hash() const;


	private:

		/** The parent of this node. */
		Node * mParent;

		Kind mKind;

		// For combination of sub-nodes (nkAnd, nkOr):
		std::vector<NodePtr> mChildren;

		// For direct comparison (nkComparison):
		SongProperty mSongProperty;
		Comparison mComparison;
		QVariant mValue;
	};



	/** Creates an empty filter with white bgcolor and a Noop node tree. */
	Filter();

	/** Creates a filter with all the fields set up and a Noop node tree. */
	Filter(
		qlonglong aDbRowId,
		const QString & aDisplayName,
		const QString & aNotes,
		bool aIsFavorite,
		QColor aBgColor,
		const DatedOptional<double> & aDurationLimit
	);

	/** Creates a new filter as a copy of the specified filter, with a clone of the node tree.
	Doesn't copy the DB RowID, -1 is assigned instead. */
	Filter(const Filter & aCopyFrom);

	qlonglong dbRowId() const { return mDbRowId; }
	const QString & displayName() const { return mDisplayName; }
	const QString & notes() const { return mNotes; }
	bool isFavorite() const { return mIsFavorite; }
	NodePtr rootNode() const { return mRootNode; }
	const QColor & bgColor() const { return mBgColor; }
	const DatedOptional<double> durationLimit() const { return mDurationLimit; }

	/** Returns the user-visible description of the entire filter. */
	QString getFilterDescription() const { return mRootNode->getDescription(); }

	void setDisplayName(const QString & aDisplayName) { mDisplayName = aDisplayName; }
	void setNotes(const QString & aNotes) { mNotes = aNotes; }
	void setIsFavorite(bool aIsFavorite) { mIsFavorite = aIsFavorite; }
	void setRootNode(NodePtr aRootNode) { mRootNode = aRootNode; mRootNode->setParent(nullptr); }
	void setBgColor(const QColor & aBgColor) { mBgColor = aBgColor; }
	void setDurationLimit(double aSeconds) { mDurationLimit = aSeconds; }

	/** Remove the duration limit. */
	void resetDurationLimit() { mDurationLimit.reset(); }

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
	qlonglong mDbRowId;

	/** The display name assigned to this filter. */
	QString mDisplayName;

	/** User notes for the filter. */
	QString mNotes;

	/** Items that are marked as Favorite are added to the quick-add list. */
	bool mIsFavorite;

	/** The root Node on which this filter operates. */
	NodePtr mRootNode;

	/** The color to use for background when displaying this filter. */
	QColor mBgColor;

	/** An optional limit for duration.
	If set, the playlist item created from this will inherit the limit. */
	DatedOptional<double> mDurationLimit;


	/** Sets the DB RowID.
	Asserts that the RowID hasn't been set before (can only be called once).
	Only to be called from within Database. */
	void setDbRowId(qlonglong aDbRowId);
};

Q_DECLARE_METATYPE(FilterPtr);

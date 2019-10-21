#include "DlgEditFilter.hpp"
#include "ui_DlgEditFilter.h"
#include <cassert>
#include <QStyledItemDelegate>
#include <QDebug>
#include <QMessageBox>
#include <QComboBox>
#include <QColorDialog>
#include <QAbstractItemModel>
#include "../../DB/Database.hpp"
#include "../../Settings.hpp"
#include "../../Utils.hpp"
#include "DlgSongs.hpp"





#define ARRAYCOUNT(X) (sizeof(X) / sizeof(*(X)))





static const auto roleFilterNodePtr = Qt::UserRole + 1;

static Filter::Node::SongProperty g_SongProperties[] =
{
	Filter::Node::nspAuthor,
	Filter::Node::nspTitle,
	Filter::Node::nspGenre,
	Filter::Node::nspLength,
	Filter::Node::nspMeasuresPerMinute,
	Filter::Node::nspLastPlayed,
	Filter::Node::nspManualAuthor,
	Filter::Node::nspManualTitle,
	Filter::Node::nspManualGenre,
	Filter::Node::nspManualMeasuresPerMinute,
	Filter::Node::nspFileNameAuthor,
	Filter::Node::nspFileNameTitle,
	Filter::Node::nspFileNameGenre,
	Filter::Node::nspFileNameMeasuresPerMinute,
	Filter::Node::nspId3Author,
	Filter::Node::nspId3Title,
	Filter::Node::nspId3Genre,
	Filter::Node::nspId3MeasuresPerMinute,
	Filter::Node::nspPrimaryAuthor,
	Filter::Node::nspPrimaryTitle,
	Filter::Node::nspPrimaryGenre,
	Filter::Node::nspPrimaryMeasuresPerMinute,
	Filter::Node::nspWarningCount,
	Filter::Node::nspLocalRating,
	Filter::Node::nspRatingRhythmClarity,
	Filter::Node::nspRatingGenreTypicality,
	Filter::Node::nspRatingPopularity,
	Filter::Node::nspNotes,
	Filter::Node::nspDetectedTempo,
};

static Filter::Node::Comparison g_Comparisons[] =
{
	Filter::Node::ncEqual,
	Filter::Node::ncNotEqual,
	Filter::Node::ncContains,
	Filter::Node::ncNotContains,
	Filter::Node::ncGreaterThan,
	Filter::Node::ncGreaterThanOrEqual,
	Filter::Node::ncLowerThan,
	Filter::Node::ncLowerThanOrEqual,
};





/** Returns the index into g_SongProperties that has the same property as the specified filter.
Returns -1 if not found. */
static int indexFromSongProperty(const Filter::Node & aNode)
{
	auto prop = aNode.songProperty();
	for (size_t i = 0; i < ARRAYCOUNT(g_SongProperties); ++i)
	{
		if (g_SongProperties[i] == prop)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}





static int indexFromComparison(const Filter::Node & aNode)
{
	auto cmp = aNode.comparison();
	for (size_t i = 0; i < ARRAYCOUNT(g_Comparisons); ++i)
	{
		if (g_Comparisons[i] == cmp)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}





////////////////////////////////////////////////////////////////////////////////
// FilterModel:

/** A filter model that applies the specified Template::Filter onto the underlying SongModel. */
class SongFilterModel:
	public QSortFilterProxyModel
{
public:
	SongFilterModel(const Filter & aFilter):
		mFilter(aFilter)
	{
	}


protected:

	/** The filter to be applied. */
	const Filter & mFilter;


	virtual bool filterAcceptsRow(int aSourceRow, const QModelIndex & aSourceParent) const override
	{
		if (aSourceParent.isValid())
		{
			assert(!"This filter should not be used for multi-level data");
			return false;
		}

		auto idx = sourceModel()->index(aSourceRow, 0, aSourceParent);
		auto song = sourceModel()->data(idx, SongModel::roleSongPtr).value<SongPtr>();
		if (song == nullptr)
		{
			qWarning() << "Underlying model returned nullptr song for row " << aSourceRow;
			assert(!"Unexpected nullptr song");
			return false;
		}
		return mFilter.rootNode()->isSatisfiedBy(*song);
	}
};





////////////////////////////////////////////////////////////////////////////////
// FilterNodeDelegate:

/** UI helper that provides editor for the filter node tree. */
class FilterNodeDelegate:
	public QStyledItemDelegate
{
	using Super = QStyledItemDelegate;

public:
	FilterNodeDelegate(){}



	// Add the combobox to size:
	virtual QSize sizeHint(
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) const override
	{
		Q_UNUSED(aIndex);
		auto widget = aOption.widget;
		auto style = (widget != nullptr) ? widget->style() : QApplication::style();
		return style->sizeFromContents(QStyle::CT_ComboBox, &aOption, Super::sizeHint(aOption, aIndex), widget);
	}



	// editing
	virtual QWidget * createEditor(
		QWidget * aParent,
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) const override
	{
		Q_UNUSED(aOption);

		// HACK: TreeWidget stores its QTreeWidgetItem ptrs in the index's internalPtr:
		auto item = reinterpret_cast<const QTreeWidgetItem *>(aIndex.internalPointer());

		auto node = reinterpret_cast<Filter::Node *>(item->data(0, roleFilterNodePtr).toULongLong());

		if (node->canHaveChildren())
		{
			// This is a combinator, use a single combobox as the editor:
			auto res = new QComboBox(aParent);
			res->setEditable(false);
			res->setProperty("nodePtr", reinterpret_cast<qulonglong>(node));
			res->addItems({
				DlgEditFilter::tr("And", "FilterTreeEditor"),
				DlgEditFilter::tr("Or", "FilterTreeEditor")
			});
			return res;
		}
		else
		{
			// This is a comparison filter, use a layout for the editor
			auto editor = new QFrame(aParent);
			editor->setFrameShape(QFrame::Panel);
			editor->setLineWidth(0);
			editor->setFrameShadow(QFrame::Plain);
			auto layout = new QHBoxLayout(editor);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setMargin(0);
			auto cbProp = new QComboBox();
			auto cbComparison = new QComboBox();
			auto leValue = new QLineEdit();
			for (const auto sp: g_SongProperties)
			{
				cbProp->addItem(Filter::Node::songPropertyCaption(sp));
			}
			cbProp->setMaxVisibleItems(static_cast<int>(sizeof(g_SongProperties) / sizeof(*g_SongProperties)));
			for (const auto cmp: g_Comparisons)
			{
				cbComparison->addItem(Filter::Node::comparisonCaption(cmp));
			}
			cbComparison->setMaxVisibleItems(static_cast<int>(sizeof(g_Comparisons) / sizeof(*g_Comparisons)));
			layout->addWidget(cbProp);
			layout->addWidget(cbComparison);
			layout->addWidget(leValue);
			editor->setProperty("cbProp",       QVariant::fromValue(cbProp));
			editor->setProperty("cbComparison", QVariant::fromValue(cbComparison));
			editor->setProperty("leValue",      QVariant::fromValue(leValue));
			editor->setProperty("nodePtr",      reinterpret_cast<qulonglong>(node));
			return editor;
		}
	}



	virtual void setEditorData(QWidget * aEditor, const QModelIndex & aIndex) const override
	{
		Q_UNUSED(aIndex);
		auto filter = reinterpret_cast<const Filter::Node *>(aEditor->property("nodePtr").toULongLong());
		assert(filter != nullptr);
		if (filter->canHaveChildren())
		{
			auto cb = reinterpret_cast<QComboBox *>(aEditor);
			assert(cb != nullptr);
			cb->setCurrentIndex((filter->kind() == Filter::Node::nkOr) ? 1 : 0);
		}
		else
		{
			auto cbProp       = aEditor->property("cbProp").value<QComboBox *>();
			auto cbComparison = aEditor->property("cbComparison").value<QComboBox *>();
			auto leValue      = aEditor->property("leValue").value<QLineEdit *>();
			assert(cbProp != nullptr);
			assert(cbComparison != nullptr);
			assert(leValue != nullptr);
			cbProp->setCurrentIndex(indexFromSongProperty(*filter));
			cbComparison->setCurrentIndex(indexFromComparison(*filter));
			leValue->setText(filter->value().toString());
		}
	}



	virtual void setModelData(
		QWidget * aEditor,
		QAbstractItemModel * aModel,
		const QModelIndex & aIndex
	) const override
	{
		Q_UNUSED(aModel);
		Q_UNUSED(aIndex);

		auto node = reinterpret_cast<Filter::Node *>(aEditor->property("nodePtr").toULongLong());
		assert(node != nullptr);
		if (node->canHaveChildren())
		{
			auto cb = reinterpret_cast<QComboBox *>(aEditor);
			assert(cb != nullptr);
			node->setKind((cb->currentIndex() == 0) ? Filter::Node::nkAnd : Filter::Node::nkOr);
		}
		else
		{
			auto cbProp       = aEditor->property("cbProp").value<QComboBox *>();
			auto cbComparison = aEditor->property("cbComparison").value<QComboBox *>();
			auto leValue      = aEditor->property("leValue").value<QLineEdit *>();
			assert(cbProp != nullptr);
			assert(cbComparison != nullptr);
			assert(leValue != nullptr);
			auto propIdx = cbProp->currentIndex();
			if (propIdx >= 0)
			{
				node->setSongProperty(g_SongProperties[propIdx]);
			}
			auto cmpIdx = cbComparison->currentIndex();
			if (cmpIdx >= 0)
			{
				node->setComparison(g_Comparisons[cmpIdx]);
			}
			node->setValue(leValue->text());
		}
		aModel->setData(aIndex, node->getCaption());
	}
};





////////////////////////////////////////////////////////////////////////////////
// DlgEditTemplateItem:

DlgEditFilter::DlgEditFilter(
	ComponentCollection & aComponents,
	Filter & aFilter,
	QWidget * aParent
):
	Super(aParent),
	mComponents(aComponents),
	mFilter(aFilter),
	mUI(new Ui::DlgEditFilter)
{
	mFilter.checkNodeConsistency();
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgEditFilter", *this);
	mUI->twNodes->setItemDelegate(new FilterNodeDelegate);

	// Connect the signals:
	connect(mUI->btnClose,            &QPushButton::clicked,   this, &DlgEditFilter::saveAndClose);
	connect(mUI->btnAddSibling,       &QPushButton::clicked,   this, &DlgEditFilter::addNodeSibling);
	connect(mUI->btnInsertCombinator, &QPushButton::clicked,   this, &DlgEditFilter::insertNodeCombinator);
	connect(mUI->btnRemoveFilter,     &QPushButton::clicked,   this, &DlgEditFilter::removeNode);
	connect(mUI->btnPreview,          &QPushButton::clicked,   this, &DlgEditFilter::previewFilter);
	connect(mUI->leBgColor,           &QLineEdit::textChanged, this, &DlgEditFilter::bgColorTextChanged);
	connect(mUI->btnBgColor,          &QPushButton::clicked,   this, &DlgEditFilter::chooseBgColor);
	connect(mUI->leDurationLimit,     &QLineEdit::textEdited,  this, &DlgEditFilter::durationLimitEdited);
	connect(
		mUI->twNodes->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &DlgEditFilter::nodeSelectionChanged
	);
	connect(mUI->twNodes->model(), &QAbstractItemModel::dataChanged, this, &DlgEditFilter::updateFilterStats);

	// Set the data into the UI:
	mUI->leDisplayName->setText(mFilter.displayName());
	mUI->chbIsFavorite->setChecked(mFilter.isFavorite());
	mUI->pteNotes->setPlainText(mFilter.notes());
	mUI->leBgColor->setText(mFilter.bgColor().name());
	if (mFilter.durationLimit().isPresent())
	{
		mUI->leDurationLimit->setText(Utils::formatTime(mFilter.durationLimit().value()));
	}

	// Display the node tree:
	rebuildFilterModel();
	mUI->twNodes->expandAll();

	// Update the UI state:
	nodeSelectionChanged();
	updateFilterStats();
}





DlgEditFilter::~DlgEditFilter()
{
	Settings::saveWindowPos("DlgEditTemplateItem", *this);
	delete mUI->twNodes->itemDelegate();
}





void DlgEditFilter::reject()
{
	// Despite being called "reject", we do save the data
	// Mainly because we don't have a way of undoing all the edits.
	save();
	Super::reject();
}





void DlgEditFilter::save()
{
	mFilter.setDisplayName(mUI->leDisplayName->text());
	mFilter.setIsFavorite(mUI->chbIsFavorite->isChecked());
	mFilter.setNotes(mUI->pteNotes->toPlainText());
	QColor c(mUI->leBgColor->text());
	if (c.isValid())
	{
		mFilter.setBgColor(c);
	}
	bool isOK;
	auto dur = mUI->leDurationLimit->text();
	if (dur.isEmpty())
	{
		mFilter.resetDurationLimit();
	}
	else
	{
		auto durationLimit = Utils::parseTime(dur, isOK);
		if (isOK)
		{
			mFilter.setDurationLimit(durationLimit);
		}
		else
		{
			mFilter.resetDurationLimit();
		}
	}
}





Filter::Node * DlgEditFilter::selectedNode() const
{
	auto item = mUI->twNodes->currentItem();
	if (item == nullptr)
	{
		return nullptr;
	}
	return reinterpret_cast<Filter::Node *>(item->data(0, roleFilterNodePtr).toULongLong());
}





void DlgEditFilter::rebuildFilterModel()
{
	mUI->twNodes->clear();
	auto root = createItemFromNode(*(mFilter.rootNode()));
	mUI->twNodes->addTopLevelItem(root);
	addNodeChildren(*(mFilter.rootNode()), *root);
}





void DlgEditFilter::addNodeChildren(Filter::Node & aParentNode, QTreeWidgetItem & aParentItem)
{
	if (!aParentNode.canHaveChildren())
	{
		return;
	}
	for (const auto & ch: aParentNode.children())
	{
		auto item = createItemFromNode(*ch);
		aParentItem.addChild(item);
		addNodeChildren(*ch, *item);
	}
}





QTreeWidgetItem * DlgEditFilter::createItemFromNode(const Filter::Node & aNode)
{
	auto res = new QTreeWidgetItem({aNode.getCaption()});
	res->setFlags(res->flags() | Qt::ItemIsEditable);
	res->setData(0, roleFilterNodePtr, reinterpret_cast<qulonglong>(&aNode));
	return res;
}





void DlgEditFilter::selectNodeItem(const Filter::Node & aNode)
{
	auto item = getNodeItem(aNode);
	if (item == nullptr)
	{
		return;
	}
	mUI->twNodes->setCurrentItem(item);
}





QTreeWidgetItem * DlgEditFilter::getNodeItem(const Filter::Node & aNode)
{
	auto parent = aNode.parent();
	if (parent == nullptr)
	{
		return mUI->twNodes->topLevelItem(0);
	}
	auto parentItem = getNodeItem(*parent);
	if (parentItem == nullptr)
	{
		return nullptr;
	}
	auto & children = parent->children();
	auto count = children.size();
	for (size_t i = 0; i < count; ++i)
	{
		if (children[i].get() == &aNode)
		{
			auto item = parentItem->child(static_cast<int>(i));
			assert(item != nullptr);
			assert(item->data(0, roleFilterNodePtr).toULongLong() == reinterpret_cast<qulonglong>(&aNode));
			return item;
		}
	}
	return nullptr;
}





void DlgEditFilter::saveAndClose()
{
	save();
	close();
}





void DlgEditFilter::addNodeSibling()
{
	auto curNode = selectedNode();
	if (curNode == nullptr)
	{
		return;
	}
	auto parent = curNode->parent();
	if (parent == nullptr)
	{
		// Need to update filter's root
		auto combinator = std::make_shared<Filter::Node>(Filter::Node::nkAnd);
		combinator->addChild(curNode->shared_from_this());
		mFilter.setRootNode(combinator);
		parent = combinator.get();
	}
	auto child = std::make_shared<Filter::Node>(
		Filter::Node::nspLength,
		(parent->kind() == Filter::Node::nkAnd) ? Filter::Node::ncGreaterThanOrEqual : Filter::Node::ncLowerThan,
		0
	);
	parent->addChild(child);

	mFilter.checkNodeConsistency();

	rebuildFilterModel();
	mUI->twNodes->expandAll();
	selectNodeItem(*child);
}





void DlgEditFilter::insertNodeCombinator()
{
	auto selNode = selectedNode();
	if (selNode == nullptr)
	{
		return;
	}
	auto curNode = selNode->shared_from_this();
	auto combinator = std::make_shared<Filter::Node>(Filter::Node::nkOr);
	auto parent = curNode->parent();
	if (parent == nullptr)
	{
		// Need to update item's root
		mFilter.setRootNode(combinator);
	}
	else
	{
		parent->replaceChild(curNode.get(), combinator);
	}
	combinator->addChild(curNode);
	combinator->addChild(std::make_shared<Filter::Node>(
		Filter::Node::nspLength,
		Filter::Node::ncLowerThan,
		0
	));

	mFilter.checkNodeConsistency();

	rebuildFilterModel();
	mUI->twNodes->expandAll();
	selectNodeItem(*combinator);
}





void DlgEditFilter::removeNode()
{
	auto curNode = selectedNode();
	if (curNode == nullptr)
	{
		return;
	}

	// Ask the user for verification:
	QString questionText;
	if (curNode->canHaveChildren())
	{
		questionText = tr("Really remove the selected filter node, including its subtree?");
	}
	else
	{
		questionText = tr("Really remove the selected filter node?");
	}
	if (QMessageBox::question(
		this,
		tr("SkauTan: Remove filter node?"),
		questionText,
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Remove the filter:
	auto parent = curNode->parent();
	if (parent == nullptr)
	{
		// Special case, removing the entire root, replace with a Noop filter instead:
		mFilter.setNoopFilter();
	}
	else
	{
		parent->removeChild(curNode);
		// Replace single-child combinators with their child directly:
		while (
			(parent != nullptr) &&  // Haven't reached the top
			(parent->children().size() == 1))  // Single-child
		{
			auto singleChild = parent->children()[0];
			auto grandParent = parent->parent();
			if (grandParent == nullptr)
			{
				// Special case, the entire root is single-child, replace it:
				mFilter.setRootNode(singleChild);
				break;
			}
			else
			{
				grandParent->replaceChild(parent, singleChild);
				parent = grandParent;
			}
		}
	}

	mFilter.checkNodeConsistency();

	rebuildFilterModel();
	mUI->twNodes->expandAll();
}





void DlgEditFilter::previewFilter()
{
	DlgSongs dlg(mComponents, std::make_unique<SongFilterModel>(mFilter), false, this);
	dlg.exec();
}





void DlgEditFilter::nodeSelectionChanged()
{
	auto curNode = selectedNode();
	mUI->btnAddSibling->setEnabled(curNode != nullptr);
	mUI->btnInsertCombinator->setEnabled(curNode != nullptr);
	mUI->btnRemoveFilter->setEnabled((curNode != nullptr) && mFilter.rootNode()->canHaveChildren());  // The top level filter cannot be removed
}





void DlgEditFilter::updateFilterStats()
{
	auto numMatching = mComponents.get<Database>()->numSongsMatchingFilter(mFilter);
	mUI->lblMatchingSongCount->setText(tr("Matching songs: %n", "", numMatching));
}





void DlgEditFilter::bgColorTextChanged(const QString & aNewText)
{
	QColor c(aNewText);
	if (!c.isValid())
	{
		mUI->leBgColor->setStyleSheet({});
		return;
	}
	mUI->leBgColor->setStyleSheet(QString("background-color: %1").arg(aNewText));
}





void DlgEditFilter::chooseBgColor()
{
	auto c = QColorDialog::getColor(
		QColor(mUI->leBgColor->text()),
		this,
		tr("SkauTan: Choose template item color")
	);
	if (c.isValid())
	{
		mUI->leBgColor->setText(c.name());
	}
}





void DlgEditFilter::durationLimitEdited(const QString & aNewText)
{
	if (aNewText.trimmed().isEmpty())
	{
		mUI->leDurationLimit->setStyleSheet({});
		return;
	}
	bool isOK;
	Utils::parseTime(aNewText, isOK);
	if (isOK)
	{
		mUI->leDurationLimit->setStyleSheet({});
	}
	else
	{
		mUI->leDurationLimit->setStyleSheet("background-color: #ff7f7f");
	}
}

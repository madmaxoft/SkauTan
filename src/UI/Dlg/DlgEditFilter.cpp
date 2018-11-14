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
static int indexFromSongProperty(const Filter::Node & a_Node)
{
	auto prop = a_Node.songProperty();
	for (size_t i = 0; i < ARRAYCOUNT(g_SongProperties); ++i)
	{
		if (g_SongProperties[i] == prop)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}





static int indexFromComparison(const Filter::Node & a_Node)
{
	auto cmp = a_Node.comparison();
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
	SongFilterModel(const Filter & a_Filter):
		m_Filter(a_Filter)
	{
	}


protected:

	/** The filter to be applied. */
	const Filter & m_Filter;


	virtual bool filterAcceptsRow(int a_SourceRow, const QModelIndex & a_SourceParent) const override
	{
		if (a_SourceParent.isValid())
		{
			assert(!"This filter should not be used for multi-level data");
			return false;
		}

		auto idx = sourceModel()->index(a_SourceRow, 0, a_SourceParent);
		auto song = sourceModel()->data(idx, SongModel::roleSongPtr).value<SongPtr>();
		if (song == nullptr)
		{
			qWarning() << "Underlying model returned nullptr song for row " << a_SourceRow;
			assert(!"Unexpected nullptr song");
			return false;
		}
		return m_Filter.rootNode()->isSatisfiedBy(*song);
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
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) const override
	{
		Q_UNUSED(a_Index);
		auto widget = a_Option.widget;
		auto style = (widget != nullptr) ? widget->style() : QApplication::style();
		return style->sizeFromContents(QStyle::CT_ComboBox, &a_Option, Super::sizeHint(a_Option, a_Index), widget);
	}



	// editing
	virtual QWidget * createEditor(
		QWidget * a_Parent,
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) const override
	{
		Q_UNUSED(a_Option);

		// HACK: TreeWidget stores its QTreeWidgetItem ptrs in the index's internalPtr:
		auto item = reinterpret_cast<const QTreeWidgetItem *>(a_Index.internalPointer());

		auto node = reinterpret_cast<Filter::Node *>(item->data(0, roleFilterNodePtr).toULongLong());

		if (node->canHaveChildren())
		{
			// This is a combinator, use a single combobox as the editor:
			auto res = new QComboBox(a_Parent);
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
			auto editor = new QFrame(a_Parent);
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



	virtual void setEditorData(QWidget * a_Editor, const QModelIndex & a_Index) const override
	{
		Q_UNUSED(a_Index);
		auto filter = reinterpret_cast<const Filter::Node *>(a_Editor->property("nodePtr").toULongLong());
		assert(filter != nullptr);
		if (filter->canHaveChildren())
		{
			auto cb = reinterpret_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			cb->setCurrentIndex((filter->kind() == Filter::Node::nkOr) ? 1 : 0);
		}
		else
		{
			auto cbProp       = a_Editor->property("cbProp").value<QComboBox *>();
			auto cbComparison = a_Editor->property("cbComparison").value<QComboBox *>();
			auto leValue      = a_Editor->property("leValue").value<QLineEdit *>();
			assert(cbProp != nullptr);
			assert(cbComparison != nullptr);
			assert(leValue != nullptr);
			cbProp->setCurrentIndex(indexFromSongProperty(*filter));
			cbComparison->setCurrentIndex(indexFromComparison(*filter));
			leValue->setText(filter->value().toString());
		}
	}



	virtual void setModelData(
		QWidget * a_Editor,
		QAbstractItemModel * a_Model,
		const QModelIndex & a_Index
	) const override
	{
		Q_UNUSED(a_Model);
		Q_UNUSED(a_Index);

		auto node = reinterpret_cast<Filter::Node *>(a_Editor->property("nodePtr").toULongLong());
		assert(node != nullptr);
		if (node->canHaveChildren())
		{
			auto cb = reinterpret_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			node->setKind((cb->currentIndex() == 0) ? Filter::Node::nkAnd : Filter::Node::nkOr);
		}
		else
		{
			auto cbProp       = a_Editor->property("cbProp").value<QComboBox *>();
			auto cbComparison = a_Editor->property("cbComparison").value<QComboBox *>();
			auto leValue      = a_Editor->property("leValue").value<QLineEdit *>();
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
		a_Model->setData(a_Index, node->getCaption());
	}
};





////////////////////////////////////////////////////////////////////////////////
// DlgEditTemplateItem:

DlgEditFilter::DlgEditFilter(
	ComponentCollection & a_Components,
	Filter & a_Filter,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_Components(a_Components),
	m_Filter(a_Filter),
	m_UI(new Ui::DlgEditFilter)
{
	m_Filter.checkNodeConsistency();
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgEditFilter", *this);
	m_UI->twNodes->setItemDelegate(new FilterNodeDelegate);

	// Connect the signals:
	connect(m_UI->btnClose,            &QPushButton::clicked,   this, &DlgEditFilter::saveAndClose);
	connect(m_UI->btnAddSibling,       &QPushButton::clicked,   this, &DlgEditFilter::addNodeSibling);
	connect(m_UI->btnInsertCombinator, &QPushButton::clicked,   this, &DlgEditFilter::insertNodeCombinator);
	connect(m_UI->btnRemoveFilter,     &QPushButton::clicked,   this, &DlgEditFilter::removeNode);
	connect(m_UI->btnPreview,          &QPushButton::clicked,   this, &DlgEditFilter::previewFilter);
	connect(m_UI->leBgColor,           &QLineEdit::textChanged, this, &DlgEditFilter::bgColorTextChanged);
	connect(m_UI->btnBgColor,          &QPushButton::clicked,   this, &DlgEditFilter::chooseBgColor);
	connect(m_UI->leDurationLimit,     &QLineEdit::textEdited,  this, &DlgEditFilter::durationLimitEdited);
	connect(
		m_UI->twNodes->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &DlgEditFilter::nodeSelectionChanged
	);
	connect(m_UI->twNodes->model(), &QAbstractItemModel::dataChanged, this, &DlgEditFilter::updateFilterStats);

	// Set the data into the UI:
	m_UI->leDisplayName->setText(m_Filter.displayName());
	m_UI->chbIsFavorite->setChecked(m_Filter.isFavorite());
	m_UI->pteNotes->setPlainText(m_Filter.notes());
	m_UI->leBgColor->setText(m_Filter.bgColor().name());
	if (m_Filter.durationLimit().isPresent())
	{
		m_UI->leDurationLimit->setText(Utils::formatTime(m_Filter.durationLimit().value()));
	}

	// Display the node tree:
	rebuildFilterModel();
	m_UI->twNodes->expandAll();

	// Update the UI state:
	nodeSelectionChanged();
	updateFilterStats();
}





DlgEditFilter::~DlgEditFilter()
{
	Settings::saveWindowPos("DlgEditTemplateItem", *this);
	delete m_UI->twNodes->itemDelegate();
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
	m_Filter.setDisplayName(m_UI->leDisplayName->text());
	m_Filter.setIsFavorite(m_UI->chbIsFavorite->isChecked());
	m_Filter.setNotes(m_UI->pteNotes->toPlainText());
	QColor c(m_UI->leBgColor->text());
	if (c.isValid())
	{
		m_Filter.setBgColor(c);
	}
	bool isOK;
	auto dur = m_UI->leDurationLimit->text();
	if (dur.isEmpty())
	{
		m_Filter.resetDurationLimit();
	}
	else
	{
		auto durationLimit = Utils::parseTime(dur, isOK);
		if (isOK)
		{
			m_Filter.setDurationLimit(durationLimit);
		}
		else
		{
			m_Filter.resetDurationLimit();
		}
	}
}





Filter::Node * DlgEditFilter::selectedNode() const
{
	auto item = m_UI->twNodes->currentItem();
	if (item == nullptr)
	{
		return nullptr;
	}
	return reinterpret_cast<Filter::Node *>(item->data(0, roleFilterNodePtr).toULongLong());
}





void DlgEditFilter::rebuildFilterModel()
{
	m_UI->twNodes->clear();
	auto root = createItemFromNode(*(m_Filter.rootNode()));
	m_UI->twNodes->addTopLevelItem(root);
	addNodeChildren(*(m_Filter.rootNode()), *root);
}





void DlgEditFilter::addNodeChildren(Filter::Node & a_ParentNode, QTreeWidgetItem & a_ParentItem)
{
	if (!a_ParentNode.canHaveChildren())
	{
		return;
	}
	for (const auto & ch: a_ParentNode.children())
	{
		auto item = createItemFromNode(*ch);
		a_ParentItem.addChild(item);
		addNodeChildren(*ch, *item);
	}
}





QTreeWidgetItem * DlgEditFilter::createItemFromNode(const Filter::Node & a_Node)
{
	auto res = new QTreeWidgetItem({a_Node.getCaption()});
	res->setFlags(res->flags() | Qt::ItemIsEditable);
	res->setData(0, roleFilterNodePtr, reinterpret_cast<qulonglong>(&a_Node));
	return res;
}





void DlgEditFilter::selectNodeItem(const Filter::Node & a_Node)
{
	auto item = getNodeItem(a_Node);
	if (item == nullptr)
	{
		return;
	}
	m_UI->twNodes->setCurrentItem(item);
}





QTreeWidgetItem * DlgEditFilter::getNodeItem(const Filter::Node & a_Node)
{
	auto parent = a_Node.parent();
	if (parent == nullptr)
	{
		return m_UI->twNodes->topLevelItem(0);
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
		if (children[i].get() == &a_Node)
		{
			auto item = parentItem->child(static_cast<int>(i));
			assert(item != nullptr);
			assert(item->data(0, roleFilterNodePtr).toULongLong() == reinterpret_cast<qulonglong>(&a_Node));
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
		auto combinator = std::make_shared<Filter::Node>(Filter::Node::nkOr);
		combinator->addChild(curNode->shared_from_this());
		m_Filter.setRootNode(combinator);
		parent = combinator.get();
	}
	auto child = std::make_shared<Filter::Node>(
		Filter::Node::nspLength,
		(parent->kind() == Filter::Node::nkAnd) ? Filter::Node::ncGreaterThanOrEqual : Filter::Node::ncLowerThan,
		0
	);
	parent->addChild(child);

	m_Filter.checkNodeConsistency();

	rebuildFilterModel();
	m_UI->twNodes->expandAll();
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
		m_Filter.setRootNode(combinator);
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

	m_Filter.checkNodeConsistency();

	rebuildFilterModel();
	m_UI->twNodes->expandAll();
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
		m_Filter.setNoopFilter();
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
				m_Filter.setRootNode(singleChild);
				break;
			}
			else
			{
				grandParent->replaceChild(parent, singleChild);
				parent = grandParent;
			}
		}
	}

	m_Filter.checkNodeConsistency();

	rebuildFilterModel();
	m_UI->twNodes->expandAll();
}





void DlgEditFilter::previewFilter()
{
	DlgSongs dlg(m_Components, std::make_unique<SongFilterModel>(m_Filter), false, this);
	dlg.exec();
}





void DlgEditFilter::nodeSelectionChanged()
{
	auto curNode = selectedNode();
	m_UI->btnAddSibling->setEnabled(curNode != nullptr);
	m_UI->btnInsertCombinator->setEnabled(curNode != nullptr);
	m_UI->btnRemoveFilter->setEnabled((curNode != nullptr) && m_Filter.rootNode()->canHaveChildren());  // The top level filter cannot be removed
}





void DlgEditFilter::updateFilterStats()
{
	auto numMatching = m_Components.get<Database>()->numSongsMatchingFilter(m_Filter);
	m_UI->lblMatchingSongCount->setText(tr("Matching songs: %n", "", numMatching));
}





void DlgEditFilter::bgColorTextChanged(const QString & a_NewText)
{
	QColor c(a_NewText);
	if (!c.isValid())
	{
		m_UI->leBgColor->setStyleSheet({});
		return;
	}
	m_UI->leBgColor->setStyleSheet(QString("background-color: %1").arg(a_NewText));
}





void DlgEditFilter::chooseBgColor()
{
	auto c = QColorDialog::getColor(
		QColor(m_UI->leBgColor->text()),
		this,
		tr("SkauTan: Choose template item color")
	);
	if (c.isValid())
	{
		m_UI->leBgColor->setText(c.name());
	}
}





void DlgEditFilter::durationLimitEdited(const QString & a_NewText)
{
	if (a_NewText.trimmed().isEmpty())
	{
		m_UI->leDurationLimit->setStyleSheet({});
		return;
	}
	bool isOK;
	Utils::parseTime(a_NewText, isOK);
	if (isOK)
	{
		m_UI->leDurationLimit->setStyleSheet({});
	}
	else
	{
		m_UI->leDurationLimit->setStyleSheet("background-color: #ff7f7f");
	}
}

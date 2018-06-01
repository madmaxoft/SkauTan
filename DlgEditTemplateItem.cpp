#include "DlgEditTemplateItem.h"
#include "ui_DlgEditTemplateItem.h"
#include <assert.h>
#include <QStyledItemDelegate>
#include <QDebug>
#include <QMessageBox>
#include <QComboBox>
#include <QColorDialog>
#include <QAbstractItemModel>
#include "DlgSongs.h"
#include "Database.h"
#include "Settings.h"
#include "Utils.h"





#define ARRAYCOUNT(X) (sizeof(X) / sizeof(*(X)))





static const auto roleFilterPtr = Qt::UserRole + 1;

static Template::Filter::SongProperty g_SongProperties[] =
{
	Template::Filter::fspAuthor,
	Template::Filter::fspTitle,
	Template::Filter::fspGenre,
	Template::Filter::fspLength,
	Template::Filter::fspMeasuresPerMinute,
	Template::Filter::fspLastPlayed,
	Template::Filter::fspManualAuthor,
	Template::Filter::fspManualTitle,
	Template::Filter::fspManualGenre,
	Template::Filter::fspManualMeasuresPerMinute,
	Template::Filter::fspFileNameAuthor,
	Template::Filter::fspFileNameTitle,
	Template::Filter::fspFileNameGenre,
	Template::Filter::fspFileNameMeasuresPerMinute,
	Template::Filter::fspId3Author,
	Template::Filter::fspId3Title,
	Template::Filter::fspId3Genre,
	Template::Filter::fspId3MeasuresPerMinute,
	Template::Filter::fspPrimaryAuthor,
	Template::Filter::fspPrimaryTitle,
	Template::Filter::fspPrimaryGenre,
	Template::Filter::fspPrimaryMeasuresPerMinute,
	Template::Filter::fspWarningCount,
	Template::Filter::fspLocalRating,
	Template::Filter::fspRatingRhythmClarity,
	Template::Filter::fspRatingGenreTypicality,
	Template::Filter::fspRatingPopularity,
	Template::Filter::fspNotes,
};

static Template::Filter::Comparison g_Comparisons[] =
{
	Template::Filter::fcEqual,
	Template::Filter::fcNotEqual,
	Template::Filter::fcContains,
	Template::Filter::fcNotContains,
	Template::Filter::fcGreaterThan,
	Template::Filter::fcGreaterThanOrEqual,
	Template::Filter::fcLowerThan,
	Template::Filter::fcLowerThanOrEqual,
};





/** Returns the index into g_SongProperties that has the same property as the specified filter.
Returns -1 if not found. */
static int indexFromSongProperty(const Template::Filter & a_Filter)
{
	auto prop = a_Filter.songProperty();
	for (size_t i = 0; i < ARRAYCOUNT(g_SongProperties); ++i)
	{
		if (g_SongProperties[i] == prop)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}





static int indexFromComparison(const Template::Filter & a_Filter)
{
	auto cmp = a_Filter.comparison();
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
class FilterModel:
	public QSortFilterProxyModel
{
public:
	FilterModel(Template::FilterPtr a_Filter):
		m_Filter(a_Filter)
	{
	}


protected:

	/** The filter to be applied. */
	Template::FilterPtr m_Filter;


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
			qWarning() << ": Underlying model returned nullptr song for row " << a_SourceRow;
			assert(!"Unexpected nullptr song");
			return false;
		}
		return m_Filter->isSatisfiedBy(*song);
	}
};





////////////////////////////////////////////////////////////////////////////////
// FilterDelegate:

/** UI helper that provides editor for the filter tree. */
class FilterDelegate:
	public QStyledItemDelegate
{
	using Super = QStyledItemDelegate;

public:
	FilterDelegate(){}



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
		auto filter = reinterpret_cast<Template::Filter *>(item->data(0, roleFilterPtr).toULongLong());

		if (filter->canHaveChildren())
		{
			// This is a combinator, use a single combobox as the editor:
			auto res = new QComboBox(a_Parent);
			res->setEditable(false);
			res->setProperty("filterPtr", reinterpret_cast<qulonglong>(filter));
			res->addItems({
				DlgEditTemplateItem::tr("And", "FilterTreeEditor"),
				DlgEditTemplateItem::tr("Or", "FilterTreeEditor")
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
				cbProp->addItem(Template::Filter::songPropertyCaption(sp));
			}
			cbProp->setMaxVisibleItems(static_cast<int>(sizeof(g_SongProperties) / sizeof(*g_SongProperties)));
			for (const auto cmp: g_Comparisons)
			{
				cbComparison->addItem(Template::Filter::comparisonCaption(cmp));
			}
			cbComparison->setMaxVisibleItems(static_cast<int>(sizeof(g_Comparisons) / sizeof(*g_Comparisons)));
			layout->addWidget(cbProp);
			layout->addWidget(cbComparison);
			layout->addWidget(leValue);
			editor->setProperty("cbProp",       QVariant::fromValue(cbProp));
			editor->setProperty("cbComparison", QVariant::fromValue(cbComparison));
			editor->setProperty("leValue",      QVariant::fromValue(leValue));
			editor->setProperty("filterPtr",    reinterpret_cast<qulonglong>(filter));
			return editor;
		}
	}



	virtual void setEditorData(QWidget * a_Editor, const QModelIndex & a_Index) const override
	{
		Q_UNUSED(a_Index);
		auto filter = reinterpret_cast<const Template::Filter *>(a_Editor->property("filterPtr").toULongLong());
		assert(filter != nullptr);
		if (filter->canHaveChildren())
		{
			auto cb = reinterpret_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			cb->setCurrentIndex((filter->kind() == Template::Filter::fkOr) ? 1 : 0);
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

		auto filter = reinterpret_cast<Template::Filter *>(a_Editor->property("filterPtr").toULongLong());
		assert(filter != nullptr);
		if (filter->canHaveChildren())
		{
			auto cb = reinterpret_cast<QComboBox *>(a_Editor);
			assert(cb != nullptr);
			filter->setKind((cb->currentIndex() == 0) ? Template::Filter::fkAnd : Template::Filter::fkOr);
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
				filter->setSongProperty(g_SongProperties[propIdx]);
			}
			auto cmpIdx = cbComparison->currentIndex();
			if (cmpIdx >= 0)
			{
				filter->setComparison(g_Comparisons[cmpIdx]);
			}
			filter->setValue(leValue->text());
		}
		a_Model->setData(a_Index, filter->getCaption());
	}
};





////////////////////////////////////////////////////////////////////////////////
// DlgEditTemplateItem:

DlgEditTemplateItem::DlgEditTemplateItem(
	Database & a_DB,
	MetadataScanner & a_Scanner,
	HashCalculator & a_Hasher,
	Template::Item & a_Item,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_DB(a_DB),
	m_MetadataScanner(a_Scanner),
	m_HashCalculator(a_Hasher),
	m_Item(a_Item),
	m_UI(new Ui::DlgEditTemplateItem)
{
	m_Item.checkFilterConsistency();
	m_UI->setupUi(this);
	Settings::loadWindowPos("DlgEditTemplateItem", *this);
	m_UI->twFilters->setItemDelegate(new FilterDelegate);

	// Connect the signals:
	connect(m_UI->btnClose,            &QPushButton::clicked,   this, &DlgEditTemplateItem::saveAndClose);
	connect(m_UI->btnAddSibling,       &QPushButton::clicked,   this, &DlgEditTemplateItem::addFilterSibling);
	connect(m_UI->btnInsertCombinator, &QPushButton::clicked,   this, &DlgEditTemplateItem::insertFilterCombinator);
	connect(m_UI->btnRemoveFilter,     &QPushButton::clicked,   this, &DlgEditTemplateItem::removeFilter);
	connect(m_UI->btnPreview,          &QPushButton::clicked,   this, &DlgEditTemplateItem::previewFilter);
	connect(m_UI->leBgColor,           &QLineEdit::textChanged, this, &DlgEditTemplateItem::bgColorTextChanged);
	connect(m_UI->btnBgColor,          &QPushButton::clicked,   this, &DlgEditTemplateItem::chooseBgColor);
	connect(m_UI->leDurationLimit,     &QLineEdit::textEdited,  this, &DlgEditTemplateItem::durationLimitEdited);
	connect(
		m_UI->twFilters->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &DlgEditTemplateItem::filterSelectionChanged
	);
	connect(m_UI->twFilters->model(), &QAbstractItemModel::dataChanged, this, &DlgEditTemplateItem::updateFilterStats);

	// Set the data into the UI:
	m_UI->leDisplayName->setText(m_Item.displayName());
	m_UI->chbIsFavorite->setChecked(m_Item.isFavorite());
	m_UI->pteNotes->setPlainText(m_Item.notes());
	m_UI->leBgColor->setText(m_Item.bgColor().name());
	if (m_Item.durationLimit().isPresent())
	{
		m_UI->leDurationLimit->setText(Utils::formatFractionalTime(m_Item.durationLimit().value()));
	}

	// Display the filters tree:
	m_UI->twFilters->setHeaderLabels({tr("Filter", "FilterTree")});
	rebuildFilterModel();
	m_UI->twFilters->expandAll();

	// Update the UI state:
	filterSelectionChanged();
	updateFilterStats();
}





DlgEditTemplateItem::~DlgEditTemplateItem()
{
	Settings::saveWindowPos("DlgEditTemplateItem", *this);
	delete m_UI->twFilters->itemDelegate();
}





void DlgEditTemplateItem::reject()
{
	// Despite being called "reject", we do save the data
	// Mainly because we don't have a way of undoing all the edits.
	save();
	Super::reject();
}





void DlgEditTemplateItem::save()
{
	m_Item.setDisplayName(m_UI->leDisplayName->text());
	m_Item.setIsFavorite(m_UI->chbIsFavorite->isChecked());
	m_Item.setNotes(m_UI->pteNotes->toPlainText());
	QColor c(m_UI->leBgColor->text());
	if (c.isValid())
	{
		m_Item.setBgColor(c);
	}
	bool isOK;
	auto durationLimit = Utils::parseTime(m_UI->leDurationLimit->text(), isOK);
	if (isOK)
	{
		m_Item.setDurationLimit(durationLimit);
	}
	else
	{
		m_Item.resetDurationLimit();
	}
}





Template::Filter * DlgEditTemplateItem::selectedFilter() const
{
	auto item = m_UI->twFilters->currentItem();
	if (item == nullptr)
	{
		return nullptr;
	}
	return reinterpret_cast<Template::Filter *>(item->data(0, roleFilterPtr).toULongLong());
}





void DlgEditTemplateItem::rebuildFilterModel()
{
	m_UI->twFilters->clear();
	auto root = createItemFromFilter(*(m_Item.filter()));
	m_UI->twFilters->addTopLevelItem(root);
	addFilterChildren(*(m_Item.filter()), *root);
}





void DlgEditTemplateItem::addFilterChildren(Template::Filter & a_ParentFilter, QTreeWidgetItem & a_ParentItem)
{
	if (!a_ParentFilter.canHaveChildren())
	{
		return;
	}
	for (const auto & ch: a_ParentFilter.children())
	{
		auto item = createItemFromFilter(*ch);
		a_ParentItem.addChild(item);
		addFilterChildren(*ch, *item);
	}
}





QTreeWidgetItem * DlgEditTemplateItem::createItemFromFilter(const Template::Filter & a_Filter)
{
	auto res = new QTreeWidgetItem({a_Filter.getCaption()});
	res->setFlags(res->flags() | Qt::ItemIsEditable);
	res->setData(0, roleFilterPtr, reinterpret_cast<qulonglong>(&a_Filter));
	return res;
}





void DlgEditTemplateItem::selectFilterItem(const Template::Filter & a_Filter)
{
	auto item = getFilterItem(a_Filter);
	if (item == nullptr)
	{
		return;
	}
	m_UI->twFilters->setCurrentItem(item);
}





QTreeWidgetItem * DlgEditTemplateItem::getFilterItem(const Template::Filter & a_Filter)
{
	auto parent = a_Filter.parent();
	if (parent == nullptr)
	{
		return m_UI->twFilters->topLevelItem(0);
	}
	auto parentItem = getFilterItem(*parent);
	if (parentItem == nullptr)
	{
		return nullptr;
	}
	auto & children = parent->children();
	auto count = children.size();
	for (size_t i = 0; i < count; ++i)
	{
		if (children[i].get() == &a_Filter)
		{
			auto item = parentItem->child(static_cast<int>(i));
			assert(item != nullptr);
			assert(item->data(0, roleFilterPtr).toULongLong() == reinterpret_cast<qulonglong>(&a_Filter));
			return item;
		}
	}
	return nullptr;
}





void DlgEditTemplateItem::saveAndClose()
{
	save();
	close();
}





void DlgEditTemplateItem::addFilterSibling()
{
	auto curFilter = selectedFilter();
	if (curFilter == nullptr)
	{
		return;
	}
	auto parent = curFilter->parent();
	if (parent == nullptr)
	{
		// Need to update item's root
		auto combinator = std::make_shared<Template::Filter>(Template::Filter::fkOr);
		combinator->addChild(curFilter->shared_from_this());
		m_Item.setFilter(combinator);
		parent = combinator.get();
	}
	auto child = std::make_shared<Template::Filter>(
		Template::Filter::fspLength,
		(parent->kind() == Template::Filter::fkAnd) ? Template::Filter::fcGreaterThanOrEqual : Template::Filter::fcLowerThan,
		0
	);
	parent->addChild(child);

	m_Item.checkFilterConsistency();

	rebuildFilterModel();
	m_UI->twFilters->expandAll();
	selectFilterItem(*child);
}





void DlgEditTemplateItem::insertFilterCombinator()
{
	auto selFilter = selectedFilter();
	if (selFilter == nullptr)
	{
		return;
	}
	auto curFilter = selFilter->shared_from_this();
	auto combinator = std::make_shared<Template::Filter>(Template::Filter::fkOr);
	auto parent = curFilter->parent();
	if (parent == nullptr)
	{
		// Need to update item's root
		m_Item.setFilter(combinator);
	}
	else
	{
		parent->replaceChild(curFilter.get(), combinator);
	}
	combinator->addChild(curFilter);
	combinator->addChild(std::make_shared<Template::Filter>(
		Template::Filter::fspLength,
		Template::Filter::fcLowerThan,
		0
	));

	m_Item.checkFilterConsistency();

	rebuildFilterModel();
	m_UI->twFilters->expandAll();
	selectFilterItem(*combinator);
}





void DlgEditTemplateItem::removeFilter()
{
	auto curFilter = selectedFilter();
	if (curFilter == nullptr)
	{
		return;
	}

	// Ask the user for verification:
	QString questionText;
	if (curFilter->canHaveChildren())
	{
		questionText = tr("Really remove the selected filter, including its subtree?");
	}
	else
	{
		questionText = tr("Really remove the selected filter?");
	}
	if (QMessageBox::question(
		this,
		tr("SkauTan - Remove filter?"),
		questionText,
		QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape
	) == QMessageBox::No)
	{
		return;
	}

	// Remove the filter:
	auto parent = curFilter->parent();
	if (parent == nullptr)
	{
		// Special case, removing the entire root, replace with a Noop filter instead:
		m_Item.setNoopFilter();
	}
	else
	{
		parent->removeChild(curFilter);
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
				m_Item.setFilter(singleChild);
				break;
			}
			else
			{
				grandParent->replaceChild(parent, singleChild);
				parent = grandParent;
			}
		}
	}

	m_Item.checkFilterConsistency();

	rebuildFilterModel();
	m_UI->twFilters->expandAll();
}





void DlgEditTemplateItem::previewFilter()
{
	DlgSongs dlg(m_DB, m_MetadataScanner, m_HashCalculator, std::make_unique<FilterModel>(m_Item.filter()), false, this);
	dlg.exec();
}





void DlgEditTemplateItem::filterSelectionChanged()
{
	auto curFilter = selectedFilter();
	m_UI->btnAddSibling->setEnabled(curFilter != nullptr);
	m_UI->btnInsertCombinator->setEnabled(curFilter != nullptr);
	m_UI->btnRemoveFilter->setEnabled((curFilter != nullptr) && m_Item.filter()->canHaveChildren());  // The top level filter cannot be removed
}





void DlgEditTemplateItem::updateFilterStats()
{
	auto numMatching = m_DB.numSongsMatchingFilter(*m_Item.filter());
	m_UI->lblMatchingSongCount->setText(tr("Matching songs: %n", "",numMatching));
}





void DlgEditTemplateItem::bgColorTextChanged(const QString & a_NewText)
{
	QColor c(a_NewText);
	if (!c.isValid())
	{
		m_UI->leBgColor->setStyleSheet({});
		return;
	}
	m_UI->leBgColor->setStyleSheet(QString("background-color: %1").arg(a_NewText));
}





void DlgEditTemplateItem::chooseBgColor()
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





void DlgEditTemplateItem::durationLimitEdited(const QString & a_NewText)
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

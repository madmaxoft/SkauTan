#include "DlgEditTemplateItem.h"
#include "ui_DlgEditTemplateItem.h"
#include <assert.h>
#include <QStyledItemDelegate>
#include <QDebug>
#include <QMessageBox>





static const auto roleFilterPtr = Qt::UserRole + 1;





////////////////////////////////////////////////////////////////////////////////
// FilterDelegate:

/** UI helper that provides editor for the filter tree. */
class FilterDelegate:
	public QStyledItemDelegate
{
	using Super = QStyledItemDelegate;

public:
	FilterDelegate(){}

	// TODO
};





////////////////////////////////////////////////////////////////////////////////
// DlgEditTemplateItem:

DlgEditTemplateItem::DlgEditTemplateItem(Template::Item & a_Item, QWidget * a_Parent):
	Super(a_Parent),
	m_Item(a_Item),
	m_UI(new Ui::DlgEditTemplateItem)
{
	m_Item.checkFilterConsistency();
	m_UI->setupUi(this);
	// m_UI->tvFilters->setItemDelegate(new FilterDelegate);

	// Connect the signals:
	connect(m_UI->btnClose,            &QPushButton::clicked, this, &DlgEditTemplateItem::saveAndClose);
	connect(m_UI->btnAddSibling,       &QPushButton::clicked, this, &DlgEditTemplateItem::addFilterSibling);
	connect(m_UI->btnInsertCombinator, &QPushButton::clicked, this, &DlgEditTemplateItem::insertFilterCombinator);
	connect(m_UI->btnRemoveFilter,     &QPushButton::clicked, this, &DlgEditTemplateItem::removeFilter);
	connect(
		m_UI->twFilters->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &DlgEditTemplateItem::filterSelectionChanged
	);

	// Set the data into the UI:
	m_UI->leDisplayName->setText(m_Item.displayName());
	m_UI->chbIsFavorite->setChecked(m_Item.isFavorite());
	m_UI->pteNotes->setPlainText(m_Item.notes());

	// Display the filters tree:
	m_UI->twFilters->setHeaderLabels({tr("Filter", "FilterTree")});
	rebuildFilterModel();
	m_UI->twFilters->expandAll();
}





DlgEditTemplateItem::~DlgEditTemplateItem()
{
	// Nothing explicit needed
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
	auto res = new QTreeWidgetItem({getFilterCaption(a_Filter)});
	res->setData(0, roleFilterPtr, reinterpret_cast<qulonglong>(&a_Filter));
	return res;
}





QString DlgEditTemplateItem::getFilterCaption(const Template::Filter & a_Filter)
{
	switch (a_Filter.kind())
	{
		case Template::Filter::fkAnd: return tr("And", "FilterModel-ItemCaption");
		case Template::Filter::fkOr:  return tr("Or",  "FilterModel-ItemCaption");
		case Template::Filter::fkComparison:
		{
			return QString("%1 %2 %3").arg(
				Template::Filter::songPropertyCaption(a_Filter.songProperty()),
				Template::Filter::comparisonCaption(a_Filter.comparison()),
				a_Filter.value().toString()
			);
		}
	}
	return QString("<invalid filter kind: %1>").arg(a_Filter.kind());
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
	auto curFilter = selectedFilter()->shared_from_this();
	if (curFilter == nullptr)
	{
		return;
	}
	auto combinator = std::make_shared<Template::Filter>(Template::Filter::fkOr);
	auto parent = curFilter->parent();
	if (parent == nullptr)
	{
		// Need to update item's root
		m_Item.setFilter(combinator);
		parent = combinator.get();
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





void DlgEditTemplateItem::filterSelectionChanged()
{
	auto curFilter = selectedFilter();
	m_UI->btnAddSibling->setEnabled(curFilter != nullptr);
	m_UI->btnRemoveFilter->setEnabled(curFilter != nullptr);
}

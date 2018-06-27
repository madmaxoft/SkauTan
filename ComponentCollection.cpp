#include "ComponentCollection.h"
#include "assert.h"





////////////////////////////////////////////////////////////////////////////////
// ComponentCollection:

void ComponentCollection::addComponent(ComponentCollection::EKind a_Kind, ComponentCollection::ComponentBasePtr a_Component)
{
	auto itr = m_Components.find(a_Kind);
	if (itr != m_Components.end())
	{
		assert(!"Component of this kind is already present in the collection");
		throw std::logic_error("Duplicate component in collection");
	}
	m_Components[a_Kind] = a_Component;
}





ComponentCollection::ComponentBasePtr ComponentCollection::get(ComponentCollection::EKind a_Kind)
{
	auto itr = m_Components.find(a_Kind);
	if (itr != m_Components.end())
	{
		return itr->second;
	}
	assert(!"Component not present in the collection");
	return nullptr;
}







#include "ComponentCollection.hpp"
#include <cassert>





////////////////////////////////////////////////////////////////////////////////
// ComponentCollection:

void ComponentCollection::addComponent(ComponentCollection::EKind aKind, ComponentCollection::ComponentBasePtr aComponent)
{
	auto itr = mComponents.find(aKind);
	if (itr != mComponents.end())
	{
		assert(!"Component of this kind is already present in the collection");
		throw LogicError("Duplicate component in collection: %1", aKind);
	}
	mComponents[aKind] = aComponent;
}





ComponentCollection::ComponentBasePtr ComponentCollection::get(ComponentCollection::EKind aKind)
{
	auto itr = mComponents.find(aKind);
	if (itr != mComponents.end())
	{
		return itr->second;
	}
	assert(!"Component not present in the collection");
	return nullptr;
}







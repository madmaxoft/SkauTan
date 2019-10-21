#ifndef COMPONENTCOLLECTION_H
#define COMPONENTCOLLECTION_H





#include <memory>
#include <map>
#include "Exception.hpp"





/** A collection of the base objects, so that they don't need to be pushed around in parameters.
The collection is built upon app startup and is expected not to change after its initial build.

Usage:
At app start:
ComponentCollection cc
cc.addComponent(...);
...

In regular operation:
auto & db = ComponentCollection::get<Database>();
*/
class ComponentCollection
{
public:

	/** Specifies the kind of the individual component. */
	enum EKind
	{
		ckDatabase,
		ckBackgroundTasks,
		ckMetadataScanner,
		ckLengthHashCalculator,
		ckPlayer,
		ckDJControllers,
		ckLocalVoteServer,
		ckInstallConfiguration,
		ckTempoDetector,
		ckBackgroundTempoDetector,
	};


protected:

	/** An internal base for all components, that keeps track of the component kind. */
	class ComponentBase
	{
		friend class ::ComponentCollection;
	public:
		ComponentBase(EKind aKind):
			mKind(aKind)
		{
		}

		virtual ~ComponentBase() {}


	protected:
		EKind mKind;
	};

	using ComponentBasePtr = std::shared_ptr<ComponentBase>;


public:

	/** A base class representing the common functionality in all components in the collection. */
	template <EKind tKind>
	class Component:
		public ComponentBase
	{
	public:
		Component():
			ComponentBase(tKind)
		{
		}

		static EKind kind() { return tKind; }
	};



	/** Adds the specified component into the collection.
	Asserts that a component of the same kind doesn't already exist. */
	template <typename ComponentClass>
	void addComponent(std::shared_ptr<ComponentClass> aComponent)
	{
		addComponent(ComponentClass::kind(), aComponent);
	}


	/** Creates a new component of the specified template type,
	adds it to the collection and returns a shared ptr to it.
	Asserts that a component of the same kind doesn't already exist. */
	template <typename ComponentClass, typename... Args>
	std::shared_ptr<ComponentClass> addNew(Args &&... aArgs)
	{
		auto res = std::make_shared<ComponentClass>(std::forward<Args>(aArgs)...);
		if (res == nullptr)
		{
			throw RuntimeError("Failed to create component %1", ComponentClass::kind());
		}
		addComponent(ComponentClass::kind(), res);
		return res;
	}


	/** Returns the component of the specified class.
	Usage: auto db = mComponents.get<Database>(); */
	template <typename ComponentClass>
	std::shared_ptr<ComponentClass> get()
	{
		return std::dynamic_pointer_cast<ComponentClass>(get(ComponentClass::kind()));
	}


protected:

	/** The collection of all components. */
	std::map<EKind, ComponentBasePtr> mComponents;


	/** Adds the specified component into the collection.
	Asserts that a component of the same kind doesn't already exist.
	Client code should use the templated version, this is its actual implementation. */
	void addComponent(EKind aKind, ComponentBasePtr aComponent);

	/** Returns the component of the specified kind, as a base pointer.
	Clients should use the templated get() instead (which calls this internally). */
	ComponentBasePtr get(EKind aKind);
};





#endif // COMPONENTCOLLECTION_H

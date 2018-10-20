#ifndef COMPONENTCOLLECTION_H
#define COMPONENTCOLLECTION_H





#include <memory>
#include <map>





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
	};


protected:

	/** An internal base for all components, that keeps track of the component kind. */
	class ComponentBase
	{
		friend class ::ComponentCollection;
	public:
		ComponentBase(EKind a_Kind):
			m_Kind(a_Kind)
		{
		}

		virtual ~ComponentBase() {}


	protected:
		EKind m_Kind;
	};

	using ComponentBasePtr = std::shared_ptr<ComponentBase>;


public:

	/** A base class representing the common functionality in all components in the collection. */
	template <EKind tKind>
	class Component:
		public ComponentBase
	{
	public:
		static constexpr EKind Kind = tKind;

		Component():
			ComponentBase(tKind)
		{
		}
	};



	/** Adds the specified component into the collection.
	Asserts that a component of the same kind doesn't already exist. */
	template <typename ComponentClass>
	void addComponent(std::shared_ptr<ComponentClass> a_Component)
	{
		addComponent(ComponentClass::Kind, a_Component);
	}


	/** Creates a new component of the specified template type,
	adds it to the collection and returns a shared ptr to it.
	Asserts that a component of the same kind doesn't already exist. */
	template <typename ComponentClass, typename... Args>
	std::shared_ptr<ComponentClass> addNew(Args &&... a_Args)
	{
		auto res = std::make_shared<ComponentClass>(std::forward<Args>(a_Args)...);
		if (res == nullptr)
		{
			throw std::runtime_error("Failed to create component");
		}
		addComponent(ComponentClass::Kind, res);
		return res;
	}


	/** Returns the component of the specified class.
	Usage: auto db = m_Components.get<Database>(); */
	template <typename ComponentClass>
	std::shared_ptr<ComponentClass> get()
	{
		return std::dynamic_pointer_cast<ComponentClass>(get(ComponentClass::Kind));
	}


protected:

	/** The collection of all components. */
	std::map<EKind, ComponentBasePtr> m_Components;


	/** Adds the specified component into the collection.
	Asserts that a component of the same kind doesn't already exist.
	Client code should use the templated version, this is its actual implementation. */
	void addComponent(EKind a_Kind, ComponentBasePtr a_Component);

	/** Returns the component of the specified kind, as a base pointer.
	Clients should use the templated get() instead (which calls this internally). */
	ComponentBasePtr get(EKind a_Kind);
};





#endif // COMPONENTCOLLECTION_H

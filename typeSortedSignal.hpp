#pragma once

#include "sortedSignal.hpp"
#include <typeindex>

namespace sig
{

template<typename, ModificationDuringEmit = WillNotBeModifiedDuringEmit>
class TypeSortedSignalConnecter;

template<typename, ModificationDuringEmit = WillNotBeModifiedDuringEmit>
class TypeSortedSignal;

namespace detail
{
	template<typename FunctionType, typename SignalType>
	class TypeSortedSignalConnecterBase;
	template<typename... Arguments, typename SignalType>
	class TypeSortedSignalConnecterBase<void (Arguments...), SignalType>
	{
	public:
		template<typename Type, typename T>
		MovableSignalDisconnecter connect(T && slot, ShouldSort do_sort = DoSort)
		{
			return signal.connect(std::type_index(typeid(Type)), std::forward<T>(slot), do_sort);
		}

		template<typename Type, typename T>
		void disconnect(const T & slot)
		{
			signal.disconnect(std::type_index(typeid(Type)), slot);
		}

		template<typename CallBefore, typename CallAfter>
		void add_dependency(ShouldSort do_sort = DoSort)
		{
			signal.add_dependency(std::type_index(typeid(CallBefore)), std::type_index(typeid(CallAfter)), do_sort);
		}
		template<typename CallBefore, typename CallAfter>
		void remove_dependency()
		{
			signal.remove_dependency(std::type_index(typeid(CallBefore)), std::type_index(typeid(CallAfter)));
		}

		// if you have called connect, addDependency or removeDependency with
		// DoNotSort then you must call sort() before emitting this signal
		void sort()
		{
			signal.sort();
		}

	protected:
		TypeSortedSignalConnecterBase() = default;
		TypeSortedSignalConnecterBase(TypeSortedSignalConnecterBase &&) = default;
		TypeSortedSignalConnecterBase & operator=(TypeSortedSignalConnecterBase &&) = default;

		SignalType signal;
	};

	template<typename FunctionType, ModificationDuringEmit modification>
	class TypeSortedSignalBase
		: public TypeSortedSignalConnecter<FunctionType, modification>
	{
	public:
		void clear()
		{
			this->signal.clear();
		}

		template<typename Type>
		void clear()
		{
			this->signal.clear(std::type_index(typeid(Type)));
		}
		template<typename Type>
		void clear_dependencies()
		{
			this->signal.clear_dependencies(std::type_index(typeid(Type)));
		}

		template<typename Type>
		void reserve(size_t count)
		{
			this->signal.reserve(std::type_index(typeid(Type)), count);
		}
	};
}

template<typename... Arguments>
class TypeSortedSignalConnecter<void (Arguments...), WillBeModifiedDuringEmit>
	: public detail::TypeSortedSignalConnecterBase<void (Arguments...), SortedSignal<std::type_index, void (Arguments...), WillBeModifiedDuringEmit>>
{
public:
	void perform_delayed_add_remove()
	{
		this->signal.perform_delayed_add_remove();
	}
};

template<typename... Arguments>
class TypeSortedSignalConnecter<void (Arguments...), WillNotBeModifiedDuringEmit>
	: public detail::TypeSortedSignalConnecterBase<void (Arguments...), SortedSignal<std::type_index, void (Arguments...), WillNotBeModifiedDuringEmit>>
{
};

template<typename... Arguments>
class TypeSortedSignal<void (Arguments...), WillNotBeModifiedDuringEmit>
	: public detail::TypeSortedSignalBase<void (Arguments...), WillNotBeModifiedDuringEmit>
{
public:
	void emit(const Arguments &... arguments) const
	{
		this->signal.emit(arguments...);
	}
};

template<typename... Arguments>
class TypeSortedSignal<void (Arguments...), WillBeModifiedDuringEmit>
	: public detail::TypeSortedSignalBase<void (Arguments...), WillBeModifiedDuringEmit>
{
public:
	void emit(const Arguments &... arguments)
	{
		this->signal.emit(arguments...);
	}
	void emit_no_delayed_add(const Arguments &... arguments)
	{
		this->signal.emit_no_delayed_add(arguments...);
	}
};
}

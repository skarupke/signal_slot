#pragma once

#include "sortedSignal.h"
#include <typeindex>

template<typename>
class TypeSortedSignal;

template<typename... Arguments>
class TypeSortedSignal<void (Arguments...)>
{
public:
	template<typename Type, typename T>
	SignalDisconnecter connect(T && slot, sig::ShouldSort do_sort = sig::DoSort)
	{
		return signal.connect(std::type_index(typeid(Type)), std::forward<T>(slot), do_sort);
	}

	template<typename Type, typename T>
	void disconnect(const T & slot)
	{
		signal.disconnect(std::type_index(typeid(Type)), slot);
	}

	void emit(const Arguments &... arguments) const
	{
		signal.emit(arguments...);
	}

	template<typename CallBefore, typename CallAfter>
	void addDependency(sig::ShouldSort do_sort = sig::DoSort)
	{
		signal.addDependency(std::type_index(typeid(CallBefore)), std::type_index(typeid(CallAfter)), do_sort);
	}
	template<typename CallBefore, typename CallAfter>
	void removeDependency(sig::ShouldSort do_sort = sig::DoSort)
	{
		signal.removeDependency(std::type_index(typeid(CallBefore)), std::type_index(typeid(CallAfter)), do_sort);
	}

	// if you have called connect, addDependency or removeDependency with
	// sig::DoNotSort then you must call sort() before emitting this signal
	void sort()
	{
		signal.sort();
	}
	
	void clear()
	{
		signal.clear();
	}

private:
	SortedSignal<std::type_index, void (Arguments...)> signal;
};


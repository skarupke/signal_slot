#pragma once

#include "signal.h"
#include <unordered_map>
#include <unordered_set>
#include <exception>

namespace sig
{
enum ShouldSort
{
	DoSort,
	DoNotSort
};

template<typename KeyType>
struct circular_dependency_error : std::exception
{
	circular_dependency_error(const KeyType & key)
	{
		addToChain(key);
	}

#ifdef _MSC_VER
	const char * what() const override
#else
	const char * what() const noexcept override
#endif
	{
		return "Circular dependency when using a SortedSignal. The exception has a member called 'chain' which shows the invalid chain of dependencies.";
	}
	
	void addToChain(const KeyType & key)
	{
		try
		{
			chain.push_back(key);
		}
		catch(...)
		{
		#ifndef _MSC_VER
			// I don't expect this to ever happen. if it does happen
			// then this makes sure that it's not my problem. the client
			// will receive a circular_dependency_error and if they really
			// caused an exception in KeyType's copy constructor then they
			// can call std::rethrow_if_nested
			std::throw_with_nested(circular_dependency_error(std::move(chain)));
		#endif
		}
	}
	
private:
	explicit circular_dependency_error(std::vector<KeyType> && chain)
		: chain(std::move(chain))
	{
	}
	
	std::vector<KeyType> chain;
	
};
}

template<typename, typename...>
class SortedSignal;

template<typename KeyType, typename... Arguments>
class SortedSignal<KeyType, void (Arguments...)>
{
public:
	template<typename T>
	SignalDisconnecter connect(const KeyType & key, T && slot, sig::ShouldSort do_sort = sig::DoSort)
	{
		create_key(key);
		if (do_sort == sig::DoSort) sort();
		return signals[key].signal.connect(std::forward<T>(slot));
	}

	template<typename T>
	void disconnect(const KeyType & key, const T & slot)
	{
		auto found = signals.find(key);
		if (found == signals.end()) return;
		found->second.signal.disconnect(slot);
	}

	void emit(const Arguments &... arguments) const
	{
		for (const auto & signal : sorted_signals)
		{
			signal->emit(arguments...);
		}
	}

	void addDependency(KeyType call_before, const KeyType & call_after, sig::ShouldSort do_sort = sig::DoSort)
	{
		create_key(call_after);
		signals[call_after].dependencies.insert(std::move(call_before));
		if (do_sort == sig::DoSort) sort();
	}
	void removeDependency(const KeyType & call_before, const KeyType & call_after, sig::ShouldSort do_sort = sig::DoSort)
	{
		create_key(call_after);
		signals[call_after].dependencies.erase(call_before);
		if (do_sort == sig::DoSort) sort();
	}

	// if you have called connect, addDependency or removeDependency with
	// sig::DoNotSort then you must call sort() before emitting this signal
	void sort()
	{
		sorted_signals.clear();
		sorted_signals.reserve(signals.size());

		// if an element is in this map then it has already been handled
		// if the value in the map is true, then it is currently being
		// evaluated. meaning if we find it again then we have a circular dependency
		std::unordered_map<KeyType, bool> handled_to_currently_active;
		handled_to_currently_active.reserve(signals.size());
		for(auto & signal : signals)
		{
			sort_handle_key(signal.first, handled_to_currently_active);
		}
	}
	
	void clear()
	{
		signals.clear();
		sorted_signals.clear();
	}

private:
	struct SignalWithDependencies
	{
		Signal<void (Arguments...)> signal;
		std::unordered_set<KeyType> dependencies;

#ifdef _MSC_VER // as of december 2012 microsoft doesn't follow the standard's
		        // rules about automatic creation of constructors. so I create
		        // them manually instead
		SignalWithDependencies()
		{
		}
		SignalWithDependencies(SignalWithDependencies && other)
			: signal(std::move(other.signal))
			, dependencies(std::move(other.dependencies))
		{
		}
		SignalWithDependencies & operator=(SignalWithDependencies && other)
		{
			signal = std::move(other.signal);
			dependencies = std::move(other.dependencies);
			return *this;
		}
#endif
	};
	std::unordered_map<KeyType, SignalWithDependencies> signals;
	std::vector<Signal<void (Arguments...)> *> sorted_signals;
	
	// this function does the actual sorting. it first puts all the dependenies
	// for this key into the sorted_signals, then it puts the key itself.
	// the map given to it is used to remember whether a key was already
	// handled and whether we are dealing with a circular dependency
	void sort_handle_key(const KeyType & key, std::unordered_map<KeyType, bool> & handled_to_currently_active)
	{
		// see if it's a key that we don't even recognize
		auto signal_group = signals.find(key);
		if (signal_group == signals.end()) return;
		// see if it was already handled
		auto found = handled_to_currently_active.find(key);
		if (found != handled_to_currently_active.end())
		{
			if (found->second)
			{
				sorted_signals.clear();
				throw sig::circular_dependency_error<KeyType>(key);
			}
			// this is the normal case. it just means we already put this key
			// in sorted_signals and it's not a circular dependency
			else return;
		}

		handled_to_currently_active[key] = true;
		for (auto & dependency : signal_group->second.dependencies)
		{
			// I don't actually care about exceptions here. a SortedSignal
			// with a circular dependency will just have an empty
			// sorted_signals list. all I care about here is that the
			// exception correctly contains the chain of dependencies that
			// caused this to happen. and exceptions allow me to do that in a
			// way that makes the code fast if everything is normal
			try
			{
				sort_handle_key(dependency, handled_to_currently_active);
			}
			catch(sig::circular_dependency_error<KeyType> & error)
			{
				error.addToChain(key);
				throw;
			}
		}
		handled_to_currently_active[key] = false;
		// all dependencies have been pushed. now it's my turn
		sorted_signals.push_back(&signal_group->second.signal);
	}
	
	void create_key(const KeyType & key)
	{
		if (signals.find(key) != signals.end()) return;
		signals[key];
		// need to clear the sorted_signals because the pointers into the
		// signals map have been invalidated
		sorted_signals.clear();
	}
};

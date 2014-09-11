#pragma once

#include "signal.hpp"
#include <unordered_map>
#include <unordered_set>
#include <exception>

namespace sig
{

template<typename KeyType, typename FunctionType, ModificationDuringEmit = WillNotBeModifiedDuringEmit>
class SortedSignalConnecter;

template<typename, typename, ModificationDuringEmit = WillNotBeModifiedDuringEmit>
class SortedSignal;

enum ShouldSort
{
	DoSort,
	DoNotSort
};

#ifndef SIG_NO_EXCEPTIONS
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
#endif

namespace detail
{
	template<typename KeyType, typename FunctionType, typename SignalType>
	class SortedSignalConnecterBase
	{
	public:
		// if you have called connect, add_dependency or remove_dependency with
		// DoNotSort then you must call sort() before emitting this signal
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

	protected:
		SortedSignalConnecterBase() = default;
		SortedSignalConnecterBase(SortedSignalConnecterBase &&) = default;
		SortedSignalConnecterBase & operator=(SortedSignalConnecterBase &&) = default;

		struct SignalWithDependencies
		{
			SignalType signal;
			std::unordered_set<KeyType> dependencies;
		};
		std::unordered_map<KeyType, SignalWithDependencies> signals;
		std::vector<SignalType *> sorted_signals;

	private:
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
#				ifdef SIG_NO_EXCEPTIONS
					// for no exceptions we will recurse infinitely if we have
					// a circular dependency
					if (!found->second) return;
#				else
					if (found->second)
					{
						sorted_signals.clear();
						throw circular_dependency_error<KeyType>(key);
					}
					// this is the normal case. it just means we already put this key
					// in sorted_signals and it's not a circular dependency
					else return;
#				endif
			}

			handled_to_currently_active[key] = true;
			for (auto & dependency : signal_group->second.dependencies)
			{
#				ifdef SIG_NO_EXCEPTIONS
					sort_handle_key(dependency, handled_to_currently_active);
#				else
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
					catch(circular_dependency_error<KeyType> & error)
					{
						error.addToChain(key);
						throw;
					}
#				endif
			}
			handled_to_currently_active[key] = false;
			// all dependencies have been pushed. now it's my turn
			sorted_signals.push_back(&signal_group->second.signal);
		}
	};
	template<typename KeyType, typename FunctionType, ModificationDuringEmit modification>
	class SortedSignalBase
		: public SortedSignalConnecter<KeyType, FunctionType, modification>
	{
	public:
		void clear()
		{
			this->signals.clear();
			this->sorted_signals.clear();
		}

		void clear(const KeyType & key)
		{
			this->signals[key].signal.clear();
		}
		void clear_dependencies(const KeyType & key)
		{
			this->signals[key].dependencies.clear();
		}

		void reserve(const KeyType & key, size_t count)
		{
			this->signals[key].reserve(count);
		}
	};
}

template<typename KeyType, typename... Arguments>
class SortedSignalConnecter<KeyType, void (Arguments...), WillBeModifiedDuringEmit>
	: public detail::SortedSignalConnecterBase<KeyType, void (Arguments...), Signal<void (Arguments...), WillBeModifiedDuringEmit>>
{
	typedef Signal<void (Arguments...), WillBeModifiedDuringEmit> SignalType;
public:
	template<typename T>
	MovableSignalDisconnecter connect(const KeyType & key, T && slot, ShouldSort do_sort = DoSort)
	{
		sort_after_delayed_add |= do_sort == DoSort;
		auto found = this->signals.find(key);
		if (found == this->signals.end())
		{
			may_need_sort = true;
			return signals_to_add[key].connect(std::forward<T>(slot));
		}
		else
		{
			return found->second.signal.connect(std::forward<T>(slot));
		}
	}

	template<typename T>
	void disconnect(const KeyType & key, const T & slot)
	{
		auto found = this->signals.find(key);
		if (found == this->signals.end())
		{
			auto delayed_found = signals_to_add.find(key);
			if (delayed_found != signals_to_add.end())
			{
				delayed_found->second.disconnect(slot);
			}
		}
		else
		{
			found->second.signal.disconnect(slot);
		}
	}

	void add_dependency(const KeyType & call_before, const KeyType & call_after, ShouldSort do_sort = DoSort)
	{
		auto found = this->signals.find(call_after);
		if (found == this->signals.end())
		{
			may_need_sort |= dependencies_to_add.insert(std::make_pair(call_before, call_after)).second;
		}
		else
		{
			may_need_sort |= found->second.dependencies.insert(call_before).second;
		}
		sort_after_delayed_add |= do_sort == DoSort;
	}
	void remove_dependency(const KeyType & call_before, const KeyType & call_after)
	{
		auto found = this->signals.find(call_after);
		if (found == this->signals.end())
		{
			dependencies_to_add.erase(call_before);
		}
		else
		{
			found->second.dependencies.erase(call_before);
		}
	}

	void perform_delayed_add_remove()
	{
		for (auto & add : signals_to_add)
		{
			this->signals[std::move(add.first)].signal = std::move(add.second);
		}
		signals_to_add.clear();
		for (auto & it : this->signals)
		{
			it.second.signal.perform_delayed_add_remove();
		}
		for (auto & add : dependencies_to_add)
		{
			this->signals[std::move(add.second)].dependencies.insert(std::move(add.first));
		}
		dependencies_to_add.clear();
		if (may_need_sort && sort_after_delayed_add) this->sort();
		may_need_sort = sort_after_delayed_add = false;
	}

private:
	std::unordered_map<KeyType, SignalType> signals_to_add;
	std::unordered_map<KeyType, KeyType> dependencies_to_add;
	bool sort_after_delayed_add = false;
	bool may_need_sort = false;
};

template<typename KeyType, typename... Arguments>
class SortedSignalConnecter<KeyType, void (Arguments...), WillNotBeModifiedDuringEmit>
	: public detail::SortedSignalConnecterBase<KeyType, void (Arguments...), Signal<void (Arguments...), WillNotBeModifiedDuringEmit>>
{
	typedef Signal<void (Arguments...), WillNotBeModifiedDuringEmit> SignalType;
public:
	template<typename T>
	MovableSignalDisconnecter connect(const KeyType & key, T && slot, ShouldSort do_sort = DoSort)
	{
		auto found = this->signals.find(key);
		if (found == this->signals.end())
		{
			auto to_return = this->signals[key].signal.connect(std::forward<T>(slot));
			if (do_sort == DoSort) this->sort();
			else this->sorted_signals.clear();
			return to_return;
		}
		else
		{
			return found->second.signal.connect(std::forward<T>(slot));
		}
	}

	template<typename T>
	void disconnect(const KeyType & key, const T & slot)
	{
		auto found = this->signals.find(key);
		if (found != this->signals.end())
		{
			found->second.signal.disconnect(slot);
		}
	}

	void add_dependency(const KeyType & call_before, const KeyType & call_after, ShouldSort do_sort = DoSort)
	{
		auto found = this->signals.find(call_after);
		if (found == this->signals.end())
		{
			this->signals[call_after].dependencies.insert(call_before);
			if (do_sort == DoSort) this->sort();
			else this->sorted_signals.clear();
		}
		else if (found->second.dependencies.insert(call_before).second
				&& do_sort == DoSort)
		{
			this->sort();
		}
	}
	void remove_dependency(const KeyType & call_before, const KeyType & call_after)
	{
		auto found = this->signals.find(call_after);
		if (found != this->signals.end())
		{
			found->second.dependencies.erase(call_before);
		}
	}
};

template<typename KeyType, typename... Arguments>
class SortedSignal<KeyType, void (Arguments...), WillBeModifiedDuringEmit>
	: public detail::SortedSignalBase<KeyType, void (Arguments...), WillBeModifiedDuringEmit>
{
public:
	void emit(const Arguments &... arguments)
	{
		this->perform_delayed_add_remove();
		for (auto & signal : this->sorted_signals)
		{
			signal->emit(arguments...);
		}
	}

	void emit_no_delayed_add(const Arguments &... arguments)
	{
		for (auto & signal : this->sorted_signals)
		{
			signal->emit_no_delayed_add(arguments...);
		}
	}
};

template<typename KeyType, typename... Arguments>
class SortedSignal<KeyType, void (Arguments...), WillNotBeModifiedDuringEmit>
	: public detail::SortedSignalBase<KeyType, void (Arguments...), WillNotBeModifiedDuringEmit>
{
public:
	void emit(const Arguments &... arguments) const
	{
		for (auto & signal : this->sorted_signals)
		{
			signal->emit(arguments...);
		}
	}
};
}

#pragma once

#include "function.hpp"
#include "small_shared_ptr.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <cstddef>

namespace sig
{
enum ModificationDuringEmit
{
	// if you instantiate the signal with WillBeModifiedDuringEmit, then all
	// the functions that are part of the SignalConnecter interface may be
	// called in a callback from emit(). for example a slot may disconnect
	// itself in a callback and it may add other slots to the signal. functions
	// that are only part of the Signal interface may not be called during
	// emit. obviously don't call clear(), but also don't call emit() again
	WillBeModifiedDuringEmit,
	// if you instantiate the signal with WillNotBeModifiedDuringEmit, you may
	// not modify the signal in any way in a callback from emit. the benefit is
	// that this allows you to call emit() multiple times simultaneously, even
	// from different threads. the signal also becomes a bit faster and smaller
	// because it has to do less checks and has less internal state
	WillNotBeModifiedDuringEmit
};

template<typename, ModificationDuringEmit = WillNotBeModifiedDuringEmit>
class Signal;

template<typename, ModificationDuringEmit = WillNotBeModifiedDuringEmit>
class SignalConnecter;

namespace detail
{
	template<typename T>
	void erase_unordered(T & container, size_t index)
	{
		std::iter_swap(container.begin() + index, container.end() - 1);
		container.pop_back();
	}

	template<typename... Arguments, typename T>
	bool is_target(const func::function<void (Arguments...)> & function, const T & to_check)
	{
		const T * target = function.template target<T>();
		return target && *target == to_check;
	}

	// this function searches the given container from both sides and returns
	// the index where the functor returns true. I use this because I couldn't
	// decide whether signals are more likely to be used in a FIFO or a FILO
	// manner
	template<typename Container, typename T>
	size_t find_index_two_way(const Container & to_search, const T & object)
	{
		size_t low = 0;
		size_t high = to_search.size();
		if (high % 2)
		{
			if (object == to_search[low]) return low;
			++low;
		}
		for (; low < high; ++low, --high)
		{
			if (object == to_search[low]) return low;
			if (object == to_search[high - 1]) return high - 1;
		}
		return static_cast<size_t>(-1);
	}
	// this function searches the given container from both sides and returns
	// the index where the functor returns true. I use this because I couldn't
	// decide whether signals are more likely to be used in a FIFO or a FILO
	// manner
	template<typename Container, typename Functor>
	size_t find_index_if_two_way(const Container & to_search, Functor && functor)
	{
		size_t low = 0;
		size_t high = to_search.size();
		if (high % 2)
		{
			if (functor(to_search[low])) return low;
			++low;
		}
		for (; low < high; ++low, --high)
		{
			if (functor(to_search[low])) return low;
			if (functor(to_search[high - 1])) return high - 1;
		}
		return static_cast<size_t>(-1);
	}

	template<typename FunctionType, ModificationDuringEmit modification>
	class SignalBase
		: public SignalConnecter<FunctionType, modification>
	{
		typedef Signal<FunctionType, modification> SignalType;
		typedef SignalConnecter<FunctionType, modification> ConnecterType;

	public:
		void clear()
		{
			ConnecterType::clear();
			slots.clear();
		}

		void reserve(size_t count)
		{
			ConnecterType::reserve(count);
			slots.reserve(count);
		}

	protected:
		SignalBase(SignalType * parent)
			: ConnecterType(parent)
		{
		}
		SignalBase(SignalBase && other, SignalType * parent)
			: ConnecterType(std::move(other), parent)
			, slots(std::move(other.slots))
		{
		}
		SignalBase & operator=(SignalBase &&) = default;

		friend ConnecterType;

		typedef std::vector<func::function<FunctionType>> SlotsListType;
		SlotsListType slots;
	};
}

class MovableSignalDisconnecter;

class SignalDisconnecter
{
public:
	SignalDisconnecter();
	SignalDisconnecter(const SignalDisconnecter &);
	SignalDisconnecter(SignalDisconnecter &&);
	SignalDisconnecter & operator=(const SignalDisconnecter &) = delete;
	SignalDisconnecter & operator=(SignalDisconnecter &&) = delete;
	SignalDisconnecter(MovableSignalDisconnecter && builder);
	SignalDisconnecter & operator=(MovableSignalDisconnecter && builder);
	~SignalDisconnecter();

	void disconnect();

	/**
	 * Will release this disconnecter, so that it will not call disconnect() on
	 * destruction. It returns a func::function that you can use to disconnect
	 * at a different time. Usually you will just want to ignore the return
	 * value
	 */
	func::function<void ()> release_disconnecter();

private:
	template<typename, ModificationDuringEmit>
	friend class SignalConnecter;
	friend class MovableSignalDisconnecter;

	typedef size_t ID;
	static ID get_next_id();
	static void free_id(ID);

	func::function<void ()> disconnecter;
};

class MovableSignalDisconnecter
{
public:
	explicit MovableSignalDisconnecter(func::function<void ()> &&);
	MovableSignalDisconnecter(MovableSignalDisconnecter &&);
	MovableSignalDisconnecter & operator=(MovableSignalDisconnecter &&);
	MovableSignalDisconnecter(SignalDisconnecter &&);
	MovableSignalDisconnecter & operator=(SignalDisconnecter &&);
	~MovableSignalDisconnecter();

	void disconnect();

	/**
	 * Will release this disconnecter, so that it will not call disconnect() on
	 * destruction. It returns a func::function that you can use to disconnect
	 * at a different time. Usually you will just want to ignore the return
	 * value
	 */
	func::function<void ()> release_disconnecter();

private:
	friend class SignalDisconnecter;
	func::function<void ()> disconnecter;
};

template<typename... Arguments>
class SignalConnecter<void (Arguments...), WillBeModifiedDuringEmit>
{
	typedef Signal<void (Arguments...), WillBeModifiedDuringEmit> SignalType;
public:
	/**
	 * Connect the given function or functor to this signal. It will be called
	 * every time that someone emits this signal.
	 * This function returns a SignalDisconnecter that will disconnect the slot
	 * when it is destroyed. Therefore if you ignore the return value this
	 * function will have no effect because the Disconnecter will disconnect
	 * immediately
	 */
	template<typename T>
	MovableSignalDisconnecter connect(T && slot)
	{
		ptr::small_shared_ptr<SignalType *> check_alive = parent;
		SignalDisconnecter::ID id = SignalDisconnecter::get_next_id();
		MovableSignalDisconnecter to_return([check_alive, id]
		{
			if (!*check_alive) return;
			SignalType & signal = **check_alive;

			// if this signal is currently executing, we need to reset the
			// signal that this disconnecter belongs to. because otherwise it
			// would still be executed from the current emit. the exception is
			// if the element being removed is the current element
			if (signal.current_slot != static_cast<size_t>(-1) && signal.disconnecters[signal.current_slot] != id)
			{
				size_t found = detail::find_index_two_way(signal.disconnecters, id);
				if (found != static_cast<size_t>(-1)) signal.slots[found] = nullptr;
			}
			signal.to_remove.push_back(id);
		});

		to_add.emplace_back(std::forward<T>(slot), id);

		return std::move(to_return);
	}

	template<typename T>
	void disconnect(const T & slot)
	{
		SignalType & signal = static_cast<SignalType &>(*this);
		auto is_target = [&](func::function<void (Arguments...)> & f){ return detail::is_target(f, slot); };
		size_t found = detail::find_index_if_two_way(signal.slots, is_target);
		if (found != static_cast<size_t>(-1))
		{
			signal.slots[found] = nullptr;
			to_remove.push_back(disconnecters[found]);
		}
		else
		{
			found = detail::find_index_if_two_way(to_add, is_target);
			if (found != static_cast<size_t>(-1))
			{
				to_remove.push_back(to_add[found].second);
			}
		}
	}

	void perform_delayed_add_remove()
	{
		for (auto & remove : to_remove)
		{
			size_t found = detail::find_index_two_way(disconnecters, remove);
			if (found != static_cast<size_t>(-1))
			{
				detail::erase_unordered(static_cast<SignalType &>(*this).slots, found);
				detail::erase_unordered(disconnecters, found);
				SignalDisconnecter::free_id(remove);
			}
			else
			{
				found = detail::find_index_if_two_way(to_add, [&](const std::pair<func::function<void (Arguments...)>, SignalDisconnecter::ID> & add)
				{
					return add.second == remove;
				});
				if (found != static_cast<size_t>(-1))
				{
					detail::erase_unordered(to_add, found);
					SignalDisconnecter::free_id(remove);
				}
			}
		}
		to_remove.clear();
		for (auto & add : to_add)
		{
			static_cast<SignalType &>(*this).slots.push_back(std::move(add.first));
			disconnecters.push_back(std::move(add.second));
		}
		to_add.clear();
	}

protected:
	SignalConnecter(SignalType * parent)
		: parent(ptr::make_shared<SignalType *>(parent))
	{
	}
	SignalConnecter(SignalConnecter &&) = delete;
	SignalConnecter(SignalConnecter && other, SignalType * parent)
		: disconnecters(std::move(other.disconnecters))
		, to_add(std::move(other.to_add))
		, to_remove(std::move(other.to_remove))
		, parent(std::move(other.parent))
	{
		*this->parent = parent;
	}
	SignalConnecter & operator=(SignalConnecter && other)
	{
		disconnecters = std::move(other.disconnecters);
		to_add = std::move(other.to_add);
		to_remove = std::move(other.to_remove);
		using std::swap;
		// I want the ownership to change but I don't want the pointer to
		// change, so I swap twice
		swap(parent, other.parent);
		swap(*parent, *other.parent);
		return *this;
	}
	~SignalConnecter()
	{
		if (parent) *parent = nullptr;
	}

	void clear()
	{
		to_remove.clear();
		to_add.clear();
		disconnecters.clear();
	}
	void reserve(size_t count)
	{
		disconnecters.reserve(count);
	}

private:
	std::vector<SignalDisconnecter::ID> disconnecters;
	// containers for storing objects that should be added or removed on the
	// next emit. this is needed because slots have a tendency to remove
	// themselves in their callback.
	std::vector<std::pair<func::function<void (Arguments...)>, SignalDisconnecter::ID>> to_add;
	std::vector<SignalDisconnecter::ID> to_remove;

	// this is used by the disconnecter to check whether I still live
	ptr::small_shared_ptr<SignalType *> parent;
};


template<typename... Arguments>
class SignalConnecter<void (Arguments...), WillNotBeModifiedDuringEmit>
{
	typedef Signal<void (Arguments...), WillNotBeModifiedDuringEmit> SignalType;
public:
	/**
	 * Connect the given function or functor to this signal. It will be called
	 * every time that someone emits this signal.
	 * This function returns a SignalDisconnecter that will disconnect the slot
	 * when it is destroyed. Therefore if you ignore the return value this
	 * function will have no effect because the Disconnecter will disconnect
	 * immediately
	 */
	template<typename T>
	MovableSignalDisconnecter connect(T && slot)
	{
		ptr::small_shared_ptr<SignalType *> check_alive = parent;
		SignalDisconnecter::ID id = SignalDisconnecter::get_next_id();
		MovableSignalDisconnecter to_return([check_alive, id]
		{
			if (!*check_alive) return;
			SignalType & signal = **check_alive;

			size_t found = detail::find_index_two_way(signal.disconnecters, id);
			if (found != static_cast<size_t>(-1))
			{
				detail::erase_unordered(signal.slots, found);
				detail::erase_unordered(signal.disconnecters, found);
				SignalDisconnecter::free_id(id);
			}
		});

		static_cast<SignalType &>(*this).slots.emplace_back(std::forward<T>(slot));
		disconnecters.emplace_back(id);

		return std::move(to_return);
	}

	template<typename T>
	void disconnect(const T & slot)
	{
		SignalType & signal = static_cast<SignalType &>(*this);
		size_t found = detail::find_index_if_two_way(signal.slots, [&](const func::function<void (Arguments...)> & f){ return detail::is_target(f, slot); });
		if (found != static_cast<size_t>(-1))
		{
			detail::erase_unordered(signal.slots, found);
			SignalDisconnecter::free_id(disconnecters[found]);
			detail::erase_unordered(disconnecters, found);
		}
	}

protected:
	SignalConnecter(SignalType * parent)
		: parent(ptr::make_shared<SignalType *>(parent))
	{
	}
	SignalConnecter(SignalConnecter &&) = delete;
	SignalConnecter(SignalConnecter && other, SignalType * parent)
		: disconnecters(std::move(other.disconnecters))
		, parent(std::move(other.parent))
	{
		*this->parent = parent;
	}
	SignalConnecter & operator=(SignalConnecter && other)
	{
		disconnecters = std::move(other.disconnecters);
		using std::swap;
		// I want the ownership to change but I don't want the pointer to
		// change, so I swap twice
		swap(parent, other.parent);
		swap(*parent, *other.parent);
		return *this;
	}
	~SignalConnecter()
	{
		if (parent) *parent = nullptr;
	}

	void clear()
	{
		disconnecters.clear();
	}
	void reserve(size_t count)
	{
		disconnecters.reserve(count);
	}

private:
	std::vector<SignalDisconnecter::ID> disconnecters;
	// this is used by the disconnecter to check whether I still live
	ptr::small_shared_ptr<SignalType *> parent;
};


template<typename... Arguments>
class Signal<void (Arguments...), WillBeModifiedDuringEmit>
	: public detail::SignalBase<void (Arguments...), WillBeModifiedDuringEmit>
{
	typedef SignalConnecter<void (Arguments...), WillBeModifiedDuringEmit> ConnecterType;
	typedef detail::SignalBase<void (Arguments...), WillBeModifiedDuringEmit> BaseType;
public:
	Signal()
		: BaseType(this)
	{
	}
	Signal(Signal && other)
		: BaseType(std::move(other), this)
	{
	}
	Signal & operator=(Signal && other)
	{
		BaseType::operator=(std::move(other));
		return *this;
	}

	void emit(const Arguments &... arguments)
	{
		this->perform_delayed_add_remove();
		emit_no_delayed_add(arguments...);
	}

	void emit_no_delayed_add(const Arguments &... arguments)
	{
		for (current_slot = 0; current_slot < this->slots.size(); ++current_slot)
		{
			if (this->slots[current_slot]) this->slots[current_slot](arguments...);
		}
		current_slot = static_cast<size_t>(-1);
	}

private:
	friend ConnecterType;
	size_t current_slot = static_cast<size_t>(-1);
};

template<typename... Arguments>
class Signal<void (Arguments...), WillNotBeModifiedDuringEmit>
	: public detail::SignalBase<void (Arguments...), WillNotBeModifiedDuringEmit>
{
	typedef detail::SignalBase<void (Arguments...), WillNotBeModifiedDuringEmit> BaseType;
public:
	Signal()
		: BaseType(this)
	{
	}
	Signal(Signal && other)
		: BaseType(std::move(other), this)
	{
	}
	Signal & operator=(Signal &&) = default;

	void emit(const Arguments &... arguments) const
	{
		for (auto & slot : this->slots)
		{
			slot(arguments...);
		}
	}

	// all iterators are const_iterators. still define all the names so that
	// I can change them in the future
	typedef typename BaseType::SlotsListType::const_iterator iterator;
	typedef typename BaseType::SlotsListType::const_iterator const_iterator;
	typedef typename BaseType::SlotsListType::const_reverse_iterator reverse_iterator;
	typedef typename BaseType::SlotsListType::const_reverse_iterator const_reverse_iterator;

	iterator begin()
	{
		return this->slots.begin();
	}
	const_iterator begin() const
	{
		return this->slots.begin();
	}
	reverse_iterator rbegin()
	{
		return this->slots.rbegin();
	}
	const_reverse_iterator rbegin() const
	{
		return this->slots.rbegin();
	}
	iterator end()
	{
		return this->slots.end();
	}
	const_iterator end() const
	{
		return this->slots.end();
	}
	reverse_iterator rend()
	{
		return this->slots.rend();
	}
	const_reverse_iterator rend() const
	{
		return this->slots.rend();
	}
};
}

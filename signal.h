#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <algorithm>

template<typename>
class Signal;

class SignalDisconnecter
{
public:
	SignalDisconnecter();
	SignalDisconnecter(SignalDisconnecter &&);
	SignalDisconnecter & operator=(SignalDisconnecter &&);
	~SignalDisconnecter();
	void disconnect();
	/**
	 * Will release this disconnecter, so that it will not call disconnect() on
	 * destruction. It returns a std::function that you can use to disconnect
	 * at a different time. Usually you will just want to ignore the return
	 * value
	 */
	std::function<void ()> releaseDisconnecter();

private:
#ifdef _MSC_VER
	SignalDisconnecter(const SignalDisconnecter &); // intentionally not implemented
	SignalDisconnecter & operator=(const SignalDisconnecter &); // intentionally not implemented
#endif
	template<typename>
	friend class Signal;
	typedef const void * ID;

	// I use the address of the functor as the ID.
	// this struct does nothing except call the functor with it's
	// own address so that the functor knows it's id
	template<typename T>
	struct ThisArgCaller
	{
		ThisArgCaller(T && to_call)
			: to_call(std::move(to_call))
		{
		}
		void operator()()
		{
			to_call(this);
		}
		void operator()() const
		{
			to_call(this);
		}
	private:
		T to_call;
		// I want to force a heap allocation inside the std::function, so that the
		// address of this object stays the same throughout it's lifetime. one way of
		// doing that is to make it so that this guy is guaranteed to be as big as a std::function
		static const ptrdiff_t bytes_to_force_heap_allocation = sizeof(std::function<void ()>) - sizeof(T);
		unsigned char force_heap_allocation[bytes_to_force_heap_allocation > 0 ? bytes_to_force_heap_allocation : 0];
	};
	template<typename T>
	static std::pair<SignalDisconnecter, ID> buildDisconnecter(T && disconnect)
	{
		std::function<void ()> disconnecter(ThisArgCaller<T>(std::forward<T>(disconnect)));
		ID id = disconnecter.template target<ThisArgCaller<T>>();
		return std::make_pair(SignalDisconnecter(std::move(disconnecter)), id);
	}

	SignalDisconnecter(std::function<void ()> && disconnecter);

	std::function<void ()> disconnecter;
};


template<typename... Arguments>
class Signal<void (Arguments...)>
{
public:
	Signal()
		: still_alive(std::make_shared<Signal *>(this))
	{
	}
	Signal(Signal && other)
		: slots(std::move(other.slots))
		, disconnecters(std::move(other.disconnecters))
		, still_alive(std::move(other.still_alive))
	{
		*still_alive = this;
	}
	Signal & operator=(Signal && other)
	{
		slots = std::move(other.slots);
		disconnecters = std::move(other.disconnecters);
		still_alive = std::move(other.still_alive);
		*still_alive = this;
		return *this;
	}

	/**
	 * Connect the given function or functor to this signal. It will be called
	 * every time that someone emits this signal.
	 * This function returns a SignalDisconnecter that will disconnect the slot
	 * when it is destroyed. Therefore if you ignore the return value this
	 * function will have no effect because the Disconnecter will disconnect
	 * immediately
	 */
	template<typename T>
	SignalDisconnecter connect(T && slot)
#ifndef _MSC_VER
		__attribute__((__warn_unused_result))
#endif
	{
		std::weak_ptr<Signal *> check_alive = still_alive;
		std::pair<SignalDisconnecter, SignalDisconnecter::ID> to_return = SignalDisconnecter::buildDisconnecter([check_alive](SignalDisconnecter::ID disconnecter)
		{
			// since this code is not thread safe, make it look not thread safe
			// the .lock() does not in fact keep the signal alive. it only keeps
			// the pointer to the signal alive
			if (check_alive.expired()) return;
			Signal & signal = **check_alive.lock();
			for (size_t i = 0; i < signal.disconnecters.size(); ++i)
			{
				if (signal.disconnecters[i] != disconnecter) continue;

				erase_unordered(signal.slots, i);
				erase_unordered(signal.disconnecters, i);
				break;
			}
		});
		
		slots.emplace_back(std::forward<T>(slot));
		disconnecters.push_back(to_return.second);

		return std::move(to_return.first);
	}

	template<typename T>
	void disconnect(const T & slot)
	{
		for (size_t i = 0; i < slots.size(); ++i)
		{
			const T * target = slots[i].template target<T>();
			if (!target) continue;
			if (*target != slot) continue;

			erase_unordered(slots, i);
			erase_unordered(disconnecters, i);
			break;
		}
	}

	void emit(const Arguments &... arguments) const
	{
		for (auto & slot : slots)
		{
			slot(arguments...);
		}
	}
	
	void clear()
	{
		slots.clear();
		disconnecters.clear();
	}

private:
#ifdef _MSC_VER
	Signal(const Signal & other); // intentionally not implemented
	Signal & operator=(const Signal & other); // intentionally not implemented
#endif


	std::vector<std::function<void (Arguments...)>> slots;
	std::vector<SignalDisconnecter::ID> disconnecters;
	std::shared_ptr<Signal *> still_alive;

	template<typename T>
	static void erase_unordered(T & container, size_t index)
	{
		std::iter_swap(container.begin() + index, container.end() - 1);
		container.pop_back();
	}
};

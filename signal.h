#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <iostream>

template<typename>
class Signal;

class Disconnecter
{
public:
	Disconnecter() = default;
	Disconnecter(Disconnecter &&) = default;
	Disconnecter & operator=(Disconnecter &&) = default;
	~Disconnecter();
	void disconnect();

	typedef const void * ID;

	struct DisconnecterBuilder
	{
		Disconnecter automaticDisconnect();
	private:
		template<typename>
		friend class Signal;

		DisconnecterBuilder(DisconnecterBuilder &&) = default;

		// I use the address of the functor as the ID
		// this guy does nothing except call the functor with it's
		// own address so that the functor knows it's id
		template<typename T>
		struct ThisArgCaller
		{
			ThisArgCaller(T && to_call)
				: to_call{std::move(to_call)}
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
		};
		template<typename T>
		DisconnecterBuilder(T && disconnect)
			: disconnecter{ThisArgCaller<T>{std::forward<T>(disconnect)}}
			, id{disconnecter.template target<ThisArgCaller<T>>()}
		{
		}
		ID getId() const;
		std::function<void ()> disconnecter;
		ID id;
	};

private:
	Disconnecter(std::function<void ()> && disconnecter);

	std::function<void ()> disconnecter;
};


template<typename... Arguments>
class Signal<void (Arguments...)>
{
public:
	Signal() = default;
	Signal(Signal &&) = default;
	Signal & operator=(Signal &&) = default;

	template<typename T>
	Disconnecter::DisconnecterBuilder connect(T && slot)
	{
		std::weak_ptr<Signal *> check_alive = still_alive;
		auto to_return = Disconnecter::DisconnecterBuilder{[check_alive](Disconnecter::ID disconnecter)
		{
			if (check_alive.expired()) return;
			// since this code is not thread safe, make it look not thread safe
			// the .lock() does not in fact keep the signal alive. it only keeps
			// the pointer to the signal alive
			Signal & signal = **check_alive.lock();
			for (size_t i = 0; i < signal.disconnecters.size(); ++i)
			{
				if (signal.disconnecters[i] != disconnecter) continue;

				signal.slots.erase(signal.slots.begin() + i);
				signal.disconnecters.erase(signal.disconnecters.begin() + i);
				break;
			}
		}};
		disconnecters.push_back(to_return.getId());
		try
		{
			slots.emplace_back(std::forward<T>(slot));
		}
		catch(...)
		{
			disconnecters.pop_back();
			throw;
		}

		return to_return;
	}

	template<typename T>
	void disconnect(const T & slot)
	{
		for (size_t i = 0; i < slots.size(); ++i)
		{
			const T * target = slots[i].template target<T>();
			if (!target) continue;
			if (*target != slot) continue;

			slots.erase(slots.begin() + i);
			disconnecters.erase(disconnecters.begin() + i);
			break;
		}
	}

	void emit(const Arguments &... arguments)
	{
		for (auto & slot : slots)
		{
			slot(arguments...);
		}
	}

private:
	std::vector<std::function<void (Arguments...)>> slots;
	std::vector<Disconnecter::ID> disconnecters;
	std::shared_ptr<Signal *> still_alive = std::make_shared<Signal *>(this);
};

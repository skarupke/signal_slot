#include "signal.h"
#include "typeSortedSignal.hpp"
#include <iostream>
#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
#endif
#include <random>
#include <cstring>
#include <chrono>

using namespace sig;

struct Updateable
{
	virtual ~Updateable() {}
	virtual void update(float dt) = 0;
};

struct UpdateableA : Updateable
{
	UpdateableA()
		: calls(0)
	{
	}
	virtual void update(float) override
	{
		++calls;
	}

	size_t calls;
};
struct UpdateableB : Updateable
{
	virtual void update(float) override
	{
		++calls;
	}
	static size_t calls;
};
size_t UpdateableB::calls = 0;


struct LambdaA
{
	explicit LambdaA(std::vector<std::function<void (float)>> & update_loop)
		: calls(0)
	{
		update_loop.emplace_back([this](float dt) { update(dt); });
	}
	void update(float)
	{
		++calls;
	}
	size_t calls;
};

struct LambdaB
{
	explicit LambdaB(std::vector<std::function<void (float)>> & update_loop)
	{
		update_loop.emplace_back([this](float dt) { update(dt); });
	}
	void update(float)
	{
		++calls;
	}
	static size_t calls;
};
size_t LambdaB::calls = 0;

struct SlotA
{
	explicit SlotA(Signal<void (float)> & update_loop)
		: calls(0)
		, update_disconnect(update_loop.connect([this](float dt) { update(dt); }))
	{
	}
	void update(float)
	{
		++calls;
	}
	size_t calls;
	SignalDisconnecter update_disconnect;
};

struct SlotB
{
	explicit SlotB(Signal<void (float)> & update_loop)
		: update_disconnect(update_loop.connect([this](float dt) { update(dt); }))
	{
	}
	void update(float)
	{
		++calls;
	}
	static size_t calls;
	SignalDisconnecter update_disconnect;
};
size_t SlotB::calls = 0;

struct ScopedMeasurer
{
	ScopedMeasurer(std::string name)
		: name(std::move(name))
		, clock()
		, before(clock.now())
	{
	}
	~ScopedMeasurer()
	{
		auto time_spent = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - before);
		std::cout << name << ": " << time_spent.count() << " ms" << std::endl;
	}
	std::string name;
	std::chrono::high_resolution_clock clock;
	std::chrono::time_point<std::chrono::high_resolution_clock> before;
};

int main(int argc, char * argv[])
{
#ifndef DISABLE_GTEST
	::testing::InitGoogleTest(&argc, argv);
	if (RUN_ALL_TESTS())
	{
		std::cin.get();
		return 1;
	}
#endif

	static const size_t num_allocations = 1000;
	static const size_t num_calls = 100000;

	{
		std::default_random_engine generator(argc);
		std::uniform_int_distribution<int> random_int(0, 1);
		Signal<void (float)> update_loop;
		std::vector<std::unique_ptr<SlotA>> slotsA;
		std::vector<std::unique_ptr<SlotB>> slotsB;
		for (size_t i = 0; i < num_allocations; ++i)
		{
			if (random_int(generator))
			{
				slotsA.emplace_back(new SlotA(update_loop));
			}
			else
			{
				slotsB.emplace_back(new SlotB(update_loop));
			}
		}
		ScopedMeasurer measure("signal");
		for (size_t i = 0; i < num_calls; ++i)
		{
			update_loop.emit(0.016f);
		}
	}
	{
		std::default_random_engine generator(argc);
		std::uniform_int_distribution<int> random_int(0, 1);
		std::vector<std::unique_ptr<Updateable>> updateables;
		std::vector<Updateable *> update_loop;
		for (size_t i = 0; i < num_allocations; ++i)
		{
			if (random_int(generator))
			{
				updateables.emplace_back(new UpdateableA);
			}
			else
			{
				updateables.emplace_back(new UpdateableB);
			}
			update_loop.push_back(updateables.back().get());
		}
		ScopedMeasurer measure("virtual function");
		for (size_t i = 0; i < num_calls; ++i)
		{
			for (auto & updateable : update_loop)
			{
				updateable->update(0.016f);
			}
		}
	}
	{
		std::default_random_engine generator(argc);
		std::uniform_int_distribution<int> random_int(0, 1);
		std::vector<std::function<void (float)>> update_loop;
		std::vector<std::unique_ptr<LambdaA>> slotsA;
		std::vector<std::unique_ptr<LambdaB>> slotsB;
		for (size_t i = 0; i < num_allocations; ++i)
		{
			if (random_int(generator))
			{
				slotsA.emplace_back(new LambdaA(update_loop));
			}
			else
			{
				slotsB.emplace_back(new LambdaB(update_loop));
			}
		}
		ScopedMeasurer measure("std::function");
		for (size_t i = 0; i < num_calls; ++i)
		{
			for (auto & updateable : update_loop)
			{
				updateable(0.016f);
			}
		}
	}
	std::cin.get();
}

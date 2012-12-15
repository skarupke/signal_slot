#include "signal.h"
#include <iostream>
#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
#endif
#include <random>
#include <cstring>

struct Updateable
{
	virtual ~Updateable() {}
	virtual void update(float dt) = 0;
};

struct UpdateableA : Updateable
{
	virtual void update(float dt) override
	{
		++calls;
	}

	size_t calls = 0;
};
struct UpdateableB : Updateable
{
	virtual void update(float dt) override
	{
		++calls;
	}
	static size_t calls;
};
size_t UpdateableB::calls = 0;


struct SlotA
{
	explicit SlotA(std::vector<std::function<void (float)>> & update_loop)
	{
		update_loop.emplace_back([this](float dt) { update(dt); });
	}
	void update(float dt)
	{
		++calls;
	}
	size_t calls = 0;
};

struct SlotB
{
	explicit SlotB(std::vector<std::function<void (float)>> & update_loop)
	{
		update_loop.emplace_back([this](float dt) { update(dt); });
	}
	void update(float dt)
	{
		++calls;
	}
	static size_t calls;
};
size_t SlotB::calls = 0;

int main(int argc, char * argv[])
{
#ifndef DISABLE_GTEST
	::testing::InitGoogleTest(&argc, argv);
	assert(!RUN_ALL_TESTS());
#endif

	const size_t num_allocations = 10000;
	const size_t num_calls = 100000;

	std::default_random_engine generator(argc);
	std::uniform_int_distribution<int> random_int(0, 1);
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "virtual_function") == 0)
		{
			std::vector<std::unique_ptr<Updateable>> updateables;
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
			}
			for (int i = 0; i < num_calls; ++i)
			{
				for (auto & updateable : updateables)
				{
					updateable->update(0.016f);
				}
			}
		}
		else if (std::strcmp(argv[i], "std_function") == 0)
		{
			std::vector<std::function<void (float)>> update_loop;
			std::vector<std::unique_ptr<SlotA>> slotsA;
			std::vector<std::unique_ptr<SlotB>> slotsB;
			for (size_t i = 0; i < num_allocations; ++i)
			{
				if (random_int(generator))
				{
					slotsA.emplace_back(new SlotA{update_loop});
				}
				else
				{
					slotsB.emplace_back(new SlotB{update_loop});
				}
			}
			for (int i = 0; i < num_calls; ++i)
			{
				for (auto & updateable : update_loop)
				{
					updateable(0.016f);
				}
			}
		}
	}
}

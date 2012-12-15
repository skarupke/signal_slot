#include "signal.h"

Disconnecter::Disconnecter(std::function<void ()> && disconnecter)
	: disconnecter{std::move(disconnecter)}
{
}

Disconnecter::~Disconnecter()
{
	disconnect();
}
void Disconnecter::disconnect()
{
	if (!disconnecter) return;

	disconnecter();
	disconnecter = nullptr;
}

Disconnecter Disconnecter::DisconnecterBuilder::automaticDisconnect()
{
	return Disconnecter{std::move(disconnecter)};
}

const void * Disconnecter::DisconnecterBuilder::getId() const
{
	return id;
}


#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(signals_slots, disconnecters)
{
	int outer_fired = 0;
	int inner_fired = 0;
	int inner_fired2 = 0;
	Disconnecter outer_disconnect;
	{
		Signal<void ()> signal;

		signal.emit();
		{
			auto disconnect2 = signal.connect([&inner_fired2]{ ++inner_fired2; }).automaticDisconnect();
			outer_disconnect = signal.connect([&outer_fired]{ ++outer_fired; }).automaticDisconnect();
			auto disconnect = signal.connect([&inner_fired]{ ++inner_fired; }).automaticDisconnect();
			signal.emit();
		}
		signal.emit();
	}

	EXPECT_EQ(1, inner_fired);
	EXPECT_EQ(1, inner_fired2);
	EXPECT_EQ(2, outer_fired);
}

TEST(signals_slots, member_functions)
{
	struct S
	{
		S(int a)
			: a{a}
		{
		}
		void foo(int a)
		{
			this->a = a;
		}
		int a = 0;
	};
	Signal<void (S &, int)> signal;
	signal.connect(&S::foo);
	S s(3);
	signal.emit(s, 4);
	EXPECT_EQ(4, s.a);
}

#endif

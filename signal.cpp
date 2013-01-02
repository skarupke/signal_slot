#include "signal.h"

SignalDisconnecter::SignalDisconnecter()
	: disconnecter()
{
}
SignalDisconnecter::SignalDisconnecter(SignalDisconnecter && other)
	: disconnecter(std::move(other.disconnecter))
{
}
SignalDisconnecter & SignalDisconnecter::operator=(SignalDisconnecter && other)
{
	disconnecter = std::move(other.disconnecter);
	return *this;
}
SignalDisconnecter::SignalDisconnecter(std::function<void ()> && disconnecter)
	: disconnecter(std::move(disconnecter))
{
}

SignalDisconnecter::~SignalDisconnecter()
{
	disconnect();
}
void SignalDisconnecter::disconnect()
{
	if (!disconnecter) return;

	disconnecter();
	disconnecter = nullptr;
}
std::function<void ()> SignalDisconnecter::releaseDisconnecter()
{
	return std::move(disconnecter);
}

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(signals_slots, disconnecters)
{
	int outer_fired = 0;
	int inner_fired = 0;
	int inner_fired2 = 0;
	SignalDisconnecter outer_disconnect;
	{
		Signal<void ()> signal;

		signal.emit();
		{
			auto disconnect2 = signal.connect([&inner_fired2]{ ++inner_fired2; });
			outer_disconnect = signal.connect([&outer_fired]{ ++outer_fired; });
			auto disconnect = signal.connect([&inner_fired]{ ++inner_fired; });
			signal.emit();
		}
		signal.emit();
	}

	EXPECT_EQ(1, inner_fired);
	EXPECT_EQ(1, inner_fired2);
	EXPECT_EQ(2, outer_fired);
}

#ifndef _MSC_VER
TEST(signals_slots, member_functions)
{
	struct S
	{
		S(int a)
			: a(a)
		{
		}
		void foo(int a)
		{
			this->a = a;
		}
		int a;
	};
	Signal<void (S &, int)> signal;
	signal.connect(&S::foo).releaseDisconnecter();
	S s((3));
	signal.emit(s, 4);
	EXPECT_EQ(4, s.a);
}
#endif

TEST(signals_slots, moving)
{
	int num_fired = 0;
	Signal<void ()> outer;
	{
		SignalDisconnecter disconnect;
		{
			Signal<void ()> inner;
			disconnect = inner.connect([&num_fired]{ ++num_fired; });
			outer = std::move(inner);
			inner.emit();
			EXPECT_EQ(0, num_fired);
			outer.emit();
			EXPECT_EQ(1, num_fired);
		}
		outer.emit();
		EXPECT_EQ(2, num_fired);
	}
	outer.emit();
	EXPECT_EQ(2, num_fired);
}

TEST(signals_slots, mutable)
{
	int a = 0;
	int b = 0;
	Signal<void (int &)> sig;
	sig.connect([a](int & b) mutable { b = ++a; }).releaseDisconnecter();
	sig.emit(b);
	EXPECT_EQ(0, a);
	EXPECT_EQ(1, b);
	sig.emit(b);
	EXPECT_EQ(0, a);
	EXPECT_EQ(2, b);
}

#endif

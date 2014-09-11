#include "signal.hpp"

namespace sig
{

static std::vector<size_t> global_free_ids;

SignalDisconnecter::ID SignalDisconnecter::get_next_id()
{
	if (global_free_ids.empty())
	{
		static SignalDisconnecter::ID id = 1;
		return id++;
	}
	else
	{
		ID to_return = global_free_ids.back();
		global_free_ids.pop_back();
		return to_return;
	}
}
void SignalDisconnecter::free_id(ID id)
{
	global_free_ids.push_back(id);
}

SignalDisconnecter::SignalDisconnecter()
	: disconnecter()
{
}
SignalDisconnecter::SignalDisconnecter(const SignalDisconnecter &)
	: disconnecter()
{
}
SignalDisconnecter::SignalDisconnecter(SignalDisconnecter &&)
	: disconnecter()
{
}
SignalDisconnecter::SignalDisconnecter(MovableSignalDisconnecter && builder)
	: disconnecter(std::move(builder.disconnecter))
{
}
SignalDisconnecter & SignalDisconnecter::operator=(MovableSignalDisconnecter && builder)
{
	disconnect();
	disconnecter = std::move(builder.disconnecter);
	return *this;
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
func::function<void ()> SignalDisconnecter::release_disconnecter()
{
	return std::move(disconnecter);
}
MovableSignalDisconnecter::MovableSignalDisconnecter(func::function<void ()> && disconnecter)
	: disconnecter(std::move(disconnecter))
{
}
MovableSignalDisconnecter::MovableSignalDisconnecter(MovableSignalDisconnecter &&) = default;
MovableSignalDisconnecter & MovableSignalDisconnecter::operator=(MovableSignalDisconnecter &&) = default;
MovableSignalDisconnecter::MovableSignalDisconnecter(SignalDisconnecter && other)
	: disconnecter(std::move(other.disconnecter))
{
}
MovableSignalDisconnecter & MovableSignalDisconnecter::operator=(SignalDisconnecter && other)
{
	disconnect();
	disconnecter = std::move(other.disconnecter);
	return *this;
}
MovableSignalDisconnecter::~MovableSignalDisconnecter()
{
	disconnect();
}
void MovableSignalDisconnecter::disconnect()
{
	if (!disconnecter) return;

	disconnecter();
	disconnecter = nullptr;
}
func::function<void ()> MovableSignalDisconnecter::release_disconnecter()
{
	return std::move(disconnecter);
}
}

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
using namespace sig;

TEST(signal_detail, find_index_if)
{
	std::vector<size_t> none;
	EXPECT_EQ(static_cast<size_t>(-1), detail::find_index_if_two_way(none, [](size_t){ return true; }));

	std::vector<size_t> one = { 1 };
	EXPECT_EQ(0, detail::find_index_if_two_way(one, [](size_t i){ return i == 1; }));

	std::vector<size_t> two = { 1, 2 };
	EXPECT_EQ(0, detail::find_index_if_two_way(two, [](size_t i){ return i == 1; }));
	EXPECT_EQ(1, detail::find_index_if_two_way(two, [](size_t i){ return i == 2; }));

	std::vector<size_t> five = { 1, 2, 3, 4, 5 };
	EXPECT_EQ(0, detail::find_index_if_two_way(five, [](size_t i){ return i == 1; }));
	EXPECT_EQ(1, detail::find_index_if_two_way(five, [](size_t i){ return i == 2; }));
	EXPECT_EQ(2, detail::find_index_if_two_way(five, [](size_t i){ return i == 3; }));
	EXPECT_EQ(3, detail::find_index_if_two_way(five, [](size_t i){ return i == 4; }));
	EXPECT_EQ(4, detail::find_index_if_two_way(five, [](size_t i){ return i == 5; }));

	std::vector<size_t> six = { 1, 2, 3, 4, 5, 6 };
	EXPECT_EQ(0, detail::find_index_if_two_way(six, [](size_t i){ return i == 1; }));
	EXPECT_EQ(1, detail::find_index_if_two_way(six, [](size_t i){ return i == 2; }));
	EXPECT_EQ(2, detail::find_index_if_two_way(six, [](size_t i){ return i == 3; }));
	EXPECT_EQ(3, detail::find_index_if_two_way(six, [](size_t i){ return i == 4; }));
	EXPECT_EQ(4, detail::find_index_if_two_way(six, [](size_t i){ return i == 5; }));
	EXPECT_EQ(5, detail::find_index_if_two_way(six, [](size_t i){ return i == 6; }));
	EXPECT_EQ(static_cast<size_t>(-1), detail::find_index_if_two_way(six, [](size_t i){ return i == 7; }));
}

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
	auto disconnect = signal.connect(&S::foo);
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
	auto disconnect = sig.connect([a](int & b) mutable { b = ++a; });
	sig.emit(b);
	EXPECT_EQ(0, a);
	EXPECT_EQ(1, b);
	sig.emit(b);
	EXPECT_EQ(0, a);
	EXPECT_EQ(2, b);
}

#endif

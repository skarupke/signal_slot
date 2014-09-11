#include "sortedSignal.hpp"
#include <string>

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
using namespace sig;

TEST(sorted_signal, dependency)
{
	SortedSignal<std::string, void ()> signal;
	bool firedA = false;
	bool firedB = false;
	bool firedC = false;
	auto disconnect_c = signal.connect("c", [&]{ EXPECT_TRUE(firedA); firedC = true; });
	auto disconnect_b = signal.connect("b", [&]{ firedB = true; });
	auto disconnect_a = signal.connect("a", [&]{ EXPECT_TRUE(firedB); firedA = true; });
	signal.add_dependency("a", "c");
	signal.add_dependency("b", "a");
	signal.emit();
	EXPECT_TRUE(firedC);
}

TEST(sorted_signal, invalid_dependency)
{
	SortedSignal<std::string, void ()> signal;
	auto disconnect = signal.connect("a", []{});
	bool had_exception = false;
	try
	{
		signal.add_dependency("a", "a");
	}
	catch(const circular_dependency_error<std::string> &)
	{
		had_exception = true;
	}
	EXPECT_TRUE(had_exception);
}

TEST(sorted_signal, circular_dependency)
{
	SortedSignal<std::string, void ()> signal;
	auto disconnect_c = signal.connect("c", []{});
	auto disconnect_b = signal.connect("b", []{});
	auto disconnect_a = signal.connect("a", []{});
	signal.add_dependency("a", "c");
	signal.add_dependency("b", "a");
	bool had_exception = false;
	try
	{
		signal.add_dependency("c", "b");
	}
	catch(const circular_dependency_error<std::string> &)
	{
		had_exception = true;
	}
	EXPECT_TRUE(had_exception);
}

TEST(sorted_signal, no_dependency)
{
	SortedSignal<std::string, void ()> signal;
	bool fired = false;
	auto disconnect = signal.connect("a", [&fired]{ fired = true; });
	signal.emit();
	ASSERT_TRUE(fired);
}


TEST(sorted_signal, remove_during_emit)
{
	typedef SortedSignal<std::string, void(), WillBeModifiedDuringEmit> SignalType;
	static size_t moved_count = 0;
	struct S
	{
		S(size_t & count, SignalType & signal)
			: count(count)
			, signal(signal)
		{
			connect();
		}
		S(S && other)
			: count(moved_count)
			, signal(other.signal)
		{
			connect();
		}
		~S()
		{
			++count;
		}

		void check()
		{
			// should never be called
			ASSERT_TRUE(false);
		}

		void connect()
		{
			disconnect = signal.connect("S", [this]{ check(); });
		}

		size_t & count;
		SignalType & signal;
		SignalDisconnecter disconnect;
	};
	SignalType signal;
	std::vector<S> slots;
	size_t count = 1;
	auto clear_disconnect = signal.connect("clear", [&]
	{
		slots.clear();
	});
	auto add_disconnect = signal.connect("add", [&]
	{
		for (size_t i = 0; i < count; ++i)
		{
			slots.emplace_back(count, signal);
		}
	});
	signal.add_dependency("clear", "S");
	signal.add_dependency("S", "add");
	signal.emit();
	signal.emit();
	signal.emit();
}

#endif

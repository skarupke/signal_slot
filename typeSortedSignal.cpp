#include "typeSortedSignal.hpp"





#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
using namespace sig;

TEST(type_sorted_signal, dependency)
{
	TypeSortedSignal<void ()> signal;
	bool firedInt = false;
	bool firedFloat = false;
	bool firedBool = false;
	auto disconnect_int = signal.connect<int>([&]{ EXPECT_TRUE(firedBool); firedInt = true; });
	auto disconnect_float = signal.connect<float>([&]{ firedFloat = true; });
	auto disconnect_bool = signal.connect<bool>([&]{ EXPECT_TRUE(firedFloat); firedBool = true; });
	signal.add_dependency<bool, int>();
	signal.add_dependency<float, bool>();
	signal.emit();
	EXPECT_TRUE(firedInt);
}

TEST(type_sorted_signal, disconnect)
{
	void (*slot)(int &) = [](int & a) { ++a; };
	TypeSortedSignal<void (int &)> signal;
	auto disconnect = signal.connect<void>(slot);
	int a = 0;
	signal.emit(a);
	EXPECT_EQ(1, a);
	signal.disconnect<void>(slot);
	signal.emit(a);
	EXPECT_EQ(1, a);
}

TEST(type_sorted_signal, clear)
{
	TypeSortedSignal<void (), WillBeModifiedDuringEmit> signal;
	size_t fired_int = 0;
	auto disconnect_int = signal.connect<int>([&]{ ++fired_int; });
	size_t fired_float = 0;
	auto disconnect_float = signal.connect<float>([&]{ ++fired_float; });
	signal.emit();
	EXPECT_EQ(1, fired_int);
	EXPECT_EQ(1, fired_float);
	signal.clear<int>();
	signal.emit();
	EXPECT_EQ(1, fired_int);
	EXPECT_EQ(2, fired_float);
	signal.clear();
	signal.emit();
	EXPECT_EQ(1, fired_int);
	EXPECT_EQ(2, fired_float);
}

#endif

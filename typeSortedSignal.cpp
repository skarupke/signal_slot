#include "typeSortedSignal.h"





#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(type_sorted_signal, dependency)
{
	TypeSortedSignal<void ()> signal;
	bool firedInt = false;
	bool firedFloat = false;
	bool firedBool = false;
	signal.connect<int>([&]{ EXPECT_TRUE(firedBool); firedInt = true; }).releaseDisconnecter();
	signal.connect<float>([&]{ firedFloat = true; }).releaseDisconnecter();
	signal.connect<bool>([&]{ EXPECT_TRUE(firedFloat); firedBool = true; }).releaseDisconnecter();
	signal.addDependency<bool, int>();
	signal.addDependency<float, bool>();
	signal.emit();
	EXPECT_TRUE(firedInt);
}

TEST(type_sorted_signal, disconnect)
{
	void (*slot)(int &) = [](int & a) { ++a; };
	TypeSortedSignal<void (int &)> signal;
	signal.connect<void>(slot).releaseDisconnecter();
	int a = 0;
	signal.emit(a);
	EXPECT_EQ(1, a);
	signal.disconnect<void>(slot);
	signal.emit(a);
	EXPECT_EQ(1, a);
}

#endif

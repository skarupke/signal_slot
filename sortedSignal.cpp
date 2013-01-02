#include "sortedSignal.h"
#include <string>

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(sorted_signal, dependency)
{
	SortedSignal<std::string, void ()> signal;
	bool firedA = false;
	bool firedB = false;
	bool firedC = false;
	signal.connect("c", [&]{ EXPECT_TRUE(firedA); firedC = true; }).releaseDisconnecter();
	signal.connect("b", [&]{ firedB = true; }).releaseDisconnecter();
	signal.connect("a", [&]{ EXPECT_TRUE(firedB); firedA = true; }).releaseDisconnecter();
	signal.addDependency("a", "c");
	signal.addDependency("b", "a");
	signal.emit();
	EXPECT_TRUE(firedC);
}

TEST(sorted_signal, invalid_dependency)
{
	SortedSignal<std::string, void ()> signal;
	signal.connect("a", []{}).releaseDisconnecter();
	bool had_exception = false;
	try
	{
		signal.addDependency("a", "a");
	}
	catch(const sig::circular_dependency_error<std::string> &)
	{
		had_exception = true;
	}
	EXPECT_TRUE(had_exception);
}

TEST(sorted_signal, circular_dependency)
{
	SortedSignal<std::string, void ()> signal;
	signal.connect("c", []{}).releaseDisconnecter();
	signal.connect("b", []{}).releaseDisconnecter();
	signal.connect("a", []{}).releaseDisconnecter();
	signal.addDependency("a", "c");
	signal.addDependency("b", "a");
	bool had_exception = false;
	try
	{
		signal.addDependency("c", "b");
	}
	catch(const sig::circular_dependency_error<std::string> &)
	{
		had_exception = true;
	}
	EXPECT_TRUE(had_exception);
}

TEST(sorted_signal, no_dependency)
{
	SortedSignal<std::string, void ()> signal;
	bool fired = false;
	signal.connect("a", [&fired]{ fired = true; }).releaseDisconnecter();
	signal.emit();
	ASSERT_TRUE(fired);
}

#endif

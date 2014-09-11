#include "small_shared_ptr.hpp"

static_assert(sizeof(ptr::small_shared_ptr<int>) == sizeof(void *), "expecting the small_shared_ptr to be the size of just one pointer");

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(small_shared_ptr, ref_count)
{
	struct DestructorCounter
	{
		DestructorCounter(int & count)
			: count(count)
		{
		}
		~DestructorCounter()
		{
			++count;
		}
		int & count;
	};

	int count = 0;
	{
		ptr::small_shared_ptr<DestructorCounter> a = ptr::make_shared<DestructorCounter>(count);
		ptr::small_shared_ptr<DestructorCounter> b = std::move(a);
		ptr::small_shared_ptr<DestructorCounter> c;
		{
			ptr::small_shared_ptr<DestructorCounter> d = b;
			c = d;
		}
		EXPECT_EQ(0, count);
	}
	EXPECT_EQ(1, count);
}

TEST(small_shared_ptr, get)
{
	ptr::small_shared_ptr<int> a;
	EXPECT_EQ(nullptr, a.get());
	EXPECT_EQ(nullptr, a);
	a = ptr::make_shared<int>(1);
	EXPECT_NE(nullptr, a.get());
	EXPECT_NE(nullptr, a);
}

#endif

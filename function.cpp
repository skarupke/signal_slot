#include "function.hpp"

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(function, empty)
{
	func::function<void ()> empty;
	ASSERT_FALSE(empty);
	ASSERT_THROW(empty(), func::bad_function_call);
}

static void call_provided_function(const func::function<void ()> & func)
{
	func();
}
static void call_provided_function(const func::function<void (int)> & func)
{
	func(5);
}

TEST(function, overloading)
{
	int i = 0;
	call_provided_function([&i]{ i = 3; });
	ASSERT_EQ(3, i);
	call_provided_function([&i](int a) { i = a; });
	ASSERT_EQ(5, i);
}

TEST(function, assignment)
{
	int a = 0;
	func::function<void ()> increment = [&a]{ ++a; };
	increment();
	ASSERT_EQ(1, a);

	increment = [increment]{ increment(); increment(); };
	increment();
	ASSERT_EQ(3, a);

	increment = [increment]{ increment(); increment(); };
	increment();
	ASSERT_EQ(7, a);

	func::function<void ()> move_to = std::move(increment);
	ASSERT_FALSE(increment);
	move_to();
	ASSERT_EQ(11, a);
}

TEST(function, allocator_and_target)
{
	auto some_lambda = []{};
	func::function<void ()> lambdaer(std::allocator_arg, std::allocator<decltype(some_lambda)>(), some_lambda);
	func::function<void ()> lambda_copy(std::allocator_arg, std::allocator<decltype(some_lambda)>(), lambdaer);
	func::function<void ()> allocator_copy(std::allocator_arg, std::allocator<int>(), lambdaer);
	func::function<void ()> allocator_copy2(std::allocator_arg, std::allocator<float>(), allocator_copy);
	lambda_copy();
	allocator_copy();
	allocator_copy2();

#	ifndef FUNC_NO_RTTI
		ASSERT_TRUE(lambdaer.target<decltype(some_lambda)>());
		ASSERT_TRUE(lambda_copy.target<decltype(some_lambda)>());
		ASSERT_TRUE(const_cast<const func::function<void ()> &>(allocator_copy).target<func::function<void ()>>());
		ASSERT_FALSE(lambdaer.target<func::function<void ()>>());
		ASSERT_FALSE(const_cast<const func::function<void ()> &>(allocator_copy).target<decltype(some_lambda)>());
#	endif
}

TEST(function, noexcept_reference)
{
	int i = 0;
	size_t padding_a = 0;
	size_t padding_b = 0;
	size_t padding_c = 1;
	size_t padding_d = 0;
	auto some_lambda = [&i, padding_a, padding_b, padding_c, padding_d]{ i += static_cast<int>(padding_a + padding_b + padding_c + padding_d); };
	auto reference = std::ref(some_lambda);
	func::function<void ()> ref_func(reference);
	static_assert(noexcept(ref_func = reference), "standard says this is noexcept");
	static_assert(!noexcept(ref_func = some_lambda), "this can not be noexcept because it allocates");
	ref_func();
	ASSERT_EQ(1, i);
	ref_func.assign(reference, std::allocator<decltype(reference)>());
	ref_func();
	ASSERT_EQ(2, i);
}

TEST(function, member_function)
{
	struct S
	{
		S(int a) : a(a) {}
		float foo() const
		{
			return 1.0f / static_cast<float>(a);
		}
		int a;
	};
	func::function<float (const S &)> mem_fun(&S::foo);
	S s(5);
	EXPECT_EQ(0.2f, mem_fun(s));
}

#endif


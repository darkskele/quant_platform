#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

#include "viewable_pool.hpp"

namespace {

template <bool UseHeap>
using Pool = qp::ViewablePool<int, 4, UseHeap>;

template <typename T>
class ViewablePoolMoveTest : public ::testing::Test {};

using StorageModes = ::testing::Types<std::bool_constant<false>, std::bool_constant<true>>;
TYPED_TEST_SUITE(ViewablePoolMoveTest, StorageModes);

TYPED_TEST(ViewablePoolMoveTest, MoveConstructionPreservesContents) {
    Pool<TypeParam::value> pool;
    pool.push(7);
    pool.push(8);

    Pool<TypeParam::value> moved{std::move(pool)};
    ASSERT_EQ(moved.size(), 2u);
    EXPECT_EQ(moved.view()[0], 7);
    EXPECT_EQ(moved.view()[1], 8);
}

TYPED_TEST(ViewablePoolMoveTest, MoveAssignmentPreservesContents) {
    Pool<TypeParam::value> pool;
    pool.push(11);

    Pool<TypeParam::value> other;
    other.push(99);
    other = std::move(pool);

    ASSERT_EQ(other.size(), 1u);
    EXPECT_EQ(other.view()[0], 11);
}

}  // namespace

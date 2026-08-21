#include <gtest/gtest.h>

#include <vector>

#include "round_robin_pool.hpp"

namespace {

struct DoubleTask {
    int operator()(const int& ctx) { return ctx * 2; }
};

struct SquareTask {
    int operator()(const int& ctx) { return ctx * ctx; }
};

struct AddTenTask {
    int operator()(const int& ctx) { return ctx + 10; }
};

}  // namespace

TEST(RoundRobinPool, FewerWorkersThanTasksStillRunsEveryTask) {
    // 3 tasks, 2 workers -> one worker runs two tasks per round.
    qp::RoundRobinPool<2, int, int, DoubleTask, SquareTask, AddTenTask> pool{
        DoubleTask{}, SquareTask{}, AddTenTask{}};

    std::vector<int>         results(3);
    std::vector<std::size_t> observed_order;
    pool.run_round(5, [&](std::size_t i, int result) {
        observed_order.push_back(i);
        results[i] = result;
    });

    EXPECT_EQ(results[0], 10);  // 5 * 2
    EXPECT_EQ(results[1], 25);  // 5 * 5
    EXPECT_EQ(results[2], 15);  // 5 + 10
    EXPECT_EQ(observed_order,
              (std::vector<std::size_t>{0, 1, 2}));  // strict index order, not arrival order
}

TEST(RoundRobinPool, OneWorkerRunsEveryTaskSequentially) {
    qp::RoundRobinPool<1, int, int, DoubleTask, SquareTask> pool{DoubleTask{}, SquareTask{}};

    std::vector<int> results(2);
    pool.run_round(3, [&](std::size_t i, int result) { results[i] = result; });

    EXPECT_EQ(results[0], 6);
    EXPECT_EQ(results[1], 9);
}

TEST(RoundRobinPool, OneWorkerPerTaskIsAlsoValid) {
    qp::RoundRobinPool<3, int, int, DoubleTask, SquareTask, AddTenTask> pool{
        DoubleTask{}, SquareTask{}, AddTenTask{}};

    std::vector<int> results(3);
    pool.run_round(4, [&](std::size_t i, int result) { results[i] = result; });

    EXPECT_EQ(results[0], 8);
    EXPECT_EQ(results[1], 16);
    EXPECT_EQ(results[2], 14);
}

TEST(RoundRobinPool, SuccessiveRoundsSeeTheNewContextNotStaleResults) {
    qp::RoundRobinPool<1, int, int, DoubleTask> pool{DoubleTask{}};

    int last = 0;
    pool.run_round(1, [&](std::size_t, int result) { last = result; });
    EXPECT_EQ(last, 2);

    pool.run_round(100, [&](std::size_t, int result) { last = result; });
    EXPECT_EQ(last, 200);
}

#include <iostream>

#include "backtest.hpp"

int main(int argc, char** argv) {
    auto config = qp::backtest::parse_args(argc, argv);
    if (!config) return 1;

    try {
        auto results = qp::backtest::run(*config);
        std::cout << "final_cash=" << results.final_cash << " final_equity=" << results.final_equity
                  << " final_spot_position=" << results.final_spot_position
                  << " final_futures_position=" << results.final_futures_position << "\n";
    } catch (const std::exception& e) {
        std::cerr << "backtest failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

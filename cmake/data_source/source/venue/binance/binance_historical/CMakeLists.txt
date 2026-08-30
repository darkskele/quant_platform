# source/venue/binance/binance_historical — binance_historical's venue::
# Parser: BinHistParserPolicy (field extraction/record-kind dispatch) +
# BinHistSymbolTable (a real, verified 10-symbol universe, BTCUSDT etc.,
# D13-style verified against live Binance exchangeInfo +
# data.binance.vision, not assumed), bound together as BinHistVenue
# (bin_hist_venue.hpp) — that alias is the only thing outside this
# directory should ever name. Funding-entry parsing uses simdjson (already
# a vcpkg.json dependency, not newly added); kline parsing isn't
# implemented yet — see bin_hist_parser_policy.hpp's TODO, blocked on a
# real MarketEvent::EventKind decision, not a missing detail.
qp_dir(include QP_INC)
qp_dir(tests QP_TESTS)
qp_dir(benchmarks QP_BENCH)

find_package(simdjson CONFIG REQUIRED)

add_library(qp_source_venue_binance_historical INTERFACE)
target_include_directories(qp_source_venue_binance_historical INTERFACE ${QP_INC})
target_link_libraries(qp_source_venue_binance_historical INTERFACE qp_source_venue simdjson::simdjson)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(qp_source_venue_binance_historical_tests
        ${QP_TESTS}/test_bin_hist_symbol_table.cpp
        ${QP_TESTS}/test_bin_hist_venue.cpp
    )
    target_link_libraries(qp_source_venue_binance_historical_tests
        PRIVATE qp_source_venue_binance_historical GTest::gtest GTest::gtest_main
    )
    qp_register_test(qp_source_venue_binance_historical_tests)
endif()

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_source_venue_binance_historical_bench ${QP_BENCH}/bench_bin_hist_symbol_table.cpp)
    target_link_libraries(qp_source_venue_binance_historical_bench
        PRIVATE qp_source_venue_binance_historical benchmark::benchmark_main
    )
endif()

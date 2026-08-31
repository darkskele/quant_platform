# source — the Source concept and the venue-agnostic types
# (SymbolTable/WsEndpoint/RestEndpoint/DepthSnapshot) a Parser is built
# against (see docs/DESIGN.md). Header-only, depends on core only.
#
# The live-transport machinery (LiveWebSocketSource, resync/gap-detection/
# backoff, the Binance WS parser) was removed along with apps/collector
# (docs/decisions.md) — this town is being redesigned around historical/
# replay data instead of live capture; venue/ (add_subdirectory'd below)
# is where a venue's wire-protocol glue and symbol universe live now —
# one subdirectory per (exchange, source-type), e.g. venue/binance/
# binance_historical/.
qp_dir(include QP_INC)
qp_dir(benchmarks QP_BENCH)

add_library(qp_source_pure INTERFACE)
target_include_directories(qp_source_pure INTERFACE ${QP_INC})
target_link_libraries(qp_source_pure INTERFACE qp_core)

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_source_pure_bench ${QP_BENCH}/bench_venue_types.cpp)
    target_link_libraries(qp_source_pure_bench PRIVATE qp_source_pure benchmark::benchmark_main)
endif()

add_subdirectory(venue)

# CsvSource — the backtest Source implementation: genuinely venue-agnostic
# (templated on any venue::Parser), reads whole CSV files and merges N
# streams by timestamp. Replaces the old FileReplaySource/wire+zstd
# lineage entirely for this venue — that codec was built for a future
# self-recorded live-collector format that doesn't exist yet, not for
# replaying vendor-delivered historical dumps (see git history if that
# lineage is ever needed again; deleted, not hidden behind a flag).
# Header-only, INTERFACE: the class is templated on N (how many streams it
# merges), so there's no non-template translation unit to compile
# separately.
add_library(qp_csv_source INTERFACE)
target_include_directories(qp_csv_source INTERFACE ${QP_INC})
target_link_libraries(qp_csv_source INTERFACE qp_source_pure qp_source_venue)

if(QP_BUILD_BENCHMARKS)
    add_executable(qp_csv_source_bench ${QP_BENCH}/bench_csv_source.cpp)
    target_link_libraries(qp_csv_source_bench
        PRIVATE qp_csv_source qp_source_venue_binance_historical qp_core_test_support
                benchmark::benchmark_main
    )
endif()

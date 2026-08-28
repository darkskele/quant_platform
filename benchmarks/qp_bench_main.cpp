#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef QP_REPO_ROOT
#error "QP_REPO_ROOT must be defined by CMake (see root CMakeLists.txt's qp_bench target)"
#endif

// The one bench binary: runs everything (or whatever --benchmark_filter
// narrows it to) with the repo's standard variance recipe
// (benchmarks/README.md) baked in as defaults, and regenerates the
// checked-in BENCHMARKS.md every time it's run — one launch, no flags to
// remember, no separate `tools/bench/bench_to_md.py` step.
//
// Defaults are injected as ordinary command-line flags ahead of the real
// argv, not set directly on Google Benchmark's internal FLAGS_* globals
// (not part of the public benchmark.h API) — its flag parser
// (ParseCommandLineFlags, benchmark.cc) is a plain left-to-right scan
// where a later occurrence of the same flag overwrites the earlier one,
// so any of these the caller explicitly passes still wins.
int main(int argc, char** argv) {
    const std::string json_path = std::string(QP_REPO_ROOT) + "/build/bench_results.json";
    const std::string reps_flag = "--benchmark_repetitions=5";
    const std::string agg_flag  = "--benchmark_report_aggregates_only=true";
    const std::string out_flag  = "--benchmark_out=" + json_path;
    const std::string fmt_flag  = "--benchmark_out_format=json";
    // Without this, Google Benchmark's own default --benchmark_min_time
    // applies per case — across ~30 files' worth of cases x 5 repetitions,
    // that's well past ten minutes (measured: killed a 280s run before it
    // finished). 0.1s matches what this repo's own bench work has used
    // throughout (docs/environment.md: trust deltas on WSL2, not absolute
    // numbers, so a short min_time is the right default, not a shortcut).
    const std::string time_flag = "--benchmark_min_time=0.1s";

    std::vector<char*> combined;
    combined.reserve(static_cast<std::size_t>(argc) + 5);
    combined.push_back(argv[0]);
    combined.push_back(const_cast<char*>(reps_flag.c_str()));
    combined.push_back(const_cast<char*>(agg_flag.c_str()));
    combined.push_back(const_cast<char*>(out_flag.c_str()));
    combined.push_back(const_cast<char*>(fmt_flag.c_str()));
    combined.push_back(const_cast<char*>(time_flag.c_str()));
    for (int i = 1; i < argc; ++i) combined.push_back(argv[i]);

    int combined_argc = static_cast<int>(combined.size());
    benchmark::Initialize(&combined_argc, combined.data());
    if (benchmark::ReportUnrecognizedArguments(combined_argc, combined.data())) return 1;
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    const std::string md_path = std::string(QP_REPO_ROOT) + "/BENCHMARKS.md";
    const std::string script  = std::string(QP_REPO_ROOT) + "/tools/bench/bench_to_md.py";
    const std::string cmd     = "python3 \"" + script + "\" \"" + json_path +
                            "\" --title \"Full benchmark suite (repetitions=5)\" -o \"" + md_path +
                            "\"";
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::fprintf(stderr, "warning: bench_to_md.py failed (exit %d) — %s not regenerated\n", rc,
                     md_path.c_str());
    } else {
        std::fprintf(stderr, "wrote %s\n", md_path.c_str());
    }
    return 0;
}

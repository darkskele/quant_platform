#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#ifndef QP_REPO_ROOT
#error "QP_REPO_ROOT must be defined by CMake (see root CMakeLists.txt's qp_bench target)"
#endif

// The one bench binary: runs everything (or whatever --benchmark_filter
// narrows it to) with the repo's standard variance recipe
// (benchmarks/README.md) baked in as defaults, writes the raw numbers to
// the tracked bench_results.json, and regenerates the checked-in
// BENCHMARKS.md — a grouped summary of every benchmark followed by a diff
// against whatever bench_results.json is committed at HEAD. The intended
// workflow is running this right before a commit to see what your
// uncommitted changes did to perf. bench_results.json is the source of
// truth for absolute numbers; BENCHMARKS.md is the readable view — see
// tools/bench/bench_to_md.py.
//
// Defaults are injected as ordinary command-line flags ahead of the real
// argv, not set directly on Google Benchmark's internal FLAGS_* globals
// (not part of the public benchmark.h API) — its flag parser
// (ParseCommandLineFlags, benchmark.cc) is a plain left-to-right scan
// where a later occurrence of the same flag overwrites the earlier one,
// so any of these the caller explicitly passes still wins.
int main(int argc, char** argv) {
    const std::string repo_root = QP_REPO_ROOT;
    // Tracked sibling of BENCHMARKS.md, same convention: both regenerated
    // and committed together, so HEAD's copy is always "the current
    // commit's numbers" — the baseline bench_diff.py below compares
    // against, with no separate baseline-tracking file to go stale.
    const std::string json_path = repo_root + "/bench_results.json";
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

    const std::string md_path = repo_root + "/BENCHMARKS.md";
    const std::string script  = repo_root + "/tools/bench/bench_to_md.py";

    // Best-effort baseline: HEAD's committed bench_results.json (the top
    // commit, never the working-tree copy this run just overwrote), so the
    // diff answers "what would committing this change do to perf." If it's
    // absent, the summary is still written, just without a diff.
    const std::string baseline_path = repo_root + "/build/bench_baseline_head.json";
    const std::string git_cmd = "git -C \"" + repo_root + "\" show HEAD:bench_results.json > \"" +
                                baseline_path + "\" 2>/dev/null";
    bool have_baseline = std::system(git_cmd.c_str()) == 0;

    std::string cmd = "python3 \"" + script + "\" \"" + json_path + "\" -o \"" + md_path + "\"";
    if (have_baseline) cmd += " --baseline \"" + baseline_path + "\"";

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::ofstream out(md_path, std::ios::trunc);
        out << "# Benchmarks\n\nbench_to_md.py failed (exit " << rc << ") — see stderr.\n";
        std::fprintf(stderr, "warning: bench_to_md.py failed (exit %d) — wrote error to %s\n", rc,
                     md_path.c_str());
    } else {
        std::fprintf(stderr, "wrote %s\n", md_path.c_str());
    }
    return 0;
}

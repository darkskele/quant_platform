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
// BENCHMARKS.md as a diff against whatever bench_results.json is
// committed at HEAD — the intended workflow is running this right before
// a commit to see what your uncommitted changes did to perf.
// bench_results.json is the source of truth for absolute numbers;
// BENCHMARKS.md only ever holds a diff (or an error explaining why it
// couldn't produce one), never a raw table — see tools/bench/bench_diff.py.
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

    // Errors go into BENCHMARKS.md itself, not just stderr — opening the
    // file should never show stale content silently, since the tracked
    // copy is what a reviewer/hook actually looks at.
    auto write_error_md = [&](const std::string& reason) {
        std::ofstream out(md_path, std::ios::trunc);
        out << "## Benchmark diff (current vs. HEAD)\n\n" << reason << "\n";
    };

    // Diff is always against HEAD's committed bench_results.json — the
    // top commit, never the working-tree copy this run just overwrote —
    // so it answers "what would committing this change do to perf."
    const std::string baseline_path = repo_root + "/build/bench_baseline_head.json";
    const std::string git_cmd = "git -C \"" + repo_root + "\" show HEAD:bench_results.json > \"" +
                                baseline_path + "\" 2>/dev/null";
    if (std::system(git_cmd.c_str()) != 0) {
        write_error_md(
            "No baseline: `git show HEAD:bench_results.json` failed — either this repo has no "
            "commits, or `bench_results.json` isn't committed at HEAD yet. Commit this run's "
            "`bench_results.json` to establish one.");
        std::fprintf(stderr, "no committed bench_results.json at HEAD — wrote error to %s\n",
                     md_path.c_str());
        return 0;
    }

    const std::string diff_script = repo_root + "/tools/bench/bench_diff.py";
    const std::string diff_cmd    = "python3 \"" + diff_script + "\" \"" + baseline_path + "\" \"" +
                                 json_path + "\" -o \"" + md_path + "\"";
    int diff_rc = std::system(diff_cmd.c_str());
    if (diff_rc != 0) {
        write_error_md("`bench_diff.py` failed (exit " + std::to_string(diff_rc) +
                       ") — see stderr for details.");
        std::fprintf(stderr, "warning: bench_diff.py failed (exit %d) — wrote error to %s\n",
                     diff_rc, md_path.c_str());
    } else {
        std::fprintf(stderr, "wrote %s\n", md_path.c_str());
    }
    return 0;
}

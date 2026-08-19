#pragma once
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace qp::test {

/// RAII scratch directory under the system temp path — unique per instance,
/// removed on destruction. Every test/benchmark in this repo that touches
/// real files needs exactly this; extracted here once duplicated a third
/// time (sink, source, collector, root parity tests, the file-replay
/// bench) — the same struct, byte-for-byte, five times over.
struct ScratchDir {
    std::filesystem::path path;

    explicit ScratchDir(std::string_view prefix = "qp_test_") {
        std::random_device rd;
        path = std::filesystem::temp_directory_path() /
               std::filesystem::path(std::string(prefix) + std::to_string(rd()));
        std::filesystem::create_directories(path);
    }

    ~ScratchDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

}  // namespace qp::test

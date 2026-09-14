#include <gtest/gtest.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row_reader.hpp"
#include "types.hpp"

using qp::SymbolId;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::read_cost_rows_csv;

namespace {

// Test-only symbol table with three symbols. Matches the SymbolTable
// interface used by the loader: a static `id_of(string_view)` returning
// `optional<SymbolId>`.
struct TestSymbolTable {
    static std::optional<SymbolId> id_of(std::string_view name) noexcept {
        if (name == "AAA") return SymbolId{0};
        if (name == "BBB") return SymbolId{1};
        if (name == "CCC") return SymbolId{2};
        return std::nullopt;
    }
};

struct TempFile {
    std::filesystem::path path;

    explicit TempFile(std::string_view contents) {
        path = std::filesystem::temp_directory_path() /
               (std::string("qp_cost_csv_") + std::to_string(std::rand()) + ".csv");
        std::ofstream out{path};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

}  // namespace

TEST(CostRowReader, ParsesTypicalFinalizeOutput) {
    // Column order taken from research/build_cost_table.ipynb's finalize().
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,half_spread_source,"
        "impact_bps_per_unit,taker_fee_bps,n_days_book,n_days_trade,n_bars\n"
        "AAA,1,2023-05-15,0.25,book,0.001,4.0,7,7,10080\n"
        "AAA,1,2023-05-22,0.30,book,0.002,4.0,7,7,10080\n"
        "BBB,1,2023-05-15,0.75,ar,,4.0,0,0,10080\n";  // impact empty -> treated as 0
    TempFile f{kCsv};

    auto rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 3u);

    // First row: AAA (id 0), market 1, 2023-05-15.
    EXPECT_EQ(rows[0].symbol, 0u);
    EXPECT_EQ(rows[0].market, 1);
    EXPECT_DOUBLE_EQ(rows[0].half_spread_bps, 0.25);
    EXPECT_DOUBLE_EQ(rows[0].impact_bps_per_unit, 0.001);
    EXPECT_DOUBLE_EQ(rows[0].taker_fee_bps, 4.0);
    // week_start_ns is 2023-05-15 00:00:00 UTC in ns. Sanity check it's
    // positive and later than the previous week.
    EXPECT_GT(rows[0].week_start_ns, 0);
    EXPECT_LT(rows[0].week_start_ns, rows[1].week_start_ns);

    // Row with empty impact_bps_per_unit resolves to 0.
    EXPECT_DOUBLE_EQ(rows[2].impact_bps_per_unit, 0.0);
    EXPECT_EQ(rows[2].symbol, 1u);
}

TEST(CostRowReader, DropsUnknownSymbolRows) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,0.25,0.001,4.0\n"
        "ZZZ,1,2023-05-15,9.99,0.001,4.0\n";  // ZZZ unknown, silently skipped
    TempFile f{kCsv};

    auto rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].symbol, 0u);
}

TEST(CostRowReader, ThrowsOnMissingHeaderColumn) {
    // Missing taker_fee_bps.
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit\n"
        "AAA,1,2023-05-15,0.25,0.001\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, ThrowsOnEmptyRequiredValue) {
    // half_spread_bps empty for a real row is a bad row.
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,,0.001,4.0\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, ThrowsOnBadDate) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-13-40,0.25,0.001,4.0\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, ThrowsOnMissingFile) {
    std::filesystem::path bogus =
        std::filesystem::temp_directory_path() / "qp_cost_does_not_exist.csv";
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(bogus), std::runtime_error);
}

TEST(CostRowReader, ThrowsOnNonNumericHalfSpread) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,notanumber,0.001,4.0\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, ThrowsOnNonNumericFee) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,0.25,0.001,bogus\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, ThrowsOnBadVenueInteger) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,abc,2023-05-15,0.25,0.001,4.0\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, HandlesCrlfLineEndings) {
    // Windows line endings should be stripped, not carried into fields.
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\r\n"
        "AAA,1,2023-05-15,0.25,0.001,4.0\r\n";
    TempFile f{kCsv};
    auto     rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_DOUBLE_EQ(rows[0].half_spread_bps, 0.25);
}

TEST(CostRowReader, HandlesBlankLines) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,0.25,0.001,4.0\n"
        "\n"
        "BBB,1,2023-05-15,0.30,0.002,4.0\n";
    TempFile f{kCsv};
    auto     rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 2u);
}

TEST(CostRowReader, ParsesMultipleSymbolsInterleaved) {
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,0.25,0.001,4.0\n"
        "BBB,1,2023-05-15,0.75,0.002,4.0\n"
        "AAA,1,2023-05-22,0.30,0.001,4.0\n"
        "CCC,1,2023-05-15,1.50,0.003,4.0\n";
    TempFile f{kCsv};
    auto     rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 4u);
    std::size_t seen_a = 0, seen_b = 0, seen_c = 0;
    for (const auto& r : rows) {
        if (r.symbol == 0) ++seen_a;
        if (r.symbol == 1) ++seen_b;
        if (r.symbol == 2) ++seen_c;
    }
    EXPECT_EQ(seen_a, 2u);
    EXPECT_EQ(seen_b, 1u);
    EXPECT_EQ(seen_c, 1u);
}

TEST(CostRowReader, AcceptsHeaderInAnyColumnOrder) {
    // The loader indexes columns by name, so reordering the header must
    // not affect row parsing.
    constexpr std::string_view kCsv =
        "taker_fee_bps,impact_bps_per_unit,week_start,market,symbol,half_spread_bps\n"
        "4.0,0.001,2023-05-15,1,AAA,0.25\n";
    TempFile f{kCsv};
    auto     rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].symbol, 0u);
    EXPECT_EQ(rows[0].market, 1);
    EXPECT_DOUBLE_EQ(rows[0].half_spread_bps, 0.25);
    EXPECT_DOUBLE_EQ(rows[0].impact_bps_per_unit, 0.001);
    EXPECT_DOUBLE_EQ(rows[0].taker_fee_bps, 4.0);
}

TEST(CostRowReader, ThrowsOnNonNumericImpact) {
    // Empty impact is legal (treated as 0). A non-empty garbage value is not.
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps\n"
        "AAA,1,2023-05-15,0.25,notanumber,4.0\n";
    TempFile f{kCsv};
    EXPECT_THROW(read_cost_rows_csv<TestSymbolTable>(f.path), std::runtime_error);
}

TEST(CostRowReader, IgnoresExtraColumns) {
    // The finalize step writes several extra bookkeeping columns after the
    // required ones (n_days_book, n_days_trade, n_bars, half_spread_source).
    // The loader must ignore them cleanly.
    constexpr std::string_view kCsv =
        "symbol,market,week_start,half_spread_bps,half_spread_source,"
        "impact_bps_per_unit,taker_fee_bps,n_days_book,n_days_trade,n_bars,extra\n"
        "AAA,1,2023-05-15,0.25,book,0.001,4.0,7,7,10080,ignored\n";
    TempFile f{kCsv};
    auto     rows = read_cost_rows_csv<TestSymbolTable>(f.path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_DOUBLE_EQ(rows[0].half_spread_bps, 0.25);
}

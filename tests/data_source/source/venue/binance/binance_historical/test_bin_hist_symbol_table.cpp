#include <gtest/gtest.h>

#include <string>

#include "bin_hist_symbol_table.hpp"
#include "venue.hpp"

using qp::data_source::source::venue::binance::binance_historical::BinHistSymbolTable;

static_assert(qp::data_source::source::venue::SymbolTable<BinHistSymbolTable>);

// Compile-time: id<->name resolution needs no runtime state at all, unlike
// the old intern-as-you-go venue_types.hpp::SymbolTable — these are plain
// static_asserts, not gtest cases, precisely because that's now possible.
static_assert(BinHistSymbolTable::id_of("BTCUSDT") == 0u);
static_assert(BinHistSymbolTable::id_of("ETHUSDT") == 1u);
static_assert(BinHistSymbolTable::id_of("btcusdt") == 0u);  // case-insensitive
static_assert(!BinHistSymbolTable::id_of("NOTASYMBOL").has_value());
static_assert(BinHistSymbolTable::name_of(0) == "BTCUSDT");
static_assert(BinHistSymbolTable::name_of(1) == "ETHUSDT");
static_assert(BinHistSymbolTable::kSymbols.size() == 10);

// Length edge cases for id_of's fixed-size uppercase buffer
// (kMaxSymbolLen, sized off the longest real symbol at compile time) —
// none of these should ever read/write past that buffer, and all should
// resolve to nullopt since none are real symbols.
static_assert(!BinHistSymbolTable::id_of("").has_value());          // empty
static_assert(!BinHistSymbolTable::id_of("BTCUSD").has_value());    // real symbol, truncated
static_assert(!BinHistSymbolTable::id_of("BTCUSDTX").has_value());  // real symbol + 1 char
static_assert(!BinHistSymbolTable::id_of("ZZZUSDT").has_value());   // same length as 6 real entries
static_assert(!BinHistSymbolTable::id_of("THISSTRINGISFARLONGERTHANANYREALBINANCESYMBOLCOULDEVERBE")
                   .has_value());  // longer than kMaxSymbolLen — exercises the early bound check

TEST(BinHistSymbolTable, IdOfResolvesKnownSymbols) {
    EXPECT_EQ(BinHistSymbolTable::id_of("BTCUSDT"), 0u);
    EXPECT_EQ(BinHistSymbolTable::id_of("ETHUSDT"), 1u);
}

TEST(BinHistSymbolTable, IdOfIsCaseInsensitive) {
    EXPECT_EQ(BinHistSymbolTable::id_of("btcusdt"), BinHistSymbolTable::id_of("BTCUSDT"));
    EXPECT_EQ(BinHistSymbolTable::id_of("BtcUsdt"), BinHistSymbolTable::id_of("BTCUSDT"));
}

TEST(BinHistSymbolTable, IdOfReturnsNulloptForAnUnknownSymbol) {
    EXPECT_FALSE(BinHistSymbolTable::id_of("NOTASYMBOL").has_value());
}

TEST(BinHistSymbolTable, IdOfReturnsNulloptForAnEmptyString) {
    EXPECT_FALSE(BinHistSymbolTable::id_of("").has_value());
}

TEST(BinHistSymbolTable, IdOfReturnsNulloptForATruncatedRealSymbol) {
    EXPECT_FALSE(BinHistSymbolTable::id_of("BTCUSD").has_value());
}

TEST(BinHistSymbolTable, IdOfReturnsNulloptForARealSymbolPlusOneChar) {
    EXPECT_FALSE(BinHistSymbolTable::id_of("BTCUSDTX").has_value());
}

TEST(BinHistSymbolTable, IdOfReturnsNulloptForASameLengthImpostor) {
    // "ZZZUSDT" is 7 chars, same as 6 of the 10 real entries — forces a
    // genuine string comparison against every one of those before
    // failing, unlike a length that collides with nothing (see
    // bench_bin_hist_symbol_table.cpp's comment on why that matters for
    // benchmarking, though correctness doesn't depend on which path runs).
    EXPECT_FALSE(BinHistSymbolTable::id_of("ZZZUSDT").has_value());
}

TEST(BinHistSymbolTable, IdOfRejectsInputLongerThanTheLongestRealSymbolWithoutCrashing) {
    std::string garbage(200, 'X');
    EXPECT_FALSE(BinHistSymbolTable::id_of(garbage).has_value());
}

TEST(BinHistSymbolTable, NameOfRoundTripsWithIdOf) {
    for (auto name : BinHistSymbolTable::kSymbols) {
        auto id = BinHistSymbolTable::id_of(name);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(BinHistSymbolTable::name_of(*id), name);
    }
}

// The property that used to need a runtime cross-check across two
// independently-read manifests (backtest.cpp's resolve_symbol_id, before
// this restructure): two call sites resolving the same name always agree,
// because there's exactly one declared list, not two independently-built
// runtime tables that could drift apart.
TEST(BinHistSymbolTable, TwoIndependentLookupsOfTheSameNameAgree) {
    auto futures_leg_id = BinHistSymbolTable::id_of("BTCUSDT");
    auto spot_leg_id    = BinHistSymbolTable::id_of("BTCUSDT");
    ASSERT_TRUE(futures_leg_id.has_value());
    ASSERT_TRUE(spot_leg_id.has_value());
    EXPECT_EQ(*futures_leg_id, *spot_leg_id);
}

#include <gtest/gtest.h>

#include <string_view>

#include "agg_trades.hpp"
#include "endpoints.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::BinanceMarket;
using binance::endpoint;
using binance::EndpointKind;
using binance::parsers::parse_agg_trades_row;
using qp::Side;
using qp::Timestamp;
using qp::TradeEvent;

namespace {

const binance::Endpoint& spot() { return endpoint(BinanceMarket::Spot, EndpointKind::AggTrades); }

const binance::Endpoint& usdm() { return endpoint(BinanceMarket::UsdM, EndpointKind::AggTrades); }

const binance::Endpoint& coinm() { return endpoint(BinanceMarket::CoinM, EndpointKind::AggTrades); }

// Real rows off the bucket, one per shape the dataset ships in.
constexpr std::string_view kUsdMRow =
    "2742797403,105583.3,0.01,6362492256,6362492256,1748822400011,false";
constexpr std::string_view kSpotMicrosRow =
    "3587770505,105642.93000000,0.00047000,4976176577,4976176577,1748822400054776,False,True";
constexpr std::string_view kSpotMillisRow =
    "3024457265,67766.84000000,0.00738000,3621628241,3621628241,1717286400355,True,True";
constexpr std::string_view kCoinMRow =
    "412265638,105652.0,158.0,974109455,974109455,1748822400415,false";

}  // namespace

// The stamp is the sixth field, so a parser reading the first would take the
// trade id for a time.
TEST(BinanceAggTrades, ParsesUsdMRowStampFromSixthField) {
    Timestamp  ts{};
    TradeEvent out{};
    ASSERT_TRUE(parse_agg_trades_row(usdm(), kUsdMRow, ts, out));
    EXPECT_EQ(ts, 1748822400011LL * 1'000'000);
    EXPECT_DOUBLE_EQ(out.price, 105583.3);
    EXPECT_DOUBLE_EQ(out.qty, 0.01);
    EXPECT_EQ(out.side, Side::Buy);
    EXPECT_EQ(out.id, 2742797403u);
    EXPECT_EQ(out.first_trade_id, 6362492256u);
    EXPECT_EQ(out.last_trade_id, 6362492256u);
}

// One aggregate can fold several underlying fills, and the ids say how many.
TEST(BinanceAggTrades, CarriesTheUnderlyingFillRange) {
    constexpr std::string_view row =
        "3587770507,105642.92000000,0.89472000,4976176582,4976176601,1748822400306937,True,True";
    Timestamp  ts{};
    TradeEvent out{};
    ASSERT_TRUE(parse_agg_trades_row(spot(), row, ts, out));
    EXPECT_EQ(out.id, 3587770507u);
    EXPECT_EQ(out.last_trade_id - out.first_trade_id + 1, 20u);
}

TEST(BinanceAggTrades, SequenceIsTheAggregateId) {
    const TradeEvent trade{.id = 77, .first_trade_id = 1, .last_trade_id = 2};
    EXPECT_EQ(binance::parsers::AggTradesParser::sequence(trade), 77u);
}

// Spot switched to microseconds at 2025-01-01, futures did not.
TEST(BinanceAggTrades, SpotMicrosAndMillisBothLandInNanos) {
    Timestamp  ts{};
    TradeEvent out{};
    ASSERT_TRUE(parse_agg_trades_row(spot(), kSpotMicrosRow, ts, out));
    EXPECT_EQ(ts, 1748822400054776LL * 1'000);
    EXPECT_DOUBLE_EQ(out.qty, 0.00047);

    ASSERT_TRUE(parse_agg_trades_row(spot(), kSpotMillisRow, ts, out));
    EXPECT_EQ(ts, 1717286400355LL * 1'000'000);
    EXPECT_DOUBLE_EQ(out.price, 67766.84);
}

// A buyer who made the market means the taker sold.
TEST(BinanceAggTrades, BuyerMakerIsATakerSell) {
    Timestamp  ts{};
    TradeEvent out{};
    ASSERT_TRUE(parse_agg_trades_row(spot(), kSpotMillisRow, ts, out));
    EXPECT_EQ(out.side, Side::Sell);
    ASSERT_TRUE(parse_agg_trades_row(spot(), kSpotMicrosRow, ts, out));
    EXPECT_EQ(out.side, Side::Buy);
}

TEST(BinanceAggTrades, CoinMQuantityIsContracts) {
    Timestamp  ts{};
    TradeEvent out{};
    ASSERT_TRUE(parse_agg_trades_row(coinm(), kCoinMRow, ts, out));
    EXPECT_DOUBLE_EQ(out.qty, 158.0);
}

TEST(BinanceAggTrades, BooleansInEitherCase) {
    Timestamp  ts{};
    TradeEvent out{};
    for (std::string_view row :
         {"1,10.0,1.0,1,1,1748822400011,true", "1,10.0,1.0,1,1,1748822400011,True"}) {
        ASSERT_TRUE(parse_agg_trades_row(usdm(), row, ts, out)) << row;
        EXPECT_EQ(out.side, Side::Sell) << row;
    }
    for (std::string_view row :
         {"1,10.0,1.0,1,1,1748822400011,false", "1,10.0,1.0,1,1,1748822400011,False"}) {
        ASSERT_TRUE(parse_agg_trades_row(usdm(), row, ts, out)) << row;
        EXPECT_EQ(out.side, Side::Buy) << row;
    }
}

TEST(BinanceAggTrades, RejectsOtherBooleanSpellings) {
    Timestamp  ts{};
    TradeEvent out{};
    for (std::string_view row :
         {"1,10.0,1.0,1,1,1748822400011,TRUE", "1,10.0,1.0,1,1,1748822400011,1",
          "1,10.0,1.0,1,1,1748822400011,"}) {
        EXPECT_FALSE(parse_agg_trades_row(usdm(), row, ts, out)) << row;
    }
}

// The column count comes from the endpoint, so a row with the wrong width for
// its market is a bad row rather than a silently ignored tail.
TEST(BinanceAggTrades, ColumnCountFollowsTheMarket) {
    Timestamp  ts{};
    TradeEvent out{};
    EXPECT_FALSE(parse_agg_trades_row(usdm(), kSpotMicrosRow, ts, out))
        << "futures takes seven columns";
    EXPECT_FALSE(parse_agg_trades_row(spot(), kUsdMRow, ts, out)) << "spot takes eight";
}

TEST(BinanceAggTrades, RejectsHeaderAndBlankStamp) {
    Timestamp  ts{};
    TradeEvent out{};
    EXPECT_FALSE(parse_agg_trades_row(
        usdm(),
        "agg_trade_id,price,quantity,first_trade_id,last_trade_id,transact_time,is_buyer_maker", ts,
        out));
    EXPECT_FALSE(parse_agg_trades_row(usdm(), "1,10.0,1.0,1,1,,false", ts, out))
        << "a blank stamp must not parse as zero";
}

TEST(BinanceAggTrades, RejectsImpossibleTrades) {
    Timestamp  ts{};
    TradeEvent out{};
    EXPECT_FALSE(parse_agg_trades_row(usdm(), "1,0,1.0,1,1,1748822400011,false", ts, out));
    EXPECT_FALSE(parse_agg_trades_row(usdm(), "1,10.0,0,1,1,1748822400011,false", ts, out));
    EXPECT_FALSE(parse_agg_trades_row(usdm(), "1,10.0,1.0,5,4,1748822400011,false", ts, out))
        << "last trade id before the first";
    EXPECT_FALSE(parse_agg_trades_row(usdm(), "-1,10.0,1.0,1,1,1748822400011,false", ts, out))
        << "ids are never negative";
}

TEST(BinanceAggTrades, FailureLeavesOutputUntouched) {
    Timestamp  ts  = 42;
    TradeEvent out = {.price = 7.0};
    EXPECT_FALSE(parse_agg_trades_row(usdm(), "not,a,trade,row,at,all,x", ts, out));
    EXPECT_EQ(ts, 42);
    EXPECT_DOUBLE_EQ(out.price, 7.0);
}

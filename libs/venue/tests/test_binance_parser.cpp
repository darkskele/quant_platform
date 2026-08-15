#include "qp/venue/binance.hpp"

#include <gtest/gtest.h>

using namespace qp;
using namespace qp::venue::binance;

TEST(BinanceParser, DepthUpdate) {
    SymbolTable symbols;
    const char* msg = R"({"stream":"btcusdt@depth@100ms","data":{)"
        R"("e":"depthUpdate","E":1723660800123,"T":1723660800120,"s":"BTCUSDT",)"
        R"("U":157,"u":160,"pu":149,)"
        R"("b":[["61000.10","1.500"],["60999.90","0.020"]],)"
        R"("a":[["61000.20","2.300"]]}})";

    MarketEvent ev;
    ASSERT_TRUE(parse_message(msg, symbols, ev));
    EXPECT_EQ(ev.kind, EventKind::BookDiff);
    EXPECT_EQ(ev.first_seq, 157u);
    EXPECT_EQ(ev.seq, 160u);
    EXPECT_EQ(ev.prev_seq, 149u);
    EXPECT_EQ(ev.ts, 1723660800123LL * 1'000'000);
    EXPECT_EQ(symbols.name(ev.symbol), "BTCUSDT");
    ASSERT_EQ(ev.bids.size(), 2u);
    EXPECT_DOUBLE_EQ(ev.bids[0].price, 61000.10);
    EXPECT_DOUBLE_EQ(ev.bids[0].qty, 1.500);
    EXPECT_DOUBLE_EQ(ev.bids[1].price, 60999.90);
    ASSERT_EQ(ev.asks.size(), 1u);
    EXPECT_DOUBLE_EQ(ev.asks[0].price, 61000.20);
}

TEST(BinanceParser, AggTrade) {
    SymbolTable symbols;
    const char* msg = R"({"stream":"ethusdt@aggTrade","data":{)"
        R"("e":"aggTrade","E":1723660801000,"s":"ETHUSDT","a":5933014,)"
        R"("p":"2650.55","q":"3.200","f":100,"l":105,"T":1723660800987,"m":true}})";

    MarketEvent ev;
    ASSERT_TRUE(parse_message(msg, symbols, ev));
    EXPECT_EQ(ev.kind, EventKind::Trade);
    EXPECT_EQ(ev.seq, 5933014u);
    EXPECT_DOUBLE_EQ(ev.price, 2650.55);
    EXPECT_DOUBLE_EQ(ev.qty, 3.200);
    EXPECT_EQ(ev.side, Side::Sell);  // buyer maker -> taker sold
    EXPECT_EQ(symbols.name(ev.symbol), "ETHUSDT");
}

// Captured live from wss://fstream.binance.com on 2026-08-14 — real book
// content, not hand-crafted. Deliberately NOT reformatted: exercises fields
// our parser ignores by design (a real book has an "st" field and a "ps"
// pair-symbol field neither of ours reads) and a real zero-qty removal level
// ("1353.77" -> "0.000") without us having to fabricate one.
TEST(BinanceParser, DepthUpdateRealCapture) {
    SymbolTable symbols;
    const char* msg =
        R"({"stream":"ethusdt@depth@100ms","data":{"e":"depthUpdate","E":1786742884159,)"
        R"("T":1786742884158,"s":"ETHUSDT","ps":"ETHUSDT","U":11289273841677,)"
        R"("u":11289273848795,"pu":11289273841549,)"
        R"("b":[["200.00","57.006"],["1353.67","4.685"],["1353.77","0.000"],)"
        R"(["1373.67","4.133"],["1690.80","0.040"],["1690.81","3.261"],)"
        R"(["1778.67","0.032"],["1803.50","0.186"],["1849.55","14.613"],)"
        R"(["1875.11","0.982"],["1876.76","0.978"],["1877.74","12.794"],)"
        R"(["1877.87","44.287"],["1878.10","0.335"],["1878.42","20.618"],)"
        R"(["1878.49","4.477"]],)"
        R"("a":[["1879.10","0.035"],["1897.68","10.462"],["1897.72","34.050"],)"
        R"(["1909.41","0.550"],["1917.19","13.544"],["1978.68","0.173"],)"
        R"(["2866.35","0.001"],["2866.44","0.000"]],"st":1}})";

    MarketEvent ev;
    ASSERT_TRUE(parse_message(msg, symbols, ev));
    EXPECT_EQ(ev.kind, EventKind::BookDiff);
    EXPECT_EQ(symbols.name(ev.symbol), "ETHUSDT");
    EXPECT_EQ(ev.first_seq, 11289273841677u);
    EXPECT_EQ(ev.seq, 11289273848795u);
    EXPECT_EQ(ev.prev_seq, 11289273841549u);
    ASSERT_EQ(ev.bids.size(), 16u);
    ASSERT_EQ(ev.asks.size(), 8u);
    EXPECT_DOUBLE_EQ(ev.bids[0].price, 200.00);
    EXPECT_DOUBLE_EQ(ev.bids[2].price, 1353.77);
    EXPECT_DOUBLE_EQ(ev.bids[2].qty, 0.0);  // real removal, not a fabricated one
    EXPECT_DOUBLE_EQ(ev.asks[7].price, 2866.44);
}

// Trade values are real (pulled via REST /fapi/v1/aggTrades, same field
// content as the WS payload); the stream/"E" envelope is reconstructed
// because live WS aggTrade delivery wasn't reachable from this sandbox (the
// raw single-stream endpoint timed out too — a network-policy artifact here,
// not a Binance behavior; depth streams over the same connection work fine).
TEST(BinanceParser, AggTradeRealValues) {
    SymbolTable symbols;
    const char* msg = R"({"stream":"btcusdt@aggTrade","data":{)"
        R"("e":"aggTrade","E":1786742457242,"s":"BTCUSDT","a":3408644882,)"
        R"("p":"62859.00","q":"0.938","f":7972465064,"l":7972465066,)"
        R"("T":1786742457242,"m":false}})";

    MarketEvent ev;
    ASSERT_TRUE(parse_message(msg, symbols, ev));
    EXPECT_EQ(ev.kind, EventKind::Trade);
    EXPECT_EQ(ev.seq, 3408644882u);
    EXPECT_DOUBLE_EQ(ev.price, 62859.00);
    EXPECT_DOUBLE_EQ(ev.qty, 0.938);
    EXPECT_EQ(ev.side, Side::Buy);  // buyer maker == false -> taker bought
    EXPECT_EQ(symbols.name(ev.symbol), "BTCUSDT");
}

TEST(BinanceParser, SymbolInterningIsStableAndOrdered) {
    SymbolTable symbols;
    EXPECT_EQ(symbols.intern("BTCUSDT"), 0u);
    EXPECT_EQ(symbols.intern("ETHUSDT"), 1u);
    EXPECT_EQ(symbols.intern("SOLUSDT"), 2u);
    EXPECT_EQ(symbols.intern("BTCUSDT"), 0u);  // re-interning returns the same id
}

TEST(BinanceParser, RejectsSubscriptionAck) {
    SymbolTable symbols;
    MarketEvent ev;
    EXPECT_FALSE(parse_message(R"({"result":null,"id":1})", symbols, ev));
}

TEST(BinanceParser, RejectsMalformedJson) {
    SymbolTable symbols;
    MarketEvent ev;
    EXPECT_FALSE(parse_message(R"({not json at all)", symbols, ev));
}

TEST(BinanceParser, RejectsUnrecognizedEventType) {
    SymbolTable symbols;
    MarketEvent ev;
    const char* msg = R"({"stream":"btcusdt@markPrice","data":{"e":"markPriceUpdate","s":"BTCUSDT"}})";
    EXPECT_FALSE(parse_message(msg, symbols, ev));
}

TEST(BinanceParser, HandlesEmptySideDiff) {
    SymbolTable symbols;
    const char* msg = R"({"stream":"btcusdt@depth@100ms","data":{)"
        R"("e":"depthUpdate","E":1,"T":1,"s":"BTCUSDT","U":161,"u":161,"pu":160,)"
        R"("b":[],"a":[["61000.20","0.000"]]}})";

    MarketEvent ev;
    ASSERT_TRUE(parse_message(msg, symbols, ev));
    EXPECT_TRUE(ev.bids.empty());
    ASSERT_EQ(ev.asks.size(), 1u);
    EXPECT_DOUBLE_EQ(ev.asks[0].qty, 0.0);  // 0 qty = level removed, per MarketEvent's own contract
}

TEST(BinanceParser, BuildStreamPath) {
    EXPECT_EQ(build_stream_path({"BTCUSDT", "ethusdt"}),
              "/stream?streams=btcusdt@depth@100ms/btcusdt@aggTrade/"
              "ethusdt@depth@100ms/ethusdt@aggTrade");
    EXPECT_EQ(build_stream_path({"BTCUSDT"}, "250ms"),
              "/stream?streams=btcusdt@depth@250ms/btcusdt@aggTrade");
}

TEST(BinanceParser, BuildStreamUrlRespectsEndpoint) {
    EXPECT_EQ(build_stream_url({"BTCUSDT"}),
              "wss://fstream.binance.com/stream?streams=btcusdt@depth@100ms/btcusdt@aggTrade");
    EXPECT_EQ(build_stream_url({"BTCUSDT"}, kFuturesWsTestnet),
              "wss://stream.binancefuture.com/stream?streams=btcusdt@depth@100ms/btcusdt@aggTrade");
}

TEST(BinanceParser, DepthSnapshotUrlRespectsEndpoint) {
    EXPECT_EQ(depth_snapshot_url("btcusdt", 1000),
              "https://fapi.binance.com/fapi/v1/depth?symbol=BTCUSDT&limit=1000");
    EXPECT_EQ(depth_snapshot_url("btcusdt", 1000, kFuturesRestTestnet),
              "https://testnet.binancefuture.com/fapi/v1/depth?symbol=BTCUSDT&limit=1000");
}

// Real capture from GET /fapi/v1/depth?symbol=BTCUSDT&limit=5 on 2026-08-14.
TEST(BinanceParser, ParseDepthSnapshotRealCapture) {
    const char* body =
        R"({"lastUpdateId":11289247357348,"E":1786742451231,"T":1786742451224,)"
        R"("bids":[["62858.90","40.395"],["62858.80","0.859"],["62858.70","0.005"],)"
        R"(["62858.40","0.003"],["62858.10","0.006"]],)"
        R"("asks":[["62859.00","2.613"],["62859.10","0.004"],["62859.20","0.001"],)"
        R"(["62859.30","0.084"],["62859.50","0.005"]]})";

    auto snapshot = parse_depth_snapshot(body);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->last_update_id, 11289247357348u);
    ASSERT_EQ(snapshot->bids.size(), 5u);
    ASSERT_EQ(snapshot->asks.size(), 5u);
    EXPECT_DOUBLE_EQ(snapshot->bids[0].price, 62858.90);
    EXPECT_DOUBLE_EQ(snapshot->bids[0].qty, 40.395);
    EXPECT_DOUBLE_EQ(snapshot->asks[4].price, 62859.50);
}

TEST(BinanceParser, ParseDepthSnapshotRejectsMalformed) {
    EXPECT_FALSE(parse_depth_snapshot(R"({not json)").has_value());
    EXPECT_FALSE(parse_depth_snapshot(R"({"bids":[],"asks":[]})").has_value());  // missing lastUpdateId
}

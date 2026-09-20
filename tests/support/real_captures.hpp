#pragma once

// Real observed Binance USD-M futures payloads (same captures used in
// libs/data_source/source/exchange/tests/test_binance_parser.cpp) — genuine price levels,
// quantities, and level counts, not synthetic placeholder data. Sequence
// numbers (U/u/pu/lastUpdateId) are parameters here rather than the
// originally-captured values: the snapshot and the depth diff were captured
// independently (for parser-only unit tests), not as a matched resync pair,
// so a test that needs them to actually *align* has to choose numbers that
// do — the payload content itself is still real, only the sequencing is
// test-controlled, same reasoning test_live_websocket_source_integration.cpp's
// make_depth_msg/make_snapshot_body already established.

#include <cstdint>
#include <string>
#include <string_view>

namespace qp::testsupport {

// A real ETHUSDT depth-diff payload: 16 bids, 8 asks.
inline std::string real_depth_msg(std::string_view symbol, std::uint64_t first_seq,
                                  std::uint64_t seq, std::uint64_t prev_seq) {
    return std::string(R"({"stream":"test@depth","data":{"e":"depthUpdate","E":1,"T":1,"s":")") +
           std::string(symbol) + R"(","U":)" + std::to_string(first_seq) + R"(,"u":)" +
           std::to_string(seq) + R"(,"pu":)" + std::to_string(prev_seq) + R"(,)" +
           R"("b":[["200.00","57.006"],["1353.67","4.685"],["1353.77","0.000"],)"
           R"(["1373.67","4.133"],["1690.80","0.040"],["1690.81","3.261"],)"
           R"(["1778.67","0.032"],["1803.50","0.186"],["1849.55","14.613"],)"
           R"(["1875.11","0.982"],["1876.76","0.978"],["1877.74","12.794"],)"
           R"(["1877.87","44.287"],["1878.10","0.335"],["1878.42","20.618"],)"
           R"(["1878.49","4.477"]],)"
           R"("a":[["1879.10","0.035"],["1897.68","10.462"],["1897.72","34.050"],)"
           R"(["1909.41","0.550"],["1917.19","13.544"],["1978.68","0.173"],)"
           R"(["2866.35","0.001"],["2866.44","0.000"]]}})";
}

// A real BTCUSDT depth-snapshot payload: 5 bids, 5 asks.
inline std::string real_snapshot_body(std::uint64_t last_update_id) {
    return std::string(R"({"lastUpdateId":)") + std::to_string(last_update_id) +
           R"(,"E":1,"T":1,)"
           R"("bids":[["62858.90","40.395"],["62858.80","0.859"],["62858.70","0.005"],)"
           R"(["62858.40","0.003"],["62858.10","0.006"]],)"
           R"("asks":[["62859.00","2.613"],["62859.10","0.004"],["62859.20","0.001"],)"
           R"(["62859.30","0.084"],["62859.50","0.005"]]})";
}

}  // namespace qp::testsupport

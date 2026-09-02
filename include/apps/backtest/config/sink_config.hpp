#pragma once
#include <cstddef>

// QP_SINK_* picks which Sink this binary is compiled against,
// independent of QP_SOURCE_* (same csv+binhist Source can feed a fanout
// ring today, an IPC sink later, without either side knowing the other
// changed). backtest.cpp only ever names config::LegSink<Capacity,N>.
#if defined(QP_SINK_FANOUT)
#include "fanout_sink.hpp"

namespace qp::backtest::config {
template <std::size_t Capacity, std::size_t NumConsumers>
using LegSink = data_source::sink::fanout::FanoutSink<Capacity, NumConsumers>;
}  // namespace qp::backtest::config
#else
#error "define exactly one QP_SINK_* macro (see cmake/apps/backtest/CMakeLists.txt)"
#endif

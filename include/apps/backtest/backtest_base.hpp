#pragma once
#include <chrono>
#include <cstddef>
#include <exception>
#include <thread>

#include "control_channel.hpp"
#include "run_data_source.hpp"

namespace qp::backtest {

/// Shared backtest lifecycle, CRTP over the concrete variant. Owns only
/// the thread/control choreography every backtest runs identically:
///   sources()     -> the source tuple to feed (references are fine)
///   sinks()       -> the sink tuple, positionally paired with sources()
///   make_engine() -> the Engine
///   results()     -> whatever this variant reports once the run is over
/// run() launches the source pump and the engine on their own threads,
/// waits for the sources to run dry, then tells the engine to stop.
template <class Derived>
class BacktestBase {
   public:
    auto run() {
        Derived& self   = static_cast<Derived&>(*this);
        auto     engine = self.make_engine();

        // Named locals
        auto  srcs  = self.sources();
        auto& sinks = self.sinks();

        // Two participants: the engine's run loop, and the source pump.
        ControlChannel<2> control;
        std::size_t       source_consumer = control.attach();
        std::size_t       engine_consumer = control.attach();

        // An exception escaping a std::thread body would terminate the
        // process, so each thread stows its error and this one rethrows it
        // after the joins. On a source error, stop so the engine cannot hang
        // waiting for data that will never come.
        std::exception_ptr source_error, engine_error;

        // Backtest replay: yield instead of sleeping when a thread is briefly
        // starved, so the run is bounded by throughput, not wall-clock backoff.
        std::thread source_thread([&] {
            try {
                data_source::run_data_source(srcs, sinks, control, source_consumer,
                                             std::chrono::milliseconds::zero());
            } catch (...) {
                source_error = std::current_exception();
                control.request_stop();
            }
        });
        std::thread engine_thread([&] {
            try {
                engine.run(control, engine_consumer, std::chrono::microseconds::zero());
            } catch (...) {
                engine_error = std::current_exception();
            }
        });

        // Sources drained (all Eof) -> ask the engine to stop; it flushes
        // and drains whatever's still buffered before its run() returns.
        source_thread.join();
        control.request_stop();
        engine_thread.join();

        if (source_error) std::rethrow_exception(source_error);
        if (engine_error) std::rethrow_exception(engine_error);
        return self.results();
    }
};

}  // namespace qp::backtest

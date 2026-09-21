#include "binance_historical_bindings.hpp"

#include <pybind11/stl.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "binance_historical_source.hpp"
#include "endpoints.hpp"
#include "http_fetch_pool.hpp"
#include "source.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace py = pybind11;

namespace qp::python {

namespace {

namespace binance = data_source::source::exchange::binance;
using data_source::source::SourceStatus;

/// The source with the subscription it resolved against kept alive beside it,
/// and a pull that waits out fetches rather than reporting them as empty.
class PythonBinanceHistoricalSource {
   public:
    PythonBinanceHistoricalSource(Subscription                     subscription,
                                  binance::BinanceHistoricalConfig config)
        : subscription_(std::move(subscription)), source_(std::move(config), subscription_) {}

    void plan() { source_.plan(); }

    /// The next event in timestamp order, or none once every stream is done.
    /// Waits while a fetch is still landing, since NoData is not the end.
    std::optional<MarketEvent> next() {
        for (;;) {
            auto pulled = source_.next();
            if (pulled) return std::move(*pulled);
            if (pulled.error() == SourceStatus::Eof) return std::nullopt;
            std::this_thread::sleep_for(kIdle);
        }
    }

    std::size_t stream_count() const noexcept { return source_.stream_count(); }

    std::vector<binance::StreamReport> reports() const { return source_.reports(); }

    binance::FetchPoolStats fetch_stats() const { return source_.pool().stats(); }

    std::vector<binance::FetchFailure> fetch_failures() const {
        return source_.pool().recent_failures();
    }

   private:
    static constexpr std::chrono::microseconds kIdle{200};

    Subscription                                             subscription_;
    binance::BinanceHistoricalSource<binance::HttpFetchPool> source_;
};

}  // namespace

void bind_binance_historical(py::module_& m) {
    py::enum_<binance::EndpointKind>(m, "EndpointKind")
        .value("Klines", binance::EndpointKind::Klines)
        .value("MarkPriceKlines", binance::EndpointKind::MarkPriceKlines)
        .value("PremiumIndexKlines", binance::EndpointKind::PremiumIndexKlines)
        .value("FundingRate", binance::EndpointKind::FundingRate)
        .value("Metrics", binance::EndpointKind::Metrics)
        .value("BookDepth", binance::EndpointKind::BookDepth)
        .value("AggTrades", binance::EndpointKind::AggTrades);

    py::enum_<binance::Cadence>(m, "Cadence")
        .value("Daily", binance::Cadence::Daily)
        .value("Monthly", binance::Cadence::Monthly);

    py::enum_<binance::BinanceMarket>(m, "BinanceMarket")
        .value("Spot", binance::BinanceMarket::Spot)
        .value("UsdM", binance::BinanceMarket::UsdM)
        .value("CoinM", binance::BinanceMarket::CoinM);

    py::class_<binance::StreamSpec>(m, "StreamSpec")
        .def(py::init([](binance::EndpointKind kind, std::string interval) {
                 return binance::StreamSpec{kind, std::move(interval)};
             }),
             py::arg("kind"), py::arg("interval") = std::string{})
        .def_readwrite("kind", &binance::StreamSpec::kind)
        .def_readwrite("interval", &binance::StreamSpec::interval);

    py::class_<binance::GapStats>(m, "GapStats")
        .def_readonly("files_planned", &binance::GapStats::files_planned)
        .def_readonly("files_read", &binance::GapStats::files_read)
        .def_readonly("files_failed", &binance::GapStats::files_failed)
        .def_readonly("header_rows", &binance::GapStats::header_rows)
        .def_readonly("blank_rows", &binance::GapStats::blank_rows)
        .def_readonly("rows_expected", &binance::GapStats::rows_expected)
        .def_readonly("rows_parsed", &binance::GapStats::rows_parsed)
        .def_readonly("rows_rejected", &binance::GapStats::rows_rejected)
        .def_readonly("backwards_stamps", &binance::GapStats::backwards_stamps)
        .def_readonly("repeated_stamps", &binance::GapStats::repeated_stamps)
        .def_readonly("sequence_gaps", &binance::GapStats::sequence_gaps);

    py::class_<binance::StreamReport>(m, "StreamReport")
        .def_readonly("market", &binance::StreamReport::market)
        .def_readonly("symbol", &binance::StreamReport::symbol)
        .def_readonly("kind", &binance::StreamReport::kind)
        .def_readonly("interval", &binance::StreamReport::interval)
        .def_readonly("stats", &binance::StreamReport::stats);

    py::class_<binance::FetchPoolStats>(m, "FetchPoolStats")
        .def_readonly("submitted", &binance::FetchPoolStats::submitted)
        .def_readonly("refused", &binance::FetchPoolStats::refused)
        .def_readonly("completed_ok", &binance::FetchPoolStats::completed_ok)
        .def_readonly("completed_failed", &binance::FetchPoolStats::completed_failed)
        .def_readonly("retries", &binance::FetchPoolStats::retries)
        .def_readonly("not_found", &binance::FetchPoolStats::not_found)
        .def_readonly("server_error", &binance::FetchPoolStats::server_error)
        .def_readonly("transport_error", &binance::FetchPoolStats::transport_error)
        .def_readonly("zip_error", &binance::FetchPoolStats::zip_error)
        .def_readonly("cancelled", &binance::FetchPoolStats::cancelled)
        .def_readonly("cancellations_dropped", &binance::FetchPoolStats::cancellations_dropped)
        .def_readonly("queued", &binance::FetchPoolStats::queued)
        .def_readonly("in_flight", &binance::FetchPoolStats::in_flight)
        .def_readonly("workers_alive", &binance::FetchPoolStats::workers_alive)
        .def_readonly("oldest_in_flight_ms", &binance::FetchPoolStats::oldest_in_flight_ms)
        .def_readonly("critical", &binance::FetchPoolStats::critical)
        .def_readonly("bytes_fetched", &binance::FetchPoolStats::bytes_fetched)
        .def_readonly("bytes_inflated", &binance::FetchPoolStats::bytes_inflated);

    py::class_<binance::FetchFailure>(m, "FetchFailure")
        .def_readonly("url", &binance::FetchFailure::url)
        .def_readonly("detail", &binance::FetchFailure::detail)
        .def_readonly("status", &binance::FetchFailure::status)
        .def_readonly("attempts", &binance::FetchFailure::attempts);

    py::enum_<binance::FetchStatus>(m, "FetchStatus")
        .value("Ok", binance::FetchStatus::Ok)
        .value("NotFound", binance::FetchStatus::NotFound)
        .value("ServerError", binance::FetchStatus::ServerError)
        .value("TransportError", binance::FetchStatus::TransportError)
        .value("ZipError", binance::FetchStatus::ZipError)
        .value("Cancelled", binance::FetchStatus::Cancelled);

    py::class_<binance::HttpFetchPoolConfig>(m, "FetchPoolConfig")
        .def(py::init<>())
        .def_readwrite("workers", &binance::HttpFetchPoolConfig::workers)
        .def_readwrite("max_retries", &binance::HttpFetchPoolConfig::max_retries)
        .def_readwrite("critical_failure_ratio",
                       &binance::HttpFetchPoolConfig::critical_failure_ratio)
        .def_readwrite("critical_window", &binance::HttpFetchPoolConfig::critical_window);

    using Config = binance::BinanceHistoricalConfig;
    py::class_<Config>(m, "BinanceHistoricalConfig")
        .def(py::init([](std::vector<binance::StreamSpec> streams, binance::Cadence cadence,
                         Timestamp from, Timestamp to, binance::HttpFetchPoolConfig pool) {
                 return Config{std::move(streams), pool, cadence, from, to};
             }),
             py::arg("streams"), py::arg("cadence") = binance::Cadence::Monthly,
             py::arg("from_ns") = 0, py::arg("to_ns") = 0,
             py::arg("pool") = binance::HttpFetchPoolConfig{})
        .def_readwrite("prefetch_depth", &Config::prefetch_depth);

    // The ceiling a prefetch depth is clamped to.
    m.attr("FILE_SLOTS") = binance::kFileSlotCount;

    using Source = PythonBinanceHistoricalSource;
    py::class_<Source>(m, "BinanceHistoricalSource")
        .def(py::init([](Subscription subscription, Config config) {
                 // Built in place, since its fetch workers hold the source's
                 // queues by address.
                 return std::make_unique<Source>(std::move(subscription), std::move(config));
             }),
             py::arg("subscription"), py::arg("config"))
        .def("plan", &Source::plan, py::call_guard<py::gil_scoped_release>())
        // Waits on the network, so the GIL is released while it does.
        .def("next", &Source::next, py::call_guard<py::gil_scoped_release>())
        .def(
            "__iter__", [](Source& self) -> Source& { return self; },
            py::return_value_policy::reference_internal)
        .def("__next__",
             [](Source& self) {
                 std::optional<MarketEvent> event;
                 {
                     py::gil_scoped_release release;
                     event = self.next();
                 }
                 if (!event) throw py::stop_iteration();
                 return std::move(*event);
             })
        .def("stream_count", &Source::stream_count)
        .def("reports", &Source::reports)
        .def("fetch_stats", &Source::fetch_stats)
        .def("fetch_failures", &Source::fetch_failures);
}

}  // namespace qp::python

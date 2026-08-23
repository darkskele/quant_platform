# transport status

- ~~G1 — Transport concept + InProcessTransport~~
- ~~G2 — CombinedTransport~~

## Last proof

New city stood up from scratch this pass. `transport.hpp` declares the
`Transport` concept (`{ t.next() } -> std::same_as<std::optional<MarketEvent>>`).
`in_process_transport.hpp` moves `InProcessTransport` out of `source`'s old
`queue_source.hpp` into `qp::transport`, fixing its `next()` to dereference
and copy out of the ring's `shared_ptr<const MarketEvent>` slot instead of
returning the pointer directly — the old shape didn't structurally satisfy
`Transport`/`Source` at all. `combined_transport.hpp` adds
`CombinedTransport<Sources...>`: a compile-time function-pointer dispatch
table (`std::array<NextFn, N>`, no vtable) maps a runtime round-robin index
to the right tuple element's `next()`, so an exhausted/quiet source can't
starve the others and no source is favored every call.

`qp_engine` now depends on `qp_transport` instead of `qp_source_pure`;
`Engine`'s `Tx` is constrained on `transport::Transport`, not
`source::Source` (completes D22's rename for the seam Engine actually
consumes).

`qp_transport_tests` covers: concept satisfaction/rejection
(`test_transport.cpp`), `InProcessTransport` against a real `SpmcRing`
including independent per-consumer cursors (`test_in_process_transport.cpp`),
and `CombinedTransport`'s round-robin fairness/exhaustion/drain behavior
against fake sources (`test_combined_transport.cpp`).

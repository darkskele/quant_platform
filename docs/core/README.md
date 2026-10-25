# core

The shared vocabulary and containers every module builds on. Event, intent, order and fill types, the instrument addressing that sizes everything else, the portfolio, and the lock-free queues the threads talk over.

## Diagram

```
MarketEvent ─▶ Intent ─▶ Order ─▶ Fill | Reject
                                    │
                                    ▼
                                Portfolio   sized once from a Subscription

queues between threads
   SpscQueue    one producer, one consumer
   MpscQueue    many producers, one consumer
   SpmcQueue    one producer, every consumer sees every element
   WorkQueue    one producer, each element to one consumer
   SlotRing     producers fill positions in any order, one consumer reads in order
```

## Components

- `types.hpp`. `MarketEvent` as an `EventBase` plus a payload variant, `Intent`, `Order`, `Fill`, `Reject`.
- `subscription.hpp`. `Subscription` and its builder. Resolves (exchange, market, symbol) to small integer slots and reports the extents that size per-instrument arrays.
- `portfolio.hpp`. Positions, cash, fees, funding and marks per instrument slot, and equity over them.
- `exchange.hpp`. `ExchangeId`.
- Queues. `SpscQueue`, `MpscQueue`, `SpmcQueue`, `WorkQueue`, `SlotRing`. Bounded, power of two, lock-free.
- `viewable_pool.hpp`. A fixed capacity append buffer handed out as a span.
- `control_channel.hpp`. Start and stop broadcast to a fixed set of participants.
- `python/`. The vocabulary types bound for python.

## Milestones

- [x] ~~Event, intent, order and fill vocabulary~~
- [x] ~~Subscription-sized instrument slots and `Portfolio`~~
- [x] ~~Lock-free queues for every producer and consumer shape~~
- [x] ~~`ControlChannel`~~
- [ ] Python bindings for the vocabulary generated from each struct's fields by reflection, no hand-listed members (C++26, GCC 16)
    - enums bound from their enumerators the same way
- [ ] `EventKind` count and names read off the enum by reflection, the `END` sentinel dropped (C++26, GCC 16)
- [ ] Compile-time check that `EventKind` order matches the `MarketEventPayload` alternatives (C++26, GCC 16)
- [ ] Book depth bands without a heap allocation per sample
    - bands from a pre-sized pool, the variant stays narrow
- [ ] `ViewablePool` stack storage on `std::inplace_vector`, unused slots never constructed (C++26, GCC 16)
- [ ] Cache-line padding sized by `std::hardware_destructive_interference_size`, not a literal 64 (C++17)
- [ ] `ControlChannel` pump parks on `std::atomic::wait` and stops through `std::jthread`'s stop token, no 100 ms sleep loop (C++20)

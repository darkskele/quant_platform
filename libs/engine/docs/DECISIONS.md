# engine decisions

## D29 — `RiskGate::on_tick` runs once per `step()`; `Strategy::on_timer` isn't wired anywhere yet
`on_tick`'s job (autonomous kill-switch/drawdown flatten) needs *some*
cadence, and "once per processed event" is the only one available in an
event-driven core with no idle-time polling — backtest only ever advances
on pulled events, there's no elapsed-wall-clock tick to hang a different
cadence off. It needed no new scheduling machinery, just the loop
iteration `step()` already has. `on_timer` is different: nothing calls it,
anywhere, in this change. Inventing a timer/scheduling mechanism now, ahead
of any real caller, would be exactly the speculative machinery "seams
first, generality later" warns against — it waits for a concrete strategy
that actually needs a wall-clock or bar-close trigger to shape what that
mechanism should look like.

## D30 — Engine's `Tx` stays constrained on `source::Source`, not renamed `Transport`
`docs/decisions.md` D22 renamed the *concept* Engine depends on from
`Source` to `Transport` in the docs, but deliberately left
`libs/data_source/source/include/source.hpp`'s `Source` concept unrenamed
until Engine/the ring-reading adapter were actually built — to avoid
doc/code drift in the meantime. Engine is now built, but a bare rename
still isn't the right move today: `InProcessTransport` (an in-process ring
reader, the adapter D22's rename was originally motivated by) returns
`optional<shared_ptr<const MarketEvent>>`, not `optional<MarketEvent>` — it
doesn't structurally satisfy `Source` as currently written, so renaming the
concept now would leave that shape mismatch unresolved for no concrete
benefit (nothing wires `InProcessTransport` to an `Engine` yet). Deferred
to whenever something concretely needs it, same as D22 already decided.

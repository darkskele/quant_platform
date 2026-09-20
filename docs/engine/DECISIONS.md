# engine decisions

1. Every collaborator (`Transport`/`ExecutionGateway`/`RiskGate`/`Strategy`/`Portfolio`/`Recorder`) is a template parameter constrained by its concept, with no vtable or heap on the `step()` path.
2. Time arrives on the pull, as `EngineInput::ts`. The loop holds no clock of its own and nothing below it reads the system clock.
3. A pull with no event is a timer tick, so `on_event` and `on_timer` share one ordered time line instead of needing a scheduler.
4. `RiskGate::on_tick()` runs once per `step()`, the only cadence an event-driven core has, and it needs no scheduling machinery.
5. `run()` steps in large batches before polling the control channel and backs off when the transport is dry, keeping the poll off the hot path.
6. On Stop, `run()` flushes the transport and drains what is buffered before returning.
7. `Recorder` is a compile-time policy defaulting to a no-op, sampled once per `step()` after the pull is fully applied, so observation costs nothing when unused and never forks the loop.

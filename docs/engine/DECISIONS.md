# engine decisions

1. Every collaborator (`Transport`/`Clock`/`ExecutionGateway`/`RiskGate`/`Strategy`/`Portfolio`) is a template parameter constrained by its concept, with no vtable or heap on the `step()` path.
2. `RiskGate::on_tick()` runs once per `step()`, the only cadence an event-driven core has, and it needs no scheduling machinery.
3. `Strategy::on_timer` is not wired anywhere yet; it waits for a concrete strategy that needs a timed trigger.
4. `run()` steps in large batches before polling the control channel and sleeps 1ms when the transport is dry, keeping the poll off the hot path.
5. On Stop, `run()` flushes the transport and drains what is buffered before returning.

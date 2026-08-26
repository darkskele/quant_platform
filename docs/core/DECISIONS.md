# core decisions

## Header-only, `INTERFACE` library
No `.cpp` to compile/link — every consumer just gets include paths. Nothing
here needs a translation unit of its own, and header-only keeps the
zero-dependency property trivially checkable (no link step to hide a
dependency in).

## SPSC queue, not a mutex-protected one
One producer (I/O thread), one consumer — a general MPMC-capable queue would
pay for synchronization this project's actual thread topology never needs.
See `docs/architecture-principles.md`'s "single process, a few threads"
metamodel note.

## RoundRobinPool lives here, not in `engine` — see `docs/decisions.md` D33
Generic round-robin worker pool, no trading types in it — `engine` is its
first consumer, not its owner.

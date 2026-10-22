# network/transport/byte_stream decisions

1. A read lends a view into the stream's own buffer. Nothing is copied into the caller's memory.
2. `read(n)` returns at most `n` bytes, fewer when fewer have arrived. It never blocks.
3. Destroying a `Readable` hands its bytes back. There is no separate consume call.
4. `Readable`s may be destroyed in any order. A stream reuses memory only behind the oldest one still alive.
5. A `Readable` holds a `LentBytes`, which hands its bytes back when destroyed. Every stream returns the same type.
6. A `Readable`'s bytes are unreachable from a temporary, so they cannot outlive it by accident.
7. A receive time is nanoseconds on the monotonic clock, taken when the bytes become visible to user space.
8. `Readable` carries the stream's status. A close or an error arrives on the read after the last bytes, with nothing lent.
9. `connect(ip, port)` is part of the concept, because every backend reaches a remote IP and port. Backend settings such as interface, queue and buffer sizes go in the constructor.
10. `connect` blocks and reports failure as the status it returns. It is setup, off the hot path. It closes any current connection first.
11. A stream never waits for bytes. Sleeping or spinning between reads is the caller's choice, outside the stream.
12. A stream receives only once every byte it holds is lent, so each `Readable` comes from one receive and its receive time is exact.
13. A stream is reusable. `close` ends the connection and `connect` opens a new one, keeping the stream's memory.
14. `close` drops bytes not yet lent and bytes not yet sent. `Readable`s already out stay valid and hand back as normal.
15. Destroying a stream closes it.
16. A write lends the stream's own space and `fill` writes into it in place. Nothing is copied in from the caller's memory.
17. `write(max, fill)` gets all `max` bytes of space or none. With no room it returns `Full` and never calls `fill`.
18. Bytes a write accepts are the stream's to deliver. The caller never sends them again.
19. A write sends at once. Bytes that do not go out wait in the stream, ahead of anything written later.
20. `flush` sends what is waiting and reports what it sent and what is left. It is its own call, so a read never sends.
21. `Full` is only ever a write's answer. A stream's lasting status is `Open`, `Closed` or `Error`.
22. A write larger than the stream's send space can never fit, so it is an `Error`.
23. Writing to a closed peer reports `Error`. It never raises a signal.

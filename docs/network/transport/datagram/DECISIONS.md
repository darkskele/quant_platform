# network/transport/datagram decisions

1. A read lends one whole datagram. A datagram is never split or joined.
2. `read()` lends one datagram per call and never blocks. A channel may receive many at once and lend them one at a time.
3. Datagrams come out in the order the channel received them. Loss, reordering and duplicates are the caller's to detect.
4. A datagram lives in a fixed size slot and is lent as a `LentSlot`. Every channel returns the same type.
5. Destroying a `Datagram` hands its slot back. Datagrams may be destroyed in any order.
6. A lent slot is never written until it is handed back.
7. A channel receives only into free slots. With none free, arrivals wait below the channel and are dropped there once that fills.
8. `dropped()` counts datagrams lost for want of a free slot, as the layer that dropped them reports it.
9. A datagram larger than a slot is thrown away and counted by `truncated()`. It is never lent cut short.
10. A read with nothing ready lends no slot. A zero length datagram still holds one.
11. Each datagram carries when it reached the NIC, the kernel and user space.
12. `bind(ip, port)` listens on a local address. Backend settings such as slot size, slot count and batch size go in the constructor.
13. A channel's status is `Open`, `Closed` or `Error`. Only `close` or an error ends a channel, never a peer.
14. `close` drops datagrams received but not yet read. Lent datagrams stay valid and hand back as normal.
15. A channel is reusable. `close` then `bind` again keeps its memory.

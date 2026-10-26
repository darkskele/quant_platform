# network/transport/datagram

A `DatagramChannel` carries whole datagrams both ways. It receives at a local IP and port, lending each datagram in place from a fixed size slot with when it arrived, and counts the datagrams it lost. It sends to one target, lending a slot for the caller to write each datagram in place. It never blocks on a read or a write.

## Diagram

```
DatagramChannel::bind(ip, port)     where it receives
DatagramChannel::target(ip, port)   where it sends, set again to send elsewhere
close() ends both, bind or target again reuses the channel

datagrams in                                 consumers
   │                                            │
   ▼                                            ▼
free slots   filled below the channel        write(max, fill)   lends one slot, fill writes a datagram in place
   │                                            │
   ▼                                            ▼
read()       lends one whole datagram        Written            bytes accepted, status
   │                                            │
   ▼                                            ▼
Datagram     slot, receive times, status     flush()            sends datagrams a write left waiting
   │                                            │
   ▼                                            ▼
consumers    destroying it hands back        datagrams out
```

## Variations

- **KernelUdpChannel**. A kernel UDP socket. One `recvmmsg` fills a batch of slots, which reads lend one at a time. A write sends from a slot of a second pool, and datagrams the kernel cannot take yet wait there for one `sendmmsg`.

## Milestones

- [x] ~~`DatagramChannel` concept, read side~~
- [x] ~~Kernel UDP channel~~
- [x] ~~Send side~~
- [ ] Rig benchmarks, send direction
- [ ] io_uring and eBPF peek channels

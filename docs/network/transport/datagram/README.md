# network/transport/datagram

A `DatagramChannel` receives whole datagrams at a local IP and port. It lends each one in place from a fixed size slot, with when it arrived, and counts the datagrams it lost. It never blocks on a read.

## Diagram

```
DatagramChannel::bind(ip, port)    close() ends it, bind again reuses the channel

datagrams in
   │
   ▼
free slots     filled below the channel, none free means arrivals wait or drop below it
   │
   ▼
read()         lends one whole datagram
   │
   ▼
Datagram       slot, receive times, status
   │
   ▼
consumers      destroying it hands the slot back
```

## Variations

- **KernelUdpChannel**. A kernel UDP socket. One `recvmmsg` fills a batch of slots, which reads lend one at a time.

## Milestones

- [x] ~~`DatagramChannel` concept, read side~~
- [x] ~~Kernel UDP channel~~
- [ ] Send side
- [ ] io_uring and eBPF peek channels

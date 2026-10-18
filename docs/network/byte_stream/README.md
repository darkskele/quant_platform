# network/byte_stream

A `ByteStream` is an ordered byte stream to a remote IP and port. It lends what it has read without copying and never blocks on a read.

## Diagram

```
ByteStream::connect(ip, port)
   │
bytes in
   │
   ▼
ByteStream::read(n)   lends up to n bytes as a Readable
   │
   ▼
Readable              bytes, receive time, stream status
   │
   ▼
consumers             destroying the Readable hands the bytes back
```

## Milestones

- [x] ~~`ByteStream` concept~~
- [ ] Kernel TCP stream
- [ ] Rig benchmarks
- [ ] Busy-polled, io_uring and eBPF peek streams

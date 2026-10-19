# network/byte_stream

A `ByteStream` is an ordered byte stream to a remote IP and port. It lends what it has read without copying and never blocks on a read.

## Diagram

```
ByteStream::connect(ip, port)    close() ends it, connect again reuses the stream
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

## Variations

- **KernelTcpStream**. A kernel TCP socket, received straight into the stream's ring.

## Milestones

- [x] ~~`ByteStream` concept~~
- [x] ~~Kernel TCP stream~~
- [ ] Rig benchmarks
- [ ] io_uring and eBPF peek streams

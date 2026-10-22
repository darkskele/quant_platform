# network/transport/byte_stream

A `ByteStream` is an ordered two way byte stream to a remote IP and port. It lends what it has read, and the space to write into, without copying. It never blocks on a read or a write.

## Diagram

```
ByteStream::connect(ip, port)    close() ends it, connect again reuses the stream

bytes in                                     consumers
   │                                            │
   ▼                                            ▼
read(n)       lends up to n bytes            write(max, fill)   lends max bytes of space, fill writes in place
   │                                            │
   ▼                                            ▼
Readable      bytes, receive time, status    Written            bytes accepted, status
   │                                            │
   ▼                                            ▼
consumers     destroying it hands back       flush()            sends what a write left waiting
                                                │
                                                ▼
                                             bytes out
```

## Variations

- **KernelTcpStream**. A kernel TCP socket. Receives straight into one ring and sends from a second one the caller fills in place.

## Milestones

- [x] ~~`ByteStream` concept~~
- [x] ~~Kernel TCP stream~~
- [ ] Rig benchmarks
- [ ] io_uring and eBPF peek streams

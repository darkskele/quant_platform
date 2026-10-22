# network/transport

Moves bytes between this machine and the network. Each kind of delivery is its own concept, and each concept has its own backends.

## Diagram

```
bytes on the wire
   ▲ │                        │
   │ ▼                        ▼
ByteStream                 DatagramChannel
ordered bytes both ways    whole datagrams in
   ▲ │                        │
   │ ▼                        ▼
consumers                  consumers
```

## Components

- **[byte_stream](byte_stream/README.md)**. The `ByteStream` concept and its backends.
- **[datagram](datagram/README.md)**. The `DatagramChannel` concept and its backends.
- **Receive times**. When received bytes reached the NIC, the kernel and user space, and which of those a backend takes. Shared by every concept here.

## Milestones

- [x] ~~Byte streams~~
- [ ] Datagram channels
    - [x] ~~Concept, read side~~
    - [ ] Kernel UDP channel

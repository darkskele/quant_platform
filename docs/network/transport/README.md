# network/transport

Moves bytes to and from a remote address. Each kind of delivery is its own concept, and each concept has its own backends.

## Diagram

```
bytes on the wire
   ▲ │
   │ ▼
ByteStream        ordered bytes both ways
   ▲ │
   │ ▼
consumers
```

## Components

- **[byte_stream](byte_stream/README.md)**. The `ByteStream` concept and its backends.

## Milestones

- [x] ~~Byte streams~~
- [ ] Datagram channels

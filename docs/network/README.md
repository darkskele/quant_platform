# network

The network stack. Each layer is a concept, and each layer above is built on the concept below, so a backend swaps without its consumers changing.

## Diagram

```
bytes on the wire
   │
   ▼
ByteStream        ordered bytes, lent without copying
   │
   ▼
consumers
```

## Components

- **[byte_stream](byte_stream/README.md)**. The `ByteStream` concept and its backends.

## Milestones

- [x] ~~Byte streams~~
- [ ] Datagram channels
- [ ] Security layer over any `ByteStream`
- [ ] Application protocols over any `ByteStream`

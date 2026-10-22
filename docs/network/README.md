# network

The network stack. Each layer is a concept, and each layer above is built on the concept below, so a backend swaps without its consumers changing.

## Diagram

```
bytes on the wire
   ▲ │
   │ ▼
transport         bytes to and from a remote address, lent without copying
   ▲ │
   │ ▼
consumers
```

## Components

- **[transport](transport/README.md)**. Moving bytes to and from a remote address, one concept per kind of delivery.

## Milestones

- [ ] Transport
    - [x] ~~Byte streams~~
    - [ ] Datagram channels
- [ ] Security layer over any `ByteStream`
- [ ] Application protocols over any `ByteStream`

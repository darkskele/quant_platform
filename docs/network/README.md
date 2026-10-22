# network

The network stack. Each layer is a concept, and each layer above is built on the concept below, so a backend swaps without its consumers changing.

## Diagram

```
bytes on the wire
   ▲ │
   │ ▼
transport         bytes between this machine and the network, lent without copying
   ▲ │
   │ ▼
consumers
```

## Components

- **[transport](transport/README.md)**. Moving bytes between this machine and the network, one concept per kind of delivery.

## Milestones

- [ ] Transport
    - [x] ~~Byte streams~~
    - [ ] Datagram channels
- [ ] Security layer over any `ByteStream`
- [ ] Application protocols over any `ByteStream`

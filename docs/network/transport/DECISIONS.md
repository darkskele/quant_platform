# network/transport decisions

1. Each kind of delivery is its own concept. Ordered bytes and separate messages never share one.
2. Receive times, and which of them a backend takes, are one set of types shared by every concept.

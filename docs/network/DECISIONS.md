# network decisions

1. Every layer is a concept, and the layer above is a template over the one below. No virtual dispatch.
2. Each connection picks its own stack, so different backends can run in one process.
3. The caller owns the loop and the thread. No callbacks, no executors.

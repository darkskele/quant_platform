# sink decisions

1. `Sink::record()` returns a bool and never blocks; false means the event was dropped rather than backpressuring the source.
2. `FanoutSink` fans out to `NumConsumers` in-process readers through one SPMC queue; a multi-venue setup uses one per venue.

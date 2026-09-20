# binance_historical decisions

1. The subscription decides which markets and symbols exist. The config only says which datasets to pull for them.
2. Market slots are Binance's own market numbering, so a slot maps to its bucket path without a second table.
3. A dataset a market does not publish, such as spot funding, is skipped rather than being an error.
4. Files are discovered by listing the bucket, never by generating dates, so a missing file is known before it is asked for.
5. The listing host and the data host are different endpoints. The data host does not answer list queries.
6. A cadence the dataset does not publish falls back to monthly rather than building a path that will 404.
7. Timestamp unit and the presence of a header row vary by file date inside one dataset, so both are sniffed per row rather than configured.
8. Coin-M klines count contracts in column 5, so volume is read from column 7 there. Volume then means base asset units on every market.
9. Each stream owns its own prefetch window and slot ring. Streams times depth is the concurrency a run asks for, and it only pays while that is under the pool's worker count.
10. A failed fetch still travels to its stream, carrying its status, so the stream advances instead of waiting on a file that will never land.
11. A failed file still counts its expected rows, so the shortfall shows against rows parsed rather than disappearing.
12. Fetch and decompression happen on pool threads, parsing happens on the pull thread. `next()` does one refill plus a heap pop in steady state.
13. The merge holds only streams without a front event in a pending list, everything else in a heap, so a pull never walks every stream.
14. Heap ties fall to the lower stream index, so a replay of the same span yields the same order every time.
15. The source owns its pool and quiesces it in the destructor, so workers are joined while the queues they write into are still alive.
16. Streams pump on starvation and on a tick mask otherwise, so a busy stream pays a local increment per event rather than the ring's atomics.
17. Per-stream gap stats are reported rather than thrown on, since a historical sweep is judged by what it read, not by whether every file existed.
18. Rows repeating the stamp before them are counted, not dropped. The 2020 and early 2021 metrics files publish every row twice, and a dataset like aggTrades shares a stamp across genuinely distinct events, so dropping on a repeat could not be a stream wide rule.

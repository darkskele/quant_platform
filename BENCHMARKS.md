## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.01 ns | 5.79 % |
| BM_SimClock_Now | 5 | 1 | 0.21 ns | 0.21 ns | 0.19 ns | 0.04 ns | 21.32 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 2.88 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.18 ns | 1.20 ns | 1.18 ns | 0.10 ns | 8.29 % |
| BM_Spsc_PopInt | 5 | 1 | 6.94 ns | 7.10 ns | 6.74 ns | 0.55 ns | 7.89 % |
| BM_Spsc_PushPopInt | 5 | 1 | 6.89 ns | 7.02 ns | 6.88 ns | 0.21 ns | 3.09 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 39.37 ns | 40.09 ns | 36.04 ns | 5.14 ns | 13.05 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 46.68 ns | 95.08 ns | 45.81 ns | 4.33 ns | 9.28 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.62 ns | 1.66 ns | 1.58 ns | 0.30 ns | 18.56 % |
| BM_Spmc_TryPopInt | 5 | 1 | 7.26 ns | 7.41 ns | 7.10 ns | 0.44 ns | 6.05 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 10.87 ns | 11.07 ns | 8.78 ns | 4.32 ns | 39.76 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 61.86 ns | 63.00 ns | 59.81 ns | 6.39 ns | 10.33 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 7.52 ns | 7.66 ns | 7.25 ns | 0.53 ns | 7.07 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 27.15 ns | 27.64 ns | 26.52 ns | 1.49 ns | 5.50 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 52.17 ns | 53.13 ns | 51.76 ns | 1.13 ns | 2.16 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 37.96 ns | 76.08 ns | 37.32 ns | 3.29 ns | 8.66 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 24.60 ns | 124.60 ns | 24.47 ns | 0.22 ns | 0.91 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 212.61 ns | 1,604.41 ns | 56.82 ns | 235.43 ns | 110.73 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 11.40 ns | 11.64 ns | 11.23 ns | 0.32 ns | 2.79 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 7.32 ns | 7.44 ns | 7.19 ns | 0.31 ns | 4.27 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 13.88 ns | 14.14 ns | 13.98 ns | 0.27 ns | 1.93 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 36.93 ns | 75.24 ns | 38.39 ns | 5.92 ns | 16.04 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 109.22 ns | 555.56 ns | 97.93 ns | 18.46 ns | 16.90 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 112.73 ns | 1,022.70 ns | 112.27 ns | 14.73 ns | 13.06 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 58.77 ns | 60.17 ns | 61.23 ns | 7.57 ns | 12.88 % |
| BM_ControlChannel_Poll | 5 | 1 | 46.89 ns | 47.98 ns | 46.37 ns | 1.54 ns | 3.29 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 14.87 ns | 15.14 ns | 14.87 ns | 0.29 ns | 1.97 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 2.68 ns | 2.73 ns | 2.68 ns | 0.02 ns | 0.67 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 2.64 ns | 2.69 ns | 2.67 ns | 0.06 ns | 2.17 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.57 ns | 0.58 ns | 0.57 ns | 0.01 ns | 2.46 % |
| BM_Portfolio_Position | 5 | 1 | 0.17 ns | 0.18 ns | 0.17 ns | 0.01 ns | 8.27 % |
| BM_Portfolio_Equity | 5 | 1 | 338.03 ns | 344.40 ns | 315.73 ns | 46.36 ns | 13.71 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.31 ns | 1.33 ns | 1.26 ns | 0.08 ns | 5.76 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.30 ns | 1.33 ns | 1.31 ns | 0.03 ns | 2.13 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.33 ns | 1.37 ns | 1.34 ns | 0.03 ns | 2.52 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.38 ns | 1.42 ns | 1.33 ns | 0.10 ns | 6.93 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.44 ns | 0.45 ns | 0.45 ns | 0.04 ns | 8.02 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.43 ns | 0.44 ns | 0.42 ns | 0.02 ns | 5.17 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.58 ns | 0.59 ns | 0.57 ns | 0.02 ns | 3.92 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.33 ns | 0.33 ns | 0.33 ns | 0.01 ns | 2.26 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.36 ns | 0.36 ns | 0.35 ns | 0.03 ns | 9.53 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 668.94 ns | 682.38 ns | 659.67 ns | 27.39 ns | 4.10 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 42.22 ns | 42.99 ns | 42.01 ns | 0.50 ns | 1.17 % |
| BM_SymbolTable_Name | 5 | 1 | 0.48 ns | 0.49 ns | 0.48 ns | 0.01 ns | 1.51 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 0.96 ns | 0.98 ns | 0.95 ns | 0.01 ns | 1.27 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.53 ns | 0.54 ns | 0.52 ns | 0.02 ns | 3.75 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.86 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.02 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 1.16 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 2.46 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 24.27 ns | 24.71 ns | 24.23 ns | 0.61 ns | 2.53 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 24.80 ns | 25.26 ns | 25.10 ns | 0.60 ns | 2.44 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.27 ns | 1.29 ns | 1.27 ns | 0.01 ns | 0.95 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 8,317,681.31 ns | 8,468,970.29 ns | 10,144,114.62 ns | 2,957,754.73 ns | 35.56 % | 2,671,280.91 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 3,497,584.29 ns | 3,561,207.43 ns | 3,506,052.33 ns | 193,060.59 ns | 5.52 % | 14,074,375.38 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 3,578,618.79 ns | 3,644,352.58 ns | 3,596,037.30 ns | 85,445.64 ns | 2.39 % | 13,726,107.65 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 363.56 ns | 370.25 ns | 365.75 ns | 6.79 ns | 1.87 % | 2,701,610.02 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 1,594.47 ns | 1,623.73 ns | 1,601.79 ns | 19.73 ns | 1.24 % | 615,940.19 |
| BM_BinanceParser_AggTrade | 5 | 1 | 276.16 ns | 281.24 ns | 276.94 ns | 6.06 ns | 2.19 % | 3,557,022.60 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 42.98 ns | 43.77 ns | 42.84 ns | 0.49 ns | 1.13 % | 22,850,746.19 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 31.81 ns | 32.40 ns | 32.00 ns | 0.61 ns | 1.91 % | 30,877,548.80 |
| BM_Wire_WriteTrade | 5 | 1 | 25.83 ns | 26.30 ns | 25.77 ns | 0.43 ns | 1.65 % | 38,027,452.59 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 60.90 ns | 62.03 ns | 60.19 ns | 1.16 ns | 1.90 % | 16,127,115.42 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 94.40 ns | 96.16 ns | 94.22 ns | 3.85 ns | 4.08 % | 10,412,844.00 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 865.20 ns | 881.35 ns | 792.39 ns | 180.10 ns | 20.82 % | 1,166,611.24 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,181.50 ns | 1,203.55 ns | 1,168.98 ns | 27.47 ns | 2.33 % | 831,225.59 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 1,955.66 ns | 1,992.20 ns | 1,952.39 ns | 37.33 ns | 1.91 % | 502,103.42 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 132.43 ns | 135.02 ns | 132.26 ns | 3.88 ns | 2.93 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 19,214.72 ns | 19,573.55 ns | 19,277.20 ns | 275.22 ns | 1.43 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 1,991.82 ns | 2,029.03 ns | 1,990.81 ns | 19.39 ns | 0.97 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 21,460.95 ns | 21,861.68 ns | 21,815.83 ns | 581.32 ns | 2.71 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 0.87 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 0.96 % |
| BM_Partition_FormatDay | 5 | 1 | 110.57 ns | 112.64 ns | 110.50 ns | 0.88 ns | 0.79 % |
| BM_Partition_SegmentPath | 5 | 1 | 444.59 ns | 452.97 ns | 447.30 ns | 4.93 ns | 1.11 % |
| BM_Partition_ListSegments | 5 | 1 | 7,386.25 ns | 7,525.71 ns | 7,414.75 ns | 176.30 ns | 2.39 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 184.14 ns | 187.57 ns | 218.02 ns | 60.89 ns | 33.07 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 328.37 ns | 334.57 ns | 325.85 ns | 10.90 ns | 3.32 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 430.77 ns | 438.89 ns | 429.81 ns | 23.60 ns | 5.48 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.32 ns | 0.32 ns | 0.32 ns | 0.00 ns | 1.47 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 44.35 ns | 45.18 ns | 44.30 ns | 0.24 ns | 0.55 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 369.53 ns | 376.50 ns | 365.73 ns | 9.66 ns | 2.61 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 562.10 ns | 572.71 ns | 559.14 ns | 10.55 ns | 1.88 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 638.11 ns | 650.08 ns | 638.71 ns | 9.15 ns | 1.43 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 179.03 ns | 182.41 ns | 178.43 ns | 2.31 ns | 1.29 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategy | 5 | 1 | 0.64 ns | 0.65 ns | 0.63 ns | 0.01 ns | 2.10 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 0.64 ns | 0.65 ns | 0.64 ns | 0.01 ns | 0.93 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 99.24 ns | 101.06 ns | 99.64 ns | 1.48 ns | 1.49 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 1.92 ns | 1.95 ns | 1.92 ns | 0.03 ns | 1.60 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 1.92 ns | 1.96 ns | 1.91 ns | 0.04 ns | 1.95 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.39 ns | 0.40 ns | 0.39 ns | 0.01 ns | 3.49 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.64 ns | 1.67 ns | 1.64 ns | 0.03 ns | 1.84 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.63 ns | 1.66 ns | 1.62 ns | 0.03 ns | 2.05 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 290.25 ns | 295.60 ns | 287.59 ns | 8.53 ns | 2.94 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 813.17 ns | 828.16 ns | 814.42 ns | 13.62 ns | 1.67 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 4.94 ns | 5.03 ns | 4.95 ns | 0.17 ns | 3.54 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 0.99 ns | 1.01 ns | 0.98 ns | 0.03 ns | 2.55 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.65 ns | 0.66 ns | 0.65 ns | 0.01 ns | 1.95 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 2.59 ns | 2.64 ns | 2.60 ns | 0.04 ns | 1.55 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 1.93 ns | 1.97 ns | 1.94 ns | 0.05 ns | 2.46 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 32.28 ns | 32.91 ns | 32.59 ns | 1.32 ns | 4.07 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 40.02 ns | 40.75 ns | 40.05 ns | 0.49 ns | 1.23 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 49.35 ns | 99.52 ns | 48.33 ns | 5.26 ns | 10.65 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 43.03 ns | 218.53 ns | 43.54 ns | 2.08 ns | 4.84 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 33.86 ns | 34.49 ns | 28.47 ns | 8.71 ns | 25.71 % | 4,088,020.60 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,480,261.04 ns | 628,990.26 ns | 19,176,886.10 ns | 894,644.11 ns | 4.59 % | 0.00 | 1,592,006.05 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 344.49 ns | 350.96 ns | 342.06 ns | 5.72 ns | 1.66 % | 2,849,963,370.17 |
| BM_RunDataSource_FourPairs | 5 | 1 | 341.74 ns | 348.22 ns | 342.05 ns | 6.25 ns | 1.83 % | 11,490,162,454.74 |

## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.15 ns | 0.15 ns | 0.15 ns | 0.00 ns | 2.12 % |
| BM_SimClock_Now | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.01 ns | 4.14 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 2.91 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 0.91 ns | 0.94 ns | 0.91 ns | 0.01 ns | 1.02 % |
| BM_Spsc_PopInt | 5 | 1 | 6.26 ns | 6.41 ns | 6.24 ns | 0.06 ns | 0.90 % |
| BM_Spsc_PushPopInt | 5 | 1 | 6.92 ns | 7.07 ns | 6.84 ns | 0.15 ns | 2.18 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 35.79 ns | 36.59 ns | 34.87 ns | 1.73 ns | 4.83 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 43.78 ns | 89.47 ns | 46.32 ns | 21.51 ns | 49.13 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.28 ns | 1.32 ns | 1.28 ns | 0.04 ns | 3.01 % |
| BM_Spmc_TryPopInt | 5 | 1 | 6.68 ns | 6.82 ns | 6.67 ns | 0.08 ns | 1.21 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 6.99 ns | 7.14 ns | 6.89 ns | 0.14 ns | 2.02 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 53.40 ns | 54.61 ns | 53.40 ns | 1.39 ns | 2.60 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 7.02 ns | 7.17 ns | 7.00 ns | 0.09 ns | 1.34 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 25.95 ns | 26.54 ns | 26.03 ns | 0.30 ns | 1.16 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 51.26 ns | 52.42 ns | 51.14 ns | 0.64 ns | 1.24 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 36.94 ns | 74.69 ns | 36.65 ns | 2.02 ns | 5.46 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 24.80 ns | 126.65 ns | 24.74 ns | 0.59 ns | 2.37 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 35.49 ns | 296.42 ns | 22.55 ns | 26.63 ns | 75.03 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 11.15 ns | 11.41 ns | 11.12 ns | 0.10 ns | 0.93 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 6.99 ns | 7.15 ns | 6.98 ns | 0.11 ns | 1.54 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 13.01 ns | 13.31 ns | 13.00 ns | 0.26 ns | 2.01 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 37.08 ns | 75.83 ns | 36.43 ns | 1.62 ns | 4.37 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 87.19 ns | 445.80 ns | 91.22 ns | 16.35 ns | 18.75 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 114.29 ns | 1,032.27 ns | 108.22 ns | 18.25 ns | 15.96 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 1.01 ns | 1.03 ns | 1.05 ns | 0.07 ns | 6.81 % |
| BM_ControlChannel_Poll | 5 | 1 | 47.47 ns | 48.82 ns | 46.96 ns | 0.88 ns | 1.85 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 2.61 ns | 2.67 ns | 2.61 ns | 0.04 ns | 1.45 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 2.57 ns | 2.63 ns | 2.57 ns | 0.05 ns | 2.06 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.52 ns | 0.53 ns | 0.52 ns | 0.01 ns | 1.35 % |
| BM_Portfolio_Position | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 2.09 % |
| BM_Portfolio_Equity | 5 | 1 | 280.97 ns | 287.37 ns | 281.96 ns | 5.36 ns | 1.91 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.27 ns | 1.30 ns | 1.25 ns | 0.05 ns | 4.05 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.24 ns | 1.27 ns | 1.24 ns | 0.01 ns | 1.15 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.26 ns | 1.29 ns | 1.25 ns | 0.03 ns | 2.08 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.27 ns | 1.30 ns | 1.24 ns | 0.05 ns | 4.24 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.39 ns | 0.40 ns | 0.39 ns | 0.01 ns | 1.60 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.39 ns | 0.40 ns | 0.39 ns | 0.01 ns | 1.86 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.32 ns | 0.33 ns | 0.32 ns | 0.01 ns | 1.99 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.32 ns | 0.33 ns | 0.32 ns | 0.00 ns | 0.87 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.56 ns | 0.57 ns | 0.56 ns | 0.02 ns | 4.04 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 663.51 ns | 678.86 ns | 671.72 ns | 15.28 ns | 2.30 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 42.54 ns | 43.49 ns | 41.37 ns | 4.01 ns | 9.41 % |
| BM_SymbolTable_Name | 5 | 1 | 0.49 ns | 0.50 ns | 0.48 ns | 0.01 ns | 1.34 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 0.98 ns | 1.00 ns | 0.99 ns | 0.02 ns | 2.50 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.33 ns | 0.33 ns | 0.33 ns | 0.01 ns | 1.95 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.83 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.66 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.50 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.99 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 24.48 ns | 25.03 ns | 24.57 ns | 0.43 ns | 1.74 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 24.07 ns | 24.61 ns | 24.16 ns | 0.41 ns | 1.70 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.30 ns | 1.33 ns | 1.30 ns | 0.02 ns | 1.85 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 7,036,705.82 ns | 7,193,190.53 ns | 5,659,746.46 ns | 2,920,230.07 ns | 41.50 % | 3,173,208.90 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 3,402,666.86 ns | 3,478,580.12 ns | 3,378,613.79 ns | 136,261.08 ns | 4.00 % | 14,391,954.20 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 3,505,439.24 ns | 3,565,616.69 ns | 3,547,920.08 ns | 171,403.25 ns | 4.89 % | 14,049,305.69 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 375.86 ns | 384.35 ns | 375.42 ns | 4.64 ns | 1.23 % | 2,602,139.46 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 1,593.63 ns | 1,629.68 ns | 1,584.88 ns | 24.00 ns | 1.51 % | 613,728.69 |
| BM_BinanceParser_AggTrade | 5 | 1 | 276.97 ns | 283.24 ns | 276.91 ns | 2.21 ns | 0.80 % | 3,530,772.30 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 38.61 ns | 39.48 ns | 38.59 ns | 0.57 ns | 1.49 % | 25,332,625.03 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 29.00 ns | 29.66 ns | 28.99 ns | 0.36 ns | 1.25 % | 33,722,526.72 |
| BM_Wire_WriteTrade | 5 | 1 | 22.86 ns | 23.38 ns | 22.84 ns | 0.25 ns | 1.09 % | 42,781,630.37 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 60.62 ns | 61.99 ns | 60.16 ns | 1.15 ns | 1.89 % | 16,136,696.54 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 91.27 ns | 93.34 ns | 91.54 ns | 2.12 ns | 2.33 % | 10,718,680.15 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 779.35 ns | 796.99 ns | 772.78 ns | 28.64 ns | 3.68 % | 1,256,071.05 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,110.43 ns | 1,135.57 ns | 1,108.94 ns | 15.62 ns | 1.41 % | 880,752.23 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 1,981.54 ns | 2,026.38 ns | 1,973.24 ns | 49.88 ns | 2.52 % | 493,738.06 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 133.86 ns | 137.03 ns | 132.41 ns | 2.92 ns | 2.18 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 19,374.05 ns | 19,815.32 ns | 19,385.22 ns | 434.77 ns | 2.24 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 1,998.58 ns | 2,044.15 ns | 2,003.29 ns | 30.72 ns | 1.54 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 21,477.14 ns | 21,967.01 ns | 21,514.78 ns | 355.42 ns | 1.65 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.48 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.01 ns | 3.59 % |
| BM_Partition_FormatDay | 5 | 1 | 118.48 ns | 121.18 ns | 117.65 ns | 2.35 ns | 1.98 % |
| BM_Partition_SegmentPath | 5 | 1 | 443.86 ns | 453.96 ns | 442.63 ns | 7.67 ns | 1.73 % |
| BM_Partition_ListSegments | 5 | 1 | 7,489.34 ns | 7,659.92 ns | 7,379.17 ns | 179.99 ns | 2.40 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 212.89 ns | 217.74 ns | 215.90 ns | 12.85 ns | 6.04 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 324.40 ns | 331.79 ns | 320.52 ns | 13.03 ns | 4.02 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 414.35 ns | 423.79 ns | 413.66 ns | 30.91 ns | 7.46 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.32 ns | 0.33 ns | 0.32 ns | 0.00 ns | 1.09 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 44.62 ns | 45.62 ns | 44.62 ns | 0.43 ns | 0.97 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 376.20 ns | 384.58 ns | 380.89 ns | 17.83 ns | 4.74 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 559.87 ns | 572.34 ns | 561.09 ns | 7.32 ns | 1.31 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 630.98 ns | 645.04 ns | 630.50 ns | 21.82 ns | 3.46 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 176.20 ns | 180.13 ns | 174.98 ns | 3.77 ns | 2.14 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategy | 5 | 1 | 0.65 ns | 0.66 ns | 0.64 ns | 0.02 ns | 2.40 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 0.65 ns | 0.66 ns | 0.64 ns | 0.02 ns | 2.89 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_NRings<2> | 5 | 1 | 109.30 ns | 111.74 ns | 106.72 ns | 6.90 ns | 6.31 % |
| BM_BacktestInProcessTransport_NRings<4> | 5 | 1 | 241.32 ns | 246.70 ns | 242.65 ns | 4.46 ns | 1.85 % |
| BM_BacktestInProcessTransport_NRings<8> | 5 | 1 | 558.95 ns | 571.40 ns | 552.41 ns | 16.51 ns | 2.95 % |
| BM_BacktestInProcessTransport_NRings<16> | 5 | 1 | 1,351.43 ns | 1,381.62 ns | 1,269.08 ns | 231.40 ns | 17.12 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 1.91 ns | 1.95 ns | 1.91 ns | 0.01 ns | 0.72 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.20 ns | 2.25 ns | 2.14 ns | 0.25 ns | 11.59 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.59 ns | 0.61 ns | 0.59 ns | 0.02 ns | 3.55 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.61 ns | 1.65 ns | 1.60 ns | 0.03 ns | 1.94 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.61 ns | 1.65 ns | 1.62 ns | 0.02 ns | 1.49 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 301.36 ns | 308.14 ns | 293.31 ns | 14.94 ns | 4.96 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 799.26 ns | 817.40 ns | 798.18 ns | 6.07 ns | 0.76 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 4.85 ns | 4.96 ns | 4.90 ns | 0.10 ns | 2.02 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 0.98 ns | 1.00 ns | 0.98 ns | 0.03 ns | 2.94 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.64 ns | 0.65 ns | 0.63 ns | 0.01 ns | 2.06 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 2.56 ns | 2.62 ns | 2.53 ns | 0.05 ns | 2.09 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 1.91 ns | 1.96 ns | 1.92 ns | 0.01 ns | 0.54 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 30.64 ns | 31.35 ns | 30.00 ns | 1.11 ns | 3.61 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 40.40 ns | 41.32 ns | 39.99 ns | 1.10 ns | 2.73 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 46.65 ns | 94.48 ns | 47.71 ns | 2.29 ns | 4.91 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 38.75 ns | 197.20 ns | 41.81 ns | 7.84 ns | 20.24 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 32.62 ns | 33.36 ns | 30.38 ns | 5.45 ns | 16.72 % | 4,161,599.40 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,118,323.92 ns | 626,364.31 ns | 19,079,484.82 ns | 523,867.14 ns | 2.74 % | 0.00 | 1,599,924.56 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 334.83 ns | 342.33 ns | 335.27 ns | 2.30 ns | 0.69 % | 2,921,295,912.08 |
| BM_RunDataSource_FourPairs | 5 | 1 | 333.78 ns | 341.28 ns | 333.89 ns | 5.02 ns | 1.50 % | 11,722,865,302.17 |

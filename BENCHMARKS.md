## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.12 ns | 0.13 ns | 0.12 ns | 0.01 ns | 4.67 % |
| BM_SimClock_Now | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 4.07 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 8.82 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.00 ns | 1.02 ns | 1.00 ns | 0.02 ns | 2.29 % |
| BM_Spsc_PopInt | 5 | 1 | 7.27 ns | 7.36 ns | 7.37 ns | 0.56 ns | 7.65 % |
| BM_Spsc_PushPopInt | 5 | 1 | 8.34 ns | 8.31 ns | 8.42 ns | 0.27 ns | 3.26 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 37.94 ns | 37.77 ns | 37.81 ns | 1.35 ns | 3.55 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 52.67 ns | 104.86 ns | 53.24 ns | 3.41 ns | 6.47 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.49 ns | 1.49 ns | 1.48 ns | 0.03 ns | 1.95 % |
| BM_Spmc_TryPopInt | 5 | 1 | 7.06 ns | 7.03 ns | 7.07 ns | 0.04 ns | 0.58 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 7.53 ns | 7.63 ns | 7.55 ns | 0.14 ns | 1.87 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 62.24 ns | 63.57 ns | 60.38 ns | 7.16 ns | 11.50 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 8.28 ns | 8.46 ns | 8.28 ns | 0.32 ns | 3.84 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 28.56 ns | 29.17 ns | 28.78 ns | 0.64 ns | 2.23 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 57.55 ns | 59.13 ns | 56.89 ns | 2.49 ns | 4.32 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 38.89 ns | 77.90 ns | 37.66 ns | 3.03 ns | 7.78 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 23.80 ns | 122.18 ns | 23.85 ns | 0.84 ns | 3.53 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 33.23 ns | 281.69 ns | 25.46 ns | 25.10 ns | 75.53 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 11.37 ns | 11.76 ns | 11.20 ns | 0.47 ns | 4.15 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 7.02 ns | 7.24 ns | 6.96 ns | 0.18 ns | 2.49 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 14.16 ns | 14.61 ns | 14.21 ns | 0.33 ns | 2.35 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 41.38 ns | 85.40 ns | 39.86 ns | 3.05 ns | 7.38 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 86.68 ns | 447.18 ns | 89.21 ns | 10.79 ns | 12.45 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 108.24 ns | 1,004.04 ns | 108.22 ns | 3.25 ns | 3.00 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 0.86 ns | 0.89 ns | 0.81 ns | 0.13 ns | 15.38 % |
| BM_ControlChannel_Poll | 5 | 1 | 49.53 ns | 51.82 ns | 48.99 ns | 3.27 ns | 6.61 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 2.74 ns | 2.86 ns | 2.80 ns | 0.13 ns | 4.78 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 2.65 ns | 2.77 ns | 2.65 ns | 0.05 ns | 1.75 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.62 ns | 0.65 ns | 0.61 ns | 0.02 ns | 2.84 % |
| BM_Portfolio_Position | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.00 ns | 1.68 % |
| BM_Portfolio_Equity | 5 | 1 | 292.08 ns | 305.17 ns | 294.95 ns | 9.74 ns | 3.33 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.26 ns | 1.32 ns | 1.27 ns | 0.02 ns | 1.32 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.41 ns | 1.48 ns | 1.37 ns | 0.07 ns | 5.03 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.32 ns | 1.38 ns | 1.32 ns | 0.03 ns | 2.04 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.31 ns | 1.36 ns | 1.31 ns | 0.04 ns | 2.74 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.01 ns | 2.77 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.01 ns | 2.11 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.58 ns | 0.59 ns | 0.58 ns | 0.01 ns | 2.08 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.33 ns | 0.33 ns | 0.33 ns | 0.01 ns | 2.33 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.35 ns | 0.35 ns | 0.35 ns | 0.02 ns | 5.06 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 659.06 ns | 669.12 ns | 657.76 ns | 19.38 ns | 2.94 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 43.05 ns | 43.65 ns | 42.92 ns | 1.03 ns | 2.39 % |
| BM_SymbolTable_Name | 5 | 1 | 0.49 ns | 0.49 ns | 0.49 ns | 0.01 ns | 1.60 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 0.97 ns | 0.99 ns | 0.98 ns | 0.01 ns | 1.44 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.52 ns | 0.53 ns | 0.54 ns | 0.03 ns | 6.39 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 5.43 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.18 ns | 0.18 ns | 0.18 ns | 0.01 ns | 5.21 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 4.06 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 5.61 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 25.90 ns | 25.57 ns | 25.54 ns | 1.14 ns | 4.42 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 27.01 ns | 26.67 ns | 26.16 ns | 1.80 ns | 6.67 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.42 ns | 1.41 ns | 1.41 ns | 0.06 ns | 4.37 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 7,219,920.87 ns | 7,187,524.20 ns | 5,210,211.63 ns | 3,324,308.50 ns | 46.04 % | 3,233,611.67 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 3,798,276.47 ns | 3,879,608.34 ns | 3,764,011.42 ns | 163,340.82 ns | 4.30 % | 12,906,480.58 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 4,066,779.51 ns | 4,153,998.29 ns | 3,991,401.74 ns | 199,193.98 ns | 4.90 % | 12,059,261.43 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 410.49 ns | 419.28 ns | 411.40 ns | 9.80 ns | 2.39 % | 2,386,113.21 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 1,639.96 ns | 1,677.19 ns | 1,644.12 ns | 42.01 ns | 2.56 % | 596,500.49 |
| BM_BinanceParser_AggTrade | 5 | 1 | 276.75 ns | 286.86 ns | 267.98 ns | 21.57 ns | 7.79 % | 3,501,567.51 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 43.24 ns | 44.82 ns | 42.97 ns | 0.65 ns | 1.51 % | 22,316,893.07 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 38.74 ns | 40.15 ns | 36.45 ns | 5.94 ns | 15.33 % | 25,315,211.46 |
| BM_Wire_WriteTrade | 5 | 1 | 35.45 ns | 36.74 ns | 35.91 ns | 3.45 ns | 9.72 % | 27,431,360.80 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 76.26 ns | 79.03 ns | 66.48 ns | 14.64 ns | 19.20 % | 13,011,851.63 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 103.15 ns | 106.90 ns | 102.41 ns | 3.00 ns | 2.91 % | 9,360,332.49 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 836.88 ns | 867.37 ns | 844.95 ns | 25.98 ns | 3.10 % | 1,153,831.22 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,179.84 ns | 1,222.78 ns | 1,191.32 ns | 75.13 ns | 6.37 % | 820,465.26 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,135.42 ns | 2,213.16 ns | 2,141.90 ns | 86.96 ns | 4.07 % | 452,459.79 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 140.65 ns | 145.93 ns | 139.81 ns | 4.12 ns | 2.93 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 19,952.49 ns | 20,679.05 ns | 20,126.35 ns | 945.41 ns | 4.74 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,168.53 ns | 2,257.02 ns | 2,070.22 ns | 194.89 ns | 8.99 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 24,745.35 ns | 25,831.35 ns | 25,020.19 ns | 4,256.11 ns | 17.20 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.17 ns | 0.18 ns | 0.17 ns | 0.01 ns | 6.36 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 0.53 % |
| BM_Partition_FormatDay | 5 | 1 | 108.84 ns | 113.62 ns | 108.01 ns | 1.98 ns | 1.82 % |
| BM_Partition_SegmentPath | 5 | 1 | 442.52 ns | 461.96 ns | 439.96 ns | 9.43 ns | 2.13 % |
| BM_Partition_ListSegments | 5 | 1 | 7,343.99 ns | 7,666.34 ns | 7,328.39 ns | 175.11 ns | 2.38 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 223.56 ns | 233.37 ns | 222.93 ns | 9.80 ns | 4.38 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 337.14 ns | 351.94 ns | 334.69 ns | 5.54 ns | 1.64 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 441.40 ns | 460.65 ns | 435.15 ns | 13.33 ns | 3.02 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.32 ns | 0.33 ns | 0.32 ns | 0.00 ns | 1.57 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 45.00 ns | 45.97 ns | 45.07 ns | 0.89 ns | 1.99 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 388.27 ns | 396.89 ns | 383.99 ns | 9.56 ns | 2.46 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 600.97 ns | 614.30 ns | 607.24 ns | 9.44 ns | 1.57 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 620.31 ns | 634.03 ns | 608.07 ns | 24.46 ns | 3.94 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 177.22 ns | 181.16 ns | 177.58 ns | 2.27 ns | 1.28 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategy | 5 | 1 | 7.09 ns | 7.25 ns | 7.10 ns | 0.11 ns | 1.54 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 15.31 ns | 15.65 ns | 15.13 ns | 0.43 ns | 2.80 % |
| BM_Engine_StepFundingEventFullPipeline | 5 | 1 | 10.82 ns | 11.06 ns | 10.79 ns | 0.32 ns | 2.98 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_NRings<2> | 5 | 1 | 107.56 ns | 109.94 ns | 110.18 ns | 7.23 ns | 6.73 % |
| BM_BacktestInProcessTransport_NRings<4> | 5 | 1 | 214.86 ns | 219.53 ns | 216.29 ns | 10.36 ns | 4.82 % |
| BM_BacktestInProcessTransport_NRings<8> | 5 | 1 | 526.94 ns | 538.14 ns | 525.65 ns | 13.80 ns | 2.62 % |
| BM_BacktestInProcessTransport_NRings<16> | 5 | 1 | 1,318.27 ns | 1,346.32 ns | 1,326.92 ns | 41.87 ns | 3.18 % |
| BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff | 5 | 1 | 277.24 ns | 283.16 ns | 269.88 ns | 22.64 ns | 8.16 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 1.62 ns | 1.66 ns | 1.62 ns | 0.02 ns | 1.21 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 1.94 ns | 1.98 ns | 1.92 ns | 0.05 ns | 2.59 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.39 ns | 0.40 ns | 0.39 ns | 0.00 ns | 0.96 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.64 ns | 1.67 ns | 1.63 ns | 0.03 ns | 1.84 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.63 ns | 1.67 ns | 1.62 ns | 0.04 ns | 2.29 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 303.74 ns | 312.31 ns | 289.87 ns | 24.97 ns | 8.22 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 875.42 ns | 900.11 ns | 846.22 ns | 61.00 ns | 6.97 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 0.78 ns | 0.80 ns | 0.77 ns | 0.03 ns | 3.94 % |
| BM_LastTradeMatcher_OnMarketEventDiscardsPopulatedBookDiff | 5 | 1 | 0.35 ns | 0.36 ns | 0.34 ns | 0.03 ns | 8.15 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 0.98 ns | 1.01 ns | 1.00 ns | 0.04 ns | 3.92 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.67 ns | 0.69 ns | 0.67 ns | 0.02 ns | 3.32 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 2.66 ns | 2.74 ns | 2.62 ns | 0.07 ns | 2.74 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 2.07 ns | 2.13 ns | 2.08 ns | 0.08 ns | 3.71 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 32.06 ns | 32.95 ns | 31.86 ns | 1.53 ns | 4.78 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 40.71 ns | 41.86 ns | 39.64 ns | 2.40 ns | 5.88 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 59.39 ns | 119.82 ns | 60.10 ns | 4.32 ns | 7.27 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 43.25 ns | 221.34 ns | 44.92 ns | 2.78 ns | 6.42 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 34.92 ns | 35.95 ns | 34.08 ns | 7.63 ns | 21.86 % | 6,220,942.40 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 18,398,043.16 ns | 646,706.98 ns | 18,414,782.18 ns | 153,518.85 ns | 0.83 % | 0.00 | 1,549,703.48 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 336.91 ns | 344.16 ns | 338.76 ns | 5.17 ns | 1.54 % | 2,906,153,026.35 |
| BM_RunDataSource_FourPairs | 5 | 1 | 337.06 ns | 344.31 ns | 334.44 ns | 9.00 ns | 2.67 % | 11,623,877,828.48 |

## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.25 ns | 0.25 ns | 0.25 ns | 0.02 ns | 8.90 % |
| BM_SimClock_Now | 5 | 1 | 0.22 ns | 0.22 ns | 0.22 ns | 0.00 ns | 1.16 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 9.04 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.38 ns | 1.38 ns | 1.34 ns | 0.13 ns | 9.28 % |
| BM_Spsc_PopInt | 5 | 1 | 8.69 ns | 8.72 ns | 8.75 ns | 0.25 ns | 2.85 % |
| BM_Spsc_PushPopInt | 5 | 1 | 9.28 ns | 9.31 ns | 9.54 ns | 0.56 ns | 6.08 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 50.39 ns | 50.55 ns | 49.09 ns | 3.56 ns | 7.07 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 29.91 ns | 60.03 ns | 31.99 ns | 7.08 ns | 23.67 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.74 ns | 1.75 ns | 1.69 ns | 0.08 ns | 4.55 % |
| BM_Spmc_TryPopInt | 5 | 1 | 8.63 ns | 8.67 ns | 8.55 ns | 0.21 ns | 2.47 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 9.35 ns | 9.39 ns | 9.26 ns | 0.36 ns | 3.85 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 75.27 ns | 75.62 ns | 75.68 ns | 3.17 ns | 4.21 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 9.74 ns | 9.79 ns | 9.91 ns | 0.61 ns | 6.25 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 34.15 ns | 34.32 ns | 33.55 ns | 1.13 ns | 3.32 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 67.63 ns | 67.98 ns | 68.18 ns | 2.17 ns | 3.21 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 29.78 ns | 58.88 ns | 28.71 ns | 10.98 ns | 36.86 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 34.57 ns | 171.13 ns | 32.84 ns | 4.85 ns | 14.03 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 55.59 ns | 449.78 ns | 28.13 ns | 57.26 ns | 103.00 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 15.22 ns | 15.33 ns | 14.98 ns | 0.70 ns | 4.58 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 9.53 ns | 9.58 ns | 9.57 ns | 0.29 ns | 3.02 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 17.45 ns | 17.55 ns | 17.40 ns | 1.10 ns | 6.33 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 38.01 ns | 76.46 ns | 38.84 ns | 7.76 ns | 20.41 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 115.26 ns | 579.60 ns | 110.90 ns | 18.75 ns | 16.26 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 140.16 ns | 1,266.41 ns | 133.95 ns | 16.81 ns | 12.00 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 64.87 ns | 65.72 ns | 66.44 ns | 2.62 ns | 4.03 % |
| BM_ControlChannel_Poll | 5 | 1 | 60.15 ns | 61.00 ns | 59.97 ns | 2.93 ns | 4.88 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 18.73 ns | 18.84 ns | 18.73 ns | 0.63 ns | 3.36 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 3.45 ns | 3.47 ns | 3.37 ns | 0.15 ns | 4.36 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 3.40 ns | 3.34 ns | 3.38 ns | 0.11 ns | 3.11 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.85 ns | 0.84 ns | 0.84 ns | 0.04 ns | 4.32 % |
| BM_Portfolio_Position | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 4.23 % |
| BM_Portfolio_Equity | 5 | 1 | 368.99 ns | 363.79 ns | 368.48 ns | 8.95 ns | 2.42 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.68 ns | 1.66 ns | 1.68 ns | 0.08 ns | 4.67 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.82 ns | 1.80 ns | 1.83 ns | 0.08 ns | 4.52 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.66 ns | 1.65 ns | 1.65 ns | 0.05 ns | 3.19 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.78 ns | 1.77 ns | 1.80 ns | 0.10 ns | 5.48 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.53 ns | 0.52 ns | 0.53 ns | 0.01 ns | 2.85 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.67 ns | 0.67 ns | 0.67 ns | 0.02 ns | 3.65 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.02 ns | 4.17 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.43 ns | 0.42 ns | 0.42 ns | 0.01 ns | 1.95 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.43 ns | 0.42 ns | 0.43 ns | 0.01 ns | 2.17 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 857.57 ns | 857.55 ns | 847.02 ns | 32.37 ns | 3.77 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 56.66 ns | 56.63 ns | 57.15 ns | 2.06 ns | 3.64 % |
| BM_SymbolTable_Name | 5 | 1 | 0.75 ns | 0.75 ns | 0.77 ns | 0.08 ns | 10.30 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 1.25 ns | 1.25 ns | 1.25 ns | 0.02 ns | 1.95 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.42 ns | 0.42 ns | 0.41 ns | 0.02 ns | 3.99 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.21 ns | 0.22 ns | 0.21 ns | 0.01 ns | 6.38 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.00 ns | 1.92 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.20 ns | 0.02 ns | 7.19 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.00 ns | 1.63 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 31.52 ns | 31.61 ns | 31.21 ns | 0.54 ns | 1.70 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 31.61 ns | 31.72 ns | 31.58 ns | 0.82 ns | 2.59 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.65 ns | 1.66 ns | 1.63 ns | 0.06 ns | 3.56 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 8,660,241.65 ns | 8,694,837.50 ns | 6,521,036.62 ns | 4,150,187.32 ns | 47.92 % | 2,741,670.44 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 4,462,882.38 ns | 4,481,053.65 ns | 4,487,678.47 ns | 236,894.95 ns | 5.31 % | 11,182,930.63 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 4,762,468.64 ns | 4,776,666.31 ns | 4,799,943.93 ns | 329,088.30 ns | 6.91 % | 10,507,144.35 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 501.26 ns | 503.53 ns | 486.99 ns | 24.77 ns | 4.94 % | 1,989,820.14 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 2,234.19 ns | 2,244.98 ns | 2,121.03 ns | 194.62 ns | 8.71 % | 448,030.79 |
| BM_BinanceParser_AggTrade | 5 | 1 | 378.40 ns | 380.31 ns | 363.60 ns | 33.67 ns | 8.90 % | 2,645,234.98 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 59.61 ns | 59.93 ns | 59.54 ns | 0.90 ns | 1.51 % | 16,690,342.03 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 43.06 ns | 43.29 ns | 42.70 ns | 2.38 ns | 5.53 % | 23,152,908.22 |
| BM_Wire_WriteTrade | 5 | 1 | 33.53 ns | 33.71 ns | 33.13 ns | 1.39 ns | 4.15 % | 29,702,082.35 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 87.31 ns | 87.80 ns | 86.08 ns | 4.79 ns | 5.49 % | 11,416,791.69 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 126.29 ns | 127.01 ns | 125.37 ns | 6.25 ns | 4.95 % | 7,888,259.76 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 1,031.11 ns | 1,037.04 ns | 1,033.01 ns | 40.07 ns | 3.89 % | 965,458.52 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,508.46 ns | 1,517.08 ns | 1,527.36 ns | 60.06 ns | 3.98 % | 660,038.21 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,613.98 ns | 2,629.46 ns | 2,593.56 ns | 88.40 ns | 3.38 % | 380,647.82 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 168.72 ns | 169.92 ns | 169.06 ns | 4.48 ns | 2.65 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 26,555.84 ns | 26,715.94 ns | 25,669.33 ns | 2,499.91 ns | 9.41 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,683.69 ns | 2,699.20 ns | 2,641.19 ns | 206.20 ns | 7.68 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 28,469.95 ns | 28,636.17 ns | 28,431.18 ns | 1,236.73 ns | 4.34 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 4.75 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.21 ns | 0.21 ns | 0.20 ns | 0.01 ns | 3.05 % |
| BM_Partition_FormatDay | 5 | 1 | 144.89 ns | 145.75 ns | 143.64 ns | 4.02 ns | 2.78 % |
| BM_Partition_SegmentPath | 5 | 1 | 619.73 ns | 607.28 ns | 612.45 ns | 30.33 ns | 4.89 % |
| BM_Partition_ListSegments | 5 | 1 | 10,343.43 ns | 10,169.76 ns | 10,246.92 ns | 543.17 ns | 5.25 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 165.21 ns | 162.80 ns | 168.63 ns | 25.08 ns | 15.18 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 215.28 ns | 212.61 ns | 205.82 ns | 53.36 ns | 24.79 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 565.35 ns | 559.35 ns | 571.47 ns | 16.89 ns | 2.99 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.42 ns | 0.41 ns | 0.41 ns | 0.01 ns | 3.51 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 60.81 ns | 60.37 ns | 58.82 ns | 4.46 ns | 7.34 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 297.39 ns | 295.72 ns | 290.51 ns | 53.24 ns | 17.90 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 648.14 ns | 645.18 ns | 646.01 ns | 72.35 ns | 11.16 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 837.96 ns | 834.85 ns | 843.63 ns | 22.39 ns | 2.67 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 236.90 ns | 236.24 ns | 240.14 ns | 9.47 ns | 4.00 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategyOneWorker | 5 | 1 | 214.38 ns | 214.02 ns | 208.51 ns | 53.35 ns | 24.88 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 327.56 ns | 327.35 ns | 357.57 ns | 57.87 ns | 17.67 % |
| BM_Engine_StepTwoNoopStrategiesOneWorker | 5 | 1 | 244.76 ns | 244.81 ns | 239.43 ns | 50.60 ns | 20.67 % |
| BM_Engine_StepTwoNoopStrategiesTwoWorkers | 5 | 1 | 457.36 ns | 457.70 ns | 469.73 ns | 33.86 ns | 7.40 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 136.95 ns | 137.12 ns | 132.58 ns | 7.10 ns | 5.19 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 2.16 ns | 2.16 ns | 2.19 ns | 0.09 ns | 4.20 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.59 ns | 2.60 ns | 2.56 ns | 0.11 ns | 4.33 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.73 ns | 0.73 ns | 0.73 ns | 0.03 ns | 4.52 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.53 ns | 1.53 ns | 1.51 ns | 0.06 ns | 3.83 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.48 ns | 1.49 ns | 1.47 ns | 0.04 ns | 2.38 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 734.41 ns | 737.14 ns | 724.07 ns | 27.48 ns | 3.74 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 1,192.20 ns | 1,196.88 ns | 1,192.31 ns | 36.08 ns | 3.03 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 5.63 ns | 5.65 ns | 5.52 ns | 0.35 ns | 6.31 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 4.38 ns | 4.39 ns | 4.24 ns | 0.36 ns | 8.13 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 1.37 ns | 1.38 ns | 1.33 ns | 0.23 ns | 17.03 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitNextOutcome | 5 | 1 | 14.58 ns | 14.66 ns | 14.50 ns | 0.77 ns | 5.26 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 42.03 ns | 42.28 ns | 42.08 ns | 0.77 ns | 1.83 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 52.61 ns | 52.90 ns | 52.49 ns | 1.73 ns | 3.28 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 48.29 ns | 96.48 ns | 46.88 ns | 2.99 ns | 6.19 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 43.41 ns | 218.11 ns | 41.61 ns | 6.68 ns | 15.38 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 37.06 ns | 37.27 ns | 39.28 ns | 4.52 ns | 12.20 % | 4,065,820.60 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,703,495.44 ns | 630,524.13 ns | 19,642,299.88 ns | 852,749.30 ns | 4.33 % | 0.00 | 1,586,457.82 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 597.29 ns | 590.07 ns | 592.76 ns | 34.44 ns | 5.77 % | 1,699,039,446.18 |
| BM_RunDataSource_FourPairs | 5 | 1 | 461.51 ns | 456.61 ns | 481.34 ns | 31.47 ns | 6.82 % | 8,793,566,190.22 |

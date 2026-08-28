## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.18 ns | 0.18 ns | 0.18 ns | 0.01 ns | 6.73 % |
| BM_SimClock_Now | 5 | 1 | 0.22 ns | 0.22 ns | 0.22 ns | 0.01 ns | 3.19 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 1.93 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.38 ns | 1.38 ns | 1.35 ns | 0.11 ns | 8.06 % |
| BM_Spsc_PopInt | 5 | 1 | 8.88 ns | 8.92 ns | 8.95 ns | 0.39 ns | 4.42 % |
| BM_Spsc_PushPopInt | 5 | 1 | 8.96 ns | 8.99 ns | 8.81 ns | 0.62 ns | 6.91 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 49.75 ns | 49.99 ns | 47.17 ns | 4.06 ns | 8.16 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 42.55 ns | 85.54 ns | 44.35 ns | 13.76 ns | 32.33 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.75 ns | 1.77 ns | 1.73 ns | 0.08 ns | 4.54 % |
| BM_Spmc_TryPopInt | 5 | 1 | 8.92 ns | 8.97 ns | 8.98 ns | 0.41 ns | 4.61 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 9.42 ns | 9.47 ns | 9.20 ns | 0.49 ns | 5.18 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 73.50 ns | 73.94 ns | 73.82 ns | 4.49 ns | 6.11 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 9.37 ns | 9.43 ns | 9.24 ns | 0.69 ns | 7.38 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 34.09 ns | 34.30 ns | 34.65 ns | 1.65 ns | 4.85 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 71.87 ns | 72.32 ns | 70.51 ns | 10.73 ns | 14.94 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 34.39 ns | 68.27 ns | 39.14 ns | 11.84 ns | 34.44 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 31.31 ns | 157.19 ns | 31.32 ns | 1.17 ns | 3.74 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 44.50 ns | 359.67 ns | 27.53 ns | 36.85 ns | 82.80 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 14.87 ns | 14.99 ns | 14.88 ns | 0.57 ns | 3.85 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 9.26 ns | 9.31 ns | 9.09 ns | 0.28 ns | 3.02 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 17.47 ns | 17.58 ns | 17.05 ns | 0.92 ns | 5.29 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 28.54 ns | 57.43 ns | 26.94 ns | 7.36 ns | 25.78 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 112.42 ns | 565.25 ns | 111.95 ns | 13.92 ns | 12.38 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 148.18 ns | 1,290.88 ns | 138.92 ns | 31.01 ns | 20.93 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 63.76 ns | 64.31 ns | 63.90 ns | 1.17 ns | 1.84 % |
| BM_ControlChannel_Poll | 5 | 1 | 60.70 ns | 59.84 ns | 61.81 ns | 2.16 ns | 3.55 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 20.46 ns | 20.14 ns | 20.12 ns | 1.71 ns | 8.34 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 3.42 ns | 3.38 ns | 3.39 ns | 0.09 ns | 2.67 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 3.39 ns | 3.35 ns | 3.43 ns | 0.12 ns | 3.46 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.72 ns | 0.71 ns | 0.72 ns | 0.05 ns | 6.71 % |
| BM_Portfolio_Position | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 4.02 % |
| BM_Portfolio_Equity | 5 | 1 | 366.35 ns | 363.68 ns | 366.01 ns | 6.05 ns | 1.65 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.68 ns | 1.68 ns | 1.69 ns | 0.06 ns | 3.58 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.62 ns | 1.62 ns | 1.61 ns | 0.05 ns | 3.07 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.69 ns | 1.69 ns | 1.69 ns | 0.08 ns | 4.54 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.72 ns | 1.72 ns | 1.69 ns | 0.10 ns | 5.69 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.66 ns | 0.65 ns | 0.65 ns | 0.05 ns | 7.02 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.54 ns | 0.54 ns | 0.53 ns | 0.03 ns | 5.36 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.01 ns | 1.64 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.41 ns | 0.41 ns | 0.41 ns | 0.01 ns | 2.61 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.02 ns | 4.55 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 855.14 ns | 857.32 ns | 852.74 ns | 13.67 ns | 1.60 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 55.23 ns | 55.35 ns | 54.37 ns | 2.91 ns | 5.27 % |
| BM_SymbolTable_Name | 5 | 1 | 0.72 ns | 0.72 ns | 0.72 ns | 0.03 ns | 4.44 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 1.23 ns | 1.23 ns | 1.21 ns | 0.05 ns | 3.80 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.01 ns | 2.34 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.22 ns | 0.01 ns | 5.07 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.86 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.00 ns | 2.30 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.69 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 31.90 ns | 32.06 ns | 31.85 ns | 1.45 ns | 4.56 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 32.02 ns | 32.23 ns | 32.02 ns | 1.42 ns | 4.44 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.70 ns | 1.71 ns | 1.68 ns | 0.07 ns | 4.28 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 8,909,451.65 ns | 8,970,665.57 ns | 6,641,316.50 ns | 3,743,267.53 ns | 42.01 % | 2,533,533.01 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 4,407,713.76 ns | 4,438,307.46 ns | 4,218,408.72 ns | 413,547.17 ns | 9.38 % | 11,343,111.44 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 4,574,867.51 ns | 4,607,392.27 ns | 4,480,506.88 ns | 352,686.79 ns | 7.71 % | 10,903,248.59 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 524.53 ns | 528.38 ns | 514.92 ns | 29.13 ns | 5.55 % | 1,897,011.53 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 2,127.82 ns | 2,143.71 ns | 2,097.50 ns | 114.09 ns | 5.36 % | 467,535.74 |
| BM_BinanceParser_AggTrade | 5 | 1 | 363.70 ns | 366.43 ns | 361.93 ns | 20.99 ns | 5.77 % | 2,736,366.96 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 56.69 ns | 57.12 ns | 57.20 ns | 2.50 ns | 4.41 % | 17,534,008.20 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 43.19 ns | 43.50 ns | 42.30 ns | 2.27 ns | 5.26 % | 23,040,441.09 |
| BM_Wire_WriteTrade | 5 | 1 | 33.16 ns | 33.36 ns | 33.63 ns | 1.42 ns | 4.28 % | 30,022,260.76 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 85.61 ns | 86.09 ns | 82.38 ns | 7.64 ns | 8.93 % | 11,686,280.30 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 125.76 ns | 126.47 ns | 126.29 ns | 6.14 ns | 4.89 % | 7,922,262.25 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 1,002.08 ns | 1,007.78 ns | 1,009.62 ns | 29.80 ns | 2.97 % | 992,987.13 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,532.32 ns | 1,541.04 ns | 1,489.38 ns | 130.99 ns | 8.55 % | 652,535.49 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,706.92 ns | 2,722.67 ns | 2,731.14 ns | 79.27 ns | 2.93 % | 367,539.28 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 163.10 ns | 164.22 ns | 163.71 ns | 10.62 ns | 6.51 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 25,996.53 ns | 26,150.16 ns | 25,772.52 ns | 1,786.41 ns | 6.87 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,834.40 ns | 2,795.75 ns | 2,676.55 ns | 322.27 ns | 11.37 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 34,965.56 ns | 34,214.36 ns | 34,584.29 ns | 2,397.62 ns | 6.86 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.22 ns | 0.21 ns | 0.21 ns | 0.02 ns | 7.71 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.22 ns | 0.22 ns | 0.22 ns | 0.00 ns | 1.78 % |
| BM_Partition_FormatDay | 5 | 1 | 154.11 ns | 151.78 ns | 159.85 ns | 9.50 ns | 6.16 % |
| BM_Partition_SegmentPath | 5 | 1 | 671.38 ns | 662.81 ns | 673.44 ns | 42.05 ns | 6.26 % |
| BM_Partition_ListSegments | 5 | 1 | 11,187.45 ns | 11,067.25 ns | 11,281.98 ns | 988.40 ns | 8.83 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 174.49 ns | 172.76 ns | 154.53 ns | 58.72 ns | 33.65 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 318.14 ns | 315.49 ns | 350.45 ns | 86.84 ns | 27.30 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 598.81 ns | 594.68 ns | 584.33 ns | 34.97 ns | 5.84 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.44 ns | 0.43 ns | 0.44 ns | 0.01 ns | 3.06 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 62.95 ns | 62.64 ns | 63.52 ns | 2.68 ns | 4.25 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 388.77 ns | 387.42 ns | 435.45 ns | 112.54 ns | 28.95 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 686.11 ns | 684.30 ns | 697.20 ns | 72.28 ns | 10.53 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 897.95 ns | 896.29 ns | 903.41 ns | 65.41 ns | 7.28 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 247.02 ns | 246.77 ns | 245.55 ns | 6.73 ns | 2.72 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategyOneWorker | 5 | 1 | 277.47 ns | 277.47 ns | 289.65 ns | 89.57 ns | 32.28 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 395.93 ns | 396.19 ns | 406.47 ns | 32.40 ns | 8.18 % |
| BM_Engine_StepTwoNoopStrategiesOneWorker | 5 | 1 | 313.56 ns | 313.99 ns | 320.87 ns | 81.35 ns | 25.94 % |
| BM_Engine_StepTwoNoopStrategiesTwoWorkers | 5 | 1 | 517.46 ns | 518.49 ns | 542.23 ns | 57.57 ns | 11.12 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 147.21 ns | 147.56 ns | 144.12 ns | 10.92 ns | 7.42 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 2.29 ns | 2.30 ns | 2.32 ns | 0.11 ns | 5.00 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.69 ns | 2.69 ns | 2.70 ns | 0.09 ns | 3.50 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.70 ns | 0.70 ns | 0.70 ns | 0.01 ns | 1.53 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 2.29 ns | 2.30 ns | 2.27 ns | 0.16 ns | 6.85 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 2.24 ns | 2.25 ns | 2.21 ns | 0.12 ns | 5.38 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 374.84 ns | 376.59 ns | 375.91 ns | 5.74 ns | 1.53 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 1,134.09 ns | 1,139.86 ns | 1,145.27 ns | 50.96 ns | 4.49 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 5.98 ns | 6.01 ns | 6.03 ns | 0.32 ns | 5.31 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 4.29 ns | 4.31 ns | 4.27 ns | 0.28 ns | 6.61 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 1.29 ns | 1.30 ns | 1.28 ns | 0.08 ns | 5.90 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitNextOutcome | 5 | 1 | 14.65 ns | 14.73 ns | 14.46 ns | 0.63 ns | 4.32 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 40.86 ns | 41.12 ns | 40.88 ns | 2.34 ns | 5.72 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 55.12 ns | 55.45 ns | 55.29 ns | 3.04 ns | 5.51 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 77.82 ns | 155.46 ns | 59.94 ns | 30.70 ns | 39.46 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 50.60 ns | 254.00 ns | 51.13 ns | 5.58 ns | 11.03 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 37.48 ns | 37.72 ns | 38.32 ns | 9.35 ns | 24.94 % | 3,542,493.40 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,419,367.49 ns | 637,167.95 ns | 19,477,560.10 ns | 728,499.69 ns | 3.75 % | 0.00 | 1,573,975.08 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 572.44 ns | 567.27 ns | 579.18 ns | 18.85 ns | 3.29 % | 1,764,427,340.65 |
| BM_RunDataSource_FourPairs | 5 | 1 | 464.41 ns | 460.93 ns | 456.57 ns | 26.69 ns | 5.75 % | 8,699,999,877.75 |

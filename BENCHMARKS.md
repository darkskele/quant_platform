## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.01 ns | 3.84 % |
| BM_SimClock_Now | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 3.49 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 1.17 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.00 ns | 1.02 ns | 0.99 ns | 0.07 ns | 7.32 % |
| BM_Spsc_PopInt | 5 | 1 | 6.95 ns | 7.06 ns | 6.96 ns | 0.25 ns | 3.58 % |
| BM_Spsc_PushPopInt | 5 | 1 | 7.02 ns | 7.11 ns | 6.95 ns | 0.14 ns | 2.05 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 35.96 ns | 36.46 ns | 36.18 ns | 0.80 ns | 2.23 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 51.98 ns | 105.41 ns | 51.74 ns | 2.51 ns | 4.83 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.29 ns | 1.32 ns | 1.30 ns | 0.03 ns | 2.03 % |
| BM_Spmc_TryPopInt | 5 | 1 | 6.83 ns | 6.93 ns | 6.83 ns | 0.15 ns | 2.25 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 7.21 ns | 7.31 ns | 7.24 ns | 0.11 ns | 1.59 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 54.86 ns | 55.63 ns | 55.08 ns | 1.70 ns | 3.09 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 7.19 ns | 7.29 ns | 7.16 ns | 0.15 ns | 2.11 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 26.39 ns | 26.76 ns | 26.29 ns | 0.45 ns | 1.72 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 53.85 ns | 54.60 ns | 52.28 ns | 2.34 ns | 4.35 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 44.09 ns | 87.43 ns | 43.69 ns | 3.90 ns | 8.84 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 24.06 ns | 121.54 ns | 23.88 ns | 0.77 ns | 3.18 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 36.76 ns | 301.47 ns | 22.17 ns | 34.25 ns | 93.17 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 11.37 ns | 11.54 ns | 11.35 ns | 0.24 ns | 2.07 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 7.18 ns | 7.29 ns | 7.18 ns | 0.11 ns | 1.52 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 13.19 ns | 13.38 ns | 13.20 ns | 0.13 ns | 0.99 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 38.26 ns | 77.61 ns | 38.78 ns | 1.18 ns | 3.08 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 89.55 ns | 453.97 ns | 89.68 ns | 10.15 ns | 11.33 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 118.99 ns | 1,043.48 ns | 106.41 ns | 29.22 ns | 24.56 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 48.81 ns | 49.74 ns | 48.90 ns | 0.79 ns | 1.61 % |
| BM_ControlChannel_Poll | 5 | 1 | 45.19 ns | 46.09 ns | 45.15 ns | 0.57 ns | 1.26 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 14.78 ns | 15.00 ns | 14.60 ns | 0.46 ns | 3.15 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 2.69 ns | 2.73 ns | 2.68 ns | 0.10 ns | 3.87 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 2.59 ns | 2.63 ns | 2.58 ns | 0.02 ns | 0.91 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.54 ns | 0.54 ns | 0.52 ns | 0.03 ns | 5.89 % |
| BM_Portfolio_Position | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.00 ns | 2.06 % |
| BM_Portfolio_Equity | 5 | 1 | 296.89 ns | 301.25 ns | 295.44 ns | 5.96 ns | 2.01 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.34 ns | 0.34 ns | 0.33 ns | 0.01 ns | 3.11 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 685.17 ns | 696.27 ns | 681.73 ns | 12.05 ns | 1.76 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 44.08 ns | 44.73 ns | 44.19 ns | 0.70 ns | 1.59 % |
| BM_SymbolTable_Name | 5 | 1 | 0.61 ns | 0.62 ns | 0.60 ns | 0.02 ns | 3.05 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 0.97 ns | 0.99 ns | 0.98 ns | 0.01 ns | 1.11 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.33 ns | 0.33 ns | 0.33 ns | 0.00 ns | 1.31 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 0.57 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 2.83 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 2.63 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 2.54 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 23.94 ns | 24.27 ns | 24.02 ns | 0.33 ns | 1.38 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 24.84 ns | 25.19 ns | 24.84 ns | 0.69 ns | 2.76 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.30 ns | 1.32 ns | 1.29 ns | 0.03 ns | 2.36 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 6,676,061.12 ns | 6,769,662.68 ns | 5,552,502.47 ns | 2,642,205.65 ns | 39.58 % | 3,338,198.15 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 3,380,335.65 ns | 3,427,715.65 ns | 3,362,371.32 ns | 190,761.69 ns | 5.64 % | 14,623,668.67 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 3,413,453.94 ns | 3,461,255.07 ns | 3,418,807.53 ns | 71,665.93 ns | 2.10 % | 14,450,744.30 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 368.16 ns | 373.33 ns | 363.78 ns | 10.08 ns | 2.74 % | 2,680,191.64 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 1,575.79 ns | 1,597.90 ns | 1,570.09 ns | 34.84 ns | 2.21 % | 626,065.13 |
| BM_BinanceParser_AggTrade | 5 | 1 | 276.39 ns | 280.25 ns | 275.19 ns | 4.14 ns | 1.50 % | 3,568,862.40 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 42.55 ns | 43.15 ns | 42.78 ns | 0.74 ns | 1.75 % | 23,183,172.17 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 30.10 ns | 30.52 ns | 29.80 ns | 0.76 ns | 2.51 % | 32,783,150.87 |
| BM_Wire_WriteTrade | 5 | 1 | 25.09 ns | 25.45 ns | 24.94 ns | 0.37 ns | 1.46 % | 39,306,006.20 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 61.65 ns | 62.52 ns | 62.18 ns | 1.35 ns | 2.19 % | 16,000,887.03 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 92.34 ns | 93.63 ns | 91.75 ns | 1.44 ns | 1.56 % | 10,681,824.49 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 779.46 ns | 790.41 ns | 775.75 ns | 8.82 ns | 1.13 % | 1,265,291.24 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,157.50 ns | 1,173.77 ns | 1,162.72 ns | 27.54 ns | 2.38 % | 852,346.91 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 1,969.08 ns | 1,996.54 ns | 1,980.58 ns | 35.35 ns | 1.80 % | 500,995.26 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 131.97 ns | 133.93 ns | 129.72 ns | 4.09 ns | 3.10 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 18,803.29 ns | 19,067.09 ns | 18,650.69 ns | 447.08 ns | 2.38 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,041.53 ns | 2,070.14 ns | 2,045.50 ns | 21.09 ns | 1.03 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 21,173.29 ns | 21,470.46 ns | 21,188.93 ns | 275.83 ns | 1.30 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 2.36 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 0.76 % |
| BM_Partition_FormatDay | 5 | 1 | 112.99 ns | 114.65 ns | 112.94 ns | 1.34 ns | 1.19 % |
| BM_Partition_SegmentPath | 5 | 1 | 452.09 ns | 458.73 ns | 448.15 ns | 6.95 ns | 1.54 % |
| BM_Partition_ListSegments | 5 | 1 | 7,417.92 ns | 7,526.96 ns | 7,401.49 ns | 99.99 ns | 1.35 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 210.02 ns | 213.09 ns | 215.13 ns | 9.23 ns | 4.40 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 330.04 ns | 334.85 ns | 331.55 ns | 5.73 ns | 1.73 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 434.19 ns | 440.55 ns | 438.66 ns | 17.25 ns | 3.97 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.33 ns | 0.34 ns | 0.33 ns | 0.01 ns | 2.63 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 45.12 ns | 45.79 ns | 45.55 ns | 0.62 ns | 1.38 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 378.41 ns | 383.99 ns | 365.52 ns | 29.62 ns | 7.83 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 561.72 ns | 570.03 ns | 574.66 ns | 21.74 ns | 3.87 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 654.69 ns | 664.39 ns | 658.38 ns | 13.23 ns | 2.02 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 183.52 ns | 186.24 ns | 183.22 ns | 6.69 ns | 3.65 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategyOneWorker | 5 | 1 | 316.24 ns | 320.90 ns | 314.72 ns | 16.42 ns | 5.19 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 599.63 ns | 608.52 ns | 593.09 ns | 20.32 ns | 3.39 % |
| BM_Engine_StepTwoNoopStrategiesOneWorker | 5 | 1 | 383.04 ns | 388.72 ns | 382.32 ns | 7.40 ns | 1.93 % |
| BM_Engine_StepTwoNoopStrategiesTwoWorkers | 5 | 1 | 484.33 ns | 491.53 ns | 480.25 ns | 19.84 ns | 4.10 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 102.23 ns | 103.75 ns | 102.66 ns | 1.36 ns | 1.33 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 17.19 ns | 17.44 ns | 17.09 ns | 0.31 ns | 1.79 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 17.40 ns | 17.66 ns | 17.44 ns | 0.19 ns | 1.10 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.50 ns | 0.51 ns | 0.50 ns | 0.01 ns | 1.79 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.17 ns | 1.19 ns | 1.17 ns | 0.01 ns | 0.95 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.17 ns | 1.19 ns | 1.16 ns | 0.01 ns | 1.21 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 589.48 ns | 597.80 ns | 586.01 ns | 9.31 ns | 1.58 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 4.30 ns | 4.36 ns | 4.34 ns | 0.07 ns | 1.70 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 3.34 ns | 3.38 ns | 3.34 ns | 0.04 ns | 1.24 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 1.02 ns | 1.03 ns | 1.02 ns | 0.02 ns | 1.95 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitNextOutcome | 5 | 1 | 11.40 ns | 11.56 ns | 11.38 ns | 0.16 ns | 1.43 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 31.81 ns | 32.28 ns | 32.10 ns | 0.54 ns | 1.69 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 41.31 ns | 41.89 ns | 41.08 ns | 1.18 ns | 2.85 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 52.74 ns | 104.45 ns | 53.47 ns | 1.59 ns | 3.01 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 46.42 ns | 233.44 ns | 45.38 ns | 4.94 ns | 10.63 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 23.74 ns | 24.07 ns | 21.97 ns | 4.90 ns | 20.63 % | 6,955,798.80 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 18,974,765.93 ns | 610,218.22 ns | 19,099,940.03 ns | 1,081,350.08 ns | 5.70 % | 0.00 | 1,644,686.45 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 339.31 ns | 344.34 ns | 336.78 ns | 8.66 ns | 2.55 % | 2,905,599,686.02 |
| BM_RunDataSource_FourPairs | 5 | 1 | 366.95 ns | 372.41 ns | 364.86 ns | 6.28 ns | 1.71 % | 10,743,449,075.37 |

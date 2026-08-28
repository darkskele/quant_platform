## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.19 ns | 0.19 ns | 0.19 ns | 0.00 ns | 2.22 % |
| BM_SimClock_Now | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.72 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 8.43 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.26 ns | 1.27 ns | 1.28 ns | 0.05 ns | 3.78 % |
| BM_Spsc_PopInt | 5 | 1 | 8.57 ns | 8.63 ns | 8.57 ns | 0.37 ns | 4.37 % |
| BM_Spsc_PushPopInt | 5 | 1 | 9.20 ns | 9.16 ns | 9.26 ns | 0.14 ns | 1.55 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 53.51 ns | 52.72 ns | 51.35 ns | 4.45 ns | 8.32 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 41.37 ns | 81.69 ns | 44.84 ns | 8.66 ns | 20.94 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.84 ns | 1.83 ns | 1.78 ns | 0.21 ns | 11.49 % |
| BM_Spmc_TryPopInt | 5 | 1 | 9.13 ns | 9.05 ns | 8.85 ns | 0.61 ns | 6.65 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 9.52 ns | 9.44 ns | 9.53 ns | 0.38 ns | 3.97 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 76.83 ns | 76.29 ns | 76.19 ns | 2.56 ns | 3.34 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 9.94 ns | 9.88 ns | 9.91 ns | 0.40 ns | 4.06 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 36.75 ns | 36.56 ns | 36.65 ns | 1.40 ns | 3.80 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 70.32 ns | 70.03 ns | 69.64 ns | 2.49 ns | 3.54 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 28.09 ns | 54.64 ns | 28.71 ns | 6.57 ns | 23.40 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 33.57 ns | 166.20 ns | 33.63 ns | 0.55 ns | 1.64 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 43.00 ns | 358.28 ns | 33.66 ns | 26.84 ns | 62.41 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 16.25 ns | 16.25 ns | 16.15 ns | 1.15 ns | 7.08 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 10.00 ns | 9.97 ns | 10.01 ns | 0.23 ns | 2.31 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 18.33 ns | 18.33 ns | 18.19 ns | 1.05 ns | 5.74 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 41.66 ns | 83.34 ns | 36.07 ns | 9.41 ns | 22.58 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 115.65 ns | 578.32 ns | 116.12 ns | 16.00 ns | 13.84 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 127.24 ns | 1,133.78 ns | 124.69 ns | 20.05 ns | 15.76 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 65.45 ns | 65.59 ns | 65.33 ns | 1.90 ns | 2.91 % |
| BM_ControlChannel_Poll | 5 | 1 | 62.27 ns | 62.70 ns | 61.36 ns | 2.47 ns | 3.96 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 20.07 ns | 20.11 ns | 20.48 ns | 0.77 ns | 3.86 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 4.35 ns | 4.36 ns | 3.98 ns | 0.91 ns | 21.01 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 4.21 ns | 4.23 ns | 4.33 ns | 0.31 ns | 7.44 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.93 ns | 0.93 ns | 0.94 ns | 0.08 ns | 8.38 % |
| BM_Portfolio_Position | 5 | 1 | 0.22 ns | 0.22 ns | 0.22 ns | 0.01 ns | 4.03 % |
| BM_Portfolio_Equity | 5 | 1 | 378.77 ns | 380.02 ns | 374.43 ns | 21.47 ns | 5.67 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.47 ns | 0.47 ns | 0.46 ns | 0.02 ns | 4.98 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 891.43 ns | 897.50 ns | 882.39 ns | 36.66 ns | 4.11 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 60.43 ns | 60.65 ns | 60.78 ns | 2.45 ns | 4.06 % |
| BM_SymbolTable_Name | 5 | 1 | 0.75 ns | 0.75 ns | 0.74 ns | 0.02 ns | 2.76 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 1.31 ns | 1.31 ns | 1.31 ns | 0.01 ns | 0.71 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.43 ns | 0.43 ns | 0.42 ns | 0.02 ns | 4.78 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.22 ns | 0.22 ns | 0.22 ns | 0.01 ns | 3.85 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.21 ns | 0.22 ns | 0.21 ns | 0.01 ns | 6.07 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.66 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 5.46 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 32.82 ns | 32.97 ns | 32.71 ns | 1.93 ns | 5.88 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 32.40 ns | 32.55 ns | 33.22 ns | 1.46 ns | 4.52 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.77 ns | 1.78 ns | 1.76 ns | 0.11 ns | 6.26 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 9,267,201.96 ns | 9,309,172.68 ns | 6,450,541.10 ns | 4,176,058.55 ns | 45.06 % | 2,498,800.71 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 5,047,145.57 ns | 5,070,185.72 ns | 5,038,932.88 ns | 487,429.44 ns | 9.66 % | 9,935,266.85 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 5,199,857.14 ns | 5,212,188.62 ns | 5,029,496.57 ns | 483,820.00 ns | 9.30 % | 9,650,708.35 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 567.84 ns | 564.30 ns | 566.08 ns | 15.43 ns | 2.72 % | 1,773,593.42 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 2,252.78 ns | 2,219.41 ns | 2,278.06 ns | 69.62 ns | 3.09 % | 450,918.49 |
| BM_BinanceParser_AggTrade | 5 | 1 | 433.43 ns | 427.98 ns | 433.94 ns | 18.94 ns | 4.37 % | 2,340,018.53 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 59.22 ns | 58.57 ns | 58.27 ns | 3.38 ns | 5.71 % | 17,117,687.30 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 45.25 ns | 44.82 ns | 45.32 ns | 3.12 ns | 6.90 % | 22,402,610.89 |
| BM_Wire_WriteTrade | 5 | 1 | 35.38 ns | 35.08 ns | 35.11 ns | 0.88 ns | 2.50 % | 28,521,670.55 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 92.28 ns | 91.65 ns | 93.34 ns | 7.69 ns | 8.33 % | 10,972,289.33 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 132.27 ns | 131.42 ns | 132.88 ns | 10.70 ns | 8.09 % | 7,649,164.99 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 1,105.30 ns | 1,099.31 ns | 1,032.73 ns | 145.09 ns | 13.13 % | 921,169.85 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,797.53 ns | 1,789.97 ns | 1,782.58 ns | 47.15 ns | 2.62 % | 558,977.18 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,749.22 ns | 2,739.61 ns | 2,728.53 ns | 109.06 ns | 3.97 % | 365,461.13 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 182.43 ns | 182.25 ns | 181.12 ns | 12.52 ns | 6.86 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 27,057.31 ns | 26,998.47 ns | 26,745.27 ns | 2,137.87 ns | 7.90 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,795.54 ns | 2,791.88 ns | 2,750.62 ns | 221.27 ns | 7.91 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 29,786.06 ns | 29,761.04 ns | 30,219.44 ns | 1,547.78 ns | 5.20 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.00 ns | 2.15 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.22 ns | 0.22 ns | 0.22 ns | 0.02 ns | 6.88 % |
| BM_Partition_FormatDay | 5 | 1 | 157.67 ns | 157.77 ns | 155.04 ns | 6.74 ns | 4.28 % |
| BM_Partition_SegmentPath | 5 | 1 | 643.99 ns | 644.61 ns | 648.03 ns | 26.29 ns | 4.08 % |
| BM_Partition_ListSegments | 5 | 1 | 11,347.07 ns | 11,362.54 ns | 11,591.57 ns | 482.77 ns | 4.25 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 182.48 ns | 182.80 ns | 187.01 ns | 25.77 ns | 14.12 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 197.34 ns | 197.75 ns | 209.62 ns | 21.77 ns | 11.03 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 567.60 ns | 569.04 ns | 562.25 ns | 22.14 ns | 3.90 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.41 ns | 0.41 ns | 0.41 ns | 0.01 ns | 1.61 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 58.97 ns | 59.15 ns | 58.30 ns | 2.28 ns | 3.86 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 344.39 ns | 345.50 ns | 354.22 ns | 46.38 ns | 13.47 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 611.70 ns | 613.80 ns | 617.51 ns | 73.38 ns | 12.00 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 823.97 ns | 826.87 ns | 813.57 ns | 19.82 ns | 2.40 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 248.92 ns | 249.64 ns | 246.22 ns | 6.83 ns | 2.74 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategyOneWorker | 5 | 1 | 309.22 ns | 310.39 ns | 319.00 ns | 51.17 ns | 16.55 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 333.16 ns | 334.48 ns | 340.22 ns | 67.05 ns | 20.13 % |
| BM_Engine_StepTwoNoopStrategiesOneWorker | 5 | 1 | 221.56 ns | 222.47 ns | 228.57 ns | 45.84 ns | 20.69 % |
| BM_Engine_StepTwoNoopStrategiesTwoWorkers | 5 | 1 | 556.64 ns | 558.97 ns | 556.54 ns | 82.67 ns | 14.85 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 141.05 ns | 141.65 ns | 138.00 ns | 11.26 ns | 7.98 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 2.30 ns | 2.31 ns | 2.20 ns | 0.29 ns | 12.38 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.57 ns | 2.58 ns | 2.43 ns | 0.32 ns | 12.58 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.69 ns | 0.69 ns | 0.71 ns | 0.03 ns | 4.90 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.53 ns | 1.54 ns | 1.51 ns | 0.12 ns | 7.61 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.61 ns | 1.62 ns | 1.58 ns | 0.13 ns | 7.80 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 775.22 ns | 778.78 ns | 785.18 ns | 24.01 ns | 3.10 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 5.64 ns | 5.66 ns | 5.62 ns | 0.26 ns | 4.59 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 4.21 ns | 4.23 ns | 4.14 ns | 0.23 ns | 5.36 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 1.28 ns | 1.29 ns | 1.27 ns | 0.03 ns | 2.28 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitNextOutcome | 5 | 1 | 14.42 ns | 14.48 ns | 14.53 ns | 0.63 ns | 4.40 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 39.53 ns | 38.94 ns | 39.30 ns | 1.46 ns | 3.70 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 54.54 ns | 53.84 ns | 53.82 ns | 3.46 ns | 6.34 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 56.75 ns | 111.16 ns | 57.38 ns | 6.47 ns | 11.40 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 50.91 ns | 251.11 ns | 52.19 ns | 5.14 ns | 10.10 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 34.42 ns | 34.05 ns | 33.53 ns | 6.40 ns | 18.60 % | 3,332,457.60 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,997,211.44 ns | 610,945.53 ns | 19,989,044.34 ns | 906,711.39 ns | 4.53 % | 0.00 | 1,638,963.96 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 586.89 ns | 587.64 ns | 579.00 ns | 59.19 ns | 10.09 % | 1,715,606,592.50 |
| BM_RunDataSource_FourPairs | 5 | 1 | 465.27 ns | 466.07 ns | 465.64 ns | 11.72 ns | 2.52 % | 8,586,872,226.89 |

## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.18 ns | 0.18 ns | 0.18 ns | 0.01 ns | 4.37 % |
| BM_SimClock_Now | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 3.34 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 5.02 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.23 ns | 1.25 ns | 1.24 ns | 0.05 ns | 4.44 % |
| BM_Spsc_PopInt | 5 | 1 | 8.43 ns | 8.56 ns | 8.50 ns | 0.23 ns | 2.70 % |
| BM_Spsc_PushPopInt | 5 | 1 | 9.03 ns | 9.16 ns | 8.97 ns | 0.48 ns | 5.33 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 53.03 ns | 53.79 ns | 51.68 ns | 3.26 ns | 6.15 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 22.69 ns | 46.05 ns | 22.22 ns | 9.24 ns | 40.72 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.79 ns | 1.84 ns | 1.76 ns | 0.14 ns | 7.71 % |
| BM_Spmc_TryPopInt | 5 | 1 | 9.04 ns | 9.13 ns | 9.06 ns | 0.40 ns | 4.39 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 9.80 ns | 9.92 ns | 9.76 ns | 0.34 ns | 3.49 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 75.28 ns | 76.20 ns | 75.62 ns | 2.07 ns | 2.75 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 9.58 ns | 9.69 ns | 9.45 ns | 0.50 ns | 5.23 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 39.70 ns | 40.19 ns | 35.25 ns | 10.80 ns | 27.21 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 82.21 ns | 83.22 ns | 79.60 ns | 17.15 ns | 20.86 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 37.10 ns | 74.38 ns | 38.32 ns | 3.46 ns | 9.33 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 30.15 ns | 152.19 ns | 30.01 ns | 1.50 ns | 4.96 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 78.43 ns | 622.96 ns | 40.07 ns | 88.11 ns | 112.34 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 15.75 ns | 15.96 ns | 16.35 ns | 1.79 ns | 11.38 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 9.66 ns | 9.76 ns | 9.58 ns | 0.33 ns | 3.41 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 16.77 ns | 16.98 ns | 16.84 ns | 0.61 ns | 3.66 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 30.28 ns | 61.31 ns | 30.20 ns | 8.80 ns | 29.07 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 118.23 ns | 598.65 ns | 114.91 ns | 17.01 ns | 14.38 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 150.49 ns | 1,329.64 ns | 136.46 ns | 36.78 ns | 24.44 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 69.40 ns | 70.44 ns | 64.04 ns | 11.68 ns | 16.83 % |
| BM_ControlChannel_Poll | 5 | 1 | 61.06 ns | 62.35 ns | 61.67 ns | 1.23 ns | 2.01 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 19.58 ns | 19.83 ns | 19.04 ns | 1.31 ns | 6.71 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 3.46 ns | 3.51 ns | 3.42 ns | 0.19 ns | 5.48 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 3.37 ns | 3.41 ns | 3.31 ns | 0.11 ns | 3.13 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.70 ns | 0.71 ns | 0.71 ns | 0.03 ns | 3.95 % |
| BM_Portfolio_Position | 5 | 1 | 0.20 ns | 0.20 ns | 0.20 ns | 0.00 ns | 1.46 % |
| BM_Portfolio_Equity | 5 | 1 | 363.07 ns | 367.73 ns | 365.87 ns | 14.80 ns | 4.08 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.69 ns | 1.72 ns | 1.64 ns | 0.07 ns | 4.29 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.72 ns | 1.76 ns | 1.73 ns | 0.06 ns | 3.74 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.64 ns | 1.66 ns | 1.60 ns | 0.08 ns | 5.09 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.70 ns | 1.73 ns | 1.69 ns | 0.05 ns | 2.69 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.65 ns | 0.66 ns | 0.65 ns | 0.04 ns | 6.11 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.53 ns | 0.54 ns | 0.54 ns | 0.02 ns | 4.58 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.43 ns | 0.44 ns | 0.43 ns | 0.03 ns | 5.76 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.42 ns | 0.43 ns | 0.42 ns | 0.02 ns | 4.45 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.44 ns | 0.44 ns | 0.43 ns | 0.01 ns | 3.43 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 826.60 ns | 841.26 ns | 824.47 ns | 33.76 ns | 4.08 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 56.34 ns | 57.22 ns | 56.27 ns | 1.90 ns | 3.37 % |
| BM_SymbolTable_Name | 5 | 1 | 0.75 ns | 0.76 ns | 0.76 ns | 0.02 ns | 2.04 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 1.25 ns | 1.27 ns | 1.25 ns | 0.05 ns | 4.06 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.44 ns | 0.44 ns | 0.44 ns | 0.03 ns | 7.90 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.48 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 3.75 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.20 ns | 0.01 ns | 3.01 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 3.42 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 32.16 ns | 32.61 ns | 32.02 ns | 1.53 ns | 4.76 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 30.95 ns | 31.38 ns | 30.82 ns | 0.59 ns | 1.90 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.74 ns | 1.77 ns | 1.74 ns | 0.07 ns | 4.13 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 8,479,309.67 ns | 8,598,524.58 ns | 6,847,959.46 ns | 3,199,182.19 ns | 37.73 % | 2,598,801.08 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 4,410,236.98 ns | 4,466,959.89 ns | 4,280,795.17 ns | 317,163.02 ns | 7.19 % | 11,237,997.64 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 4,676,436.59 ns | 4,734,177.01 ns | 4,838,216.61 ns | 235,701.46 ns | 5.04 % | 10,583,443.05 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 518.93 ns | 525.36 ns | 511.64 ns | 18.18 ns | 3.50 % | 1,905,289.49 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 2,148.65 ns | 2,175.30 ns | 2,110.11 ns | 142.12 ns | 6.61 % | 461,301.45 |
| BM_BinanceParser_AggTrade | 5 | 1 | 360.76 ns | 365.23 ns | 349.68 ns | 19.98 ns | 5.54 % | 2,744,645.46 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 56.63 ns | 57.33 ns | 56.79 ns | 0.82 ns | 1.45 % | 17,445,537.91 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 42.42 ns | 42.94 ns | 41.91 ns | 1.29 ns | 3.04 % | 23,302,970.03 |
| BM_Wire_WriteTrade | 5 | 1 | 32.91 ns | 33.32 ns | 32.90 ns | 0.70 ns | 2.12 % | 30,021,503.01 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 91.16 ns | 92.29 ns | 89.98 ns | 6.37 ns | 6.99 % | 10,877,599.30 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 131.12 ns | 132.73 ns | 129.69 ns | 3.41 ns | 2.60 % | 7,537,793.17 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 1,274.59 ns | 1,290.42 ns | 1,017.65 ns | 610.71 ns | 47.91 % | 873,102.80 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,399.75 ns | 1,417.10 ns | 1,384.33 ns | 45.06 ns | 3.22 % | 706,248.40 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,726.63 ns | 2,761.41 ns | 2,736.61 ns | 159.81 ns | 5.86 % | 363,137.20 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 172.32 ns | 174.78 ns | 170.08 ns | 10.55 ns | 6.12 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 24,663.32 ns | 24,984.77 ns | 24,403.67 ns | 761.26 ns | 3.09 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,716.55 ns | 2,751.77 ns | 2,718.57 ns | 113.63 ns | 4.18 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 27,514.45 ns | 27,872.64 ns | 27,828.67 ns | 713.38 ns | 2.59 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.69 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.20 ns | 0.21 ns | 0.20 ns | 0.01 ns | 2.94 % |
| BM_Partition_FormatDay | 5 | 1 | 145.99 ns | 147.89 ns | 145.47 ns | 5.70 ns | 3.90 % |
| BM_Partition_SegmentPath | 5 | 1 | 577.71 ns | 585.20 ns | 584.63 ns | 16.60 ns | 2.87 % |
| BM_Partition_ListSegments | 5 | 1 | 9,790.87 ns | 9,917.74 ns | 9,722.56 ns | 515.04 ns | 5.26 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 139.15 ns | 140.94 ns | 144.99 ns | 39.09 ns | 28.09 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 211.22 ns | 213.97 ns | 237.60 ns | 83.94 ns | 39.74 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 543.70 ns | 551.72 ns | 542.84 ns | 12.92 ns | 2.38 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.41 ns | 0.42 ns | 0.40 ns | 0.02 ns | 5.40 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 57.02 ns | 57.97 ns | 57.51 ns | 1.18 ns | 2.07 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 322.19 ns | 327.55 ns | 319.40 ns | 24.89 ns | 7.73 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 572.70 ns | 582.27 ns | 574.51 ns | 45.32 ns | 7.91 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 824.65 ns | 837.61 ns | 806.66 ns | 30.41 ns | 3.69 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 226.23 ns | 230.00 ns | 224.08 ns | 3.90 ns | 1.72 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategyOneWorker | 5 | 1 | 232.70 ns | 236.57 ns | 229.94 ns | 44.03 ns | 18.92 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 272.30 ns | 276.83 ns | 291.01 ns | 68.68 ns | 25.22 % |
| BM_Engine_StepTwoNoopStrategiesOneWorker | 5 | 1 | 253.53 ns | 257.74 ns | 257.78 ns | 70.14 ns | 27.67 % |
| BM_Engine_StepTwoNoopStrategiesTwoWorkers | 5 | 1 | 457.02 ns | 463.71 ns | 453.64 ns | 10.79 ns | 2.36 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 132.77 ns | 134.68 ns | 129.56 ns | 8.98 ns | 6.76 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 2.57 ns | 2.61 ns | 2.58 ns | 0.05 ns | 1.77 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.60 ns | 2.63 ns | 2.58 ns | 0.10 ns | 3.91 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.53 ns | 0.54 ns | 0.54 ns | 0.01 ns | 2.78 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 2.51 ns | 2.54 ns | 2.51 ns | 0.12 ns | 4.64 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 2.53 ns | 2.57 ns | 2.55 ns | 0.11 ns | 4.26 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 372.87 ns | 378.25 ns | 370.83 ns | 9.75 ns | 2.61 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 1,045.03 ns | 1,060.10 ns | 1,040.10 ns | 40.84 ns | 3.91 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 6.46 ns | 6.56 ns | 6.50 ns | 0.20 ns | 3.09 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 1.26 ns | 1.28 ns | 1.26 ns | 0.03 ns | 2.49 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.85 ns | 0.86 ns | 0.82 ns | 0.08 ns | 9.06 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 3.13 ns | 3.17 ns | 2.91 ns | 0.39 ns | 12.36 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 2.53 ns | 2.57 ns | 2.52 ns | 0.08 ns | 3.30 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 43.05 ns | 43.80 ns | 43.93 ns | 2.92 ns | 6.78 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 54.25 ns | 54.99 ns | 54.17 ns | 2.22 ns | 4.09 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 54.89 ns | 110.76 ns | 56.23 ns | 5.33 ns | 9.70 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 49.19 ns | 248.61 ns | 49.29 ns | 4.09 ns | 8.31 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 34.60 ns | 35.07 ns | 33.71 ns | 3.54 ns | 10.23 % | 5,537,554.40 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,466,554.61 ns | 639,200.63 ns | 19,628,459.72 ns | 578,767.55 ns | 2.97 % | 0.00 | 1,564,961.21 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 457.50 ns | 463.80 ns | 451.73 ns | 19.45 ns | 4.25 % | 2,159,207,450.72 |
| BM_RunDataSource_FourPairs | 5 | 1 | 545.86 ns | 553.76 ns | 549.10 ns | 36.82 ns | 6.75 % | 7,249,720,528.66 |

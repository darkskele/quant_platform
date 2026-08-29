## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.21 ns | 0.20 ns | 0.21 ns | 0.02 ns | 9.49 % |
| BM_SimClock_Now | 5 | 1 | 0.21 ns | 0.20 ns | 0.21 ns | 0.00 ns | 1.89 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 10.95 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.25 ns | 1.24 ns | 1.25 ns | 0.04 ns | 2.93 % |
| BM_Spsc_PopInt | 5 | 1 | 8.40 ns | 8.40 ns | 8.34 ns | 0.31 ns | 3.66 % |
| BM_Spsc_PushPopInt | 5 | 1 | 8.73 ns | 8.74 ns | 8.55 ns | 0.36 ns | 4.12 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 50.23 ns | 50.32 ns | 47.87 ns | 8.09 ns | 16.10 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 30.30 ns | 60.79 ns | 25.64 ns | 13.67 ns | 45.11 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.81 ns | 1.83 ns | 1.74 ns | 0.18 ns | 10.22 % |
| BM_Spmc_TryPopInt | 5 | 1 | 8.87 ns | 8.88 ns | 8.98 ns | 0.26 ns | 2.97 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 9.35 ns | 9.30 ns | 9.35 ns | 0.20 ns | 2.14 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 74.29 ns | 74.50 ns | 75.01 ns | 3.13 ns | 4.22 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 9.23 ns | 9.27 ns | 9.20 ns | 0.21 ns | 2.24 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 34.07 ns | 34.23 ns | 34.10 ns | 0.79 ns | 2.33 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 65.80 ns | 66.14 ns | 66.29 ns | 1.16 ns | 1.76 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 28.36 ns | 56.51 ns | 27.94 ns | 3.21 ns | 11.31 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 30.84 ns | 154.41 ns | 30.87 ns | 0.73 ns | 2.37 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 155.95 ns | 1,199.75 ns | 85.88 ns | 172.32 ns | 110.50 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 14.98 ns | 15.09 ns | 14.81 ns | 0.63 ns | 4.22 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 9.15 ns | 9.21 ns | 9.16 ns | 0.24 ns | 2.64 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 16.95 ns | 17.07 ns | 16.88 ns | 0.83 ns | 4.88 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 30.44 ns | 61.34 ns | 28.86 ns | 5.93 ns | 19.49 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 110.66 ns | 557.19 ns | 113.32 ns | 9.33 ns | 8.43 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 143.01 ns | 1,289.33 ns | 138.13 ns | 19.01 ns | 13.29 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 62.75 ns | 64.18 ns | 60.56 ns | 3.61 ns | 5.76 % |
| BM_ControlChannel_Poll | 5 | 1 | 56.48 ns | 57.79 ns | 56.76 ns | 0.82 ns | 1.46 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 19.05 ns | 19.40 ns | 18.81 ns | 0.45 ns | 2.36 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 3.50 ns | 3.56 ns | 3.55 ns | 0.13 ns | 3.63 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 3.18 ns | 3.24 ns | 3.15 ns | 0.07 ns | 2.31 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.82 ns | 0.82 ns | 0.80 ns | 0.05 ns | 6.27 % |
| BM_Portfolio_Position | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 2.60 % |
| BM_Portfolio_Equity | 5 | 1 | 364.35 ns | 364.55 ns | 358.36 ns | 12.27 ns | 3.37 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.71 ns | 1.72 ns | 1.70 ns | 0.13 ns | 7.54 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.69 ns | 1.70 ns | 1.66 ns | 0.09 ns | 5.05 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.62 ns | 1.63 ns | 1.60 ns | 0.05 ns | 3.39 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 2.29 ns | 2.31 ns | 2.33 ns | 0.14 ns | 6.27 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.54 ns | 0.52 ns | 0.54 ns | 0.01 ns | 2.02 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.63 ns | 0.61 ns | 0.65 ns | 0.06 ns | 8.75 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.43 ns | 0.42 ns | 0.43 ns | 0.01 ns | 2.82 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.45 ns | 0.44 ns | 0.45 ns | 0.02 ns | 3.97 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.45 ns | 0.44 ns | 0.44 ns | 0.04 ns | 8.65 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 884.96 ns | 871.38 ns | 862.20 ns | 51.06 ns | 5.77 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 57.10 ns | 56.29 ns | 57.53 ns | 2.16 ns | 3.78 % |
| BM_SymbolTable_Name | 5 | 1 | 0.73 ns | 0.72 ns | 0.73 ns | 0.04 ns | 5.81 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 1.30 ns | 1.29 ns | 1.31 ns | 0.05 ns | 3.65 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.42 ns | 0.42 ns | 0.42 ns | 0.02 ns | 4.85 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 3.33 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.01 ns | 4.49 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.22 ns | 0.22 ns | 0.21 ns | 0.03 ns | 13.77 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.21 ns | 0.21 ns | 0.22 ns | 0.01 ns | 3.55 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 30.55 ns | 30.55 ns | 30.69 ns | 0.27 ns | 0.88 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 32.17 ns | 32.21 ns | 31.96 ns | 1.53 ns | 4.75 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.66 ns | 1.66 ns | 1.68 ns | 0.06 ns | 3.74 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 8,900,255.30 ns | 8,839,410.10 ns | 7,078,064.87 ns | 3,606,029.12 ns | 40.52 % | 2,564,382.67 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 4,171,283.39 ns | 4,146,754.39 ns | 4,153,717.91 ns | 196,695.88 ns | 4.72 % | 12,079,442.74 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 4,379,228.06 ns | 4,368,509.00 ns | 4,324,753.41 ns | 124,558.12 ns | 2.84 % | 11,452,566.16 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 495.70 ns | 501.12 ns | 497.64 ns | 9.76 ns | 1.97 % | 1,996,176.05 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 2,085.19 ns | 2,109.01 ns | 2,076.96 ns | 66.54 ns | 3.19 % | 474,540.88 |
| BM_BinanceParser_AggTrade | 5 | 1 | 357.01 ns | 361.33 ns | 351.58 ns | 17.23 ns | 4.83 % | 2,772,503.81 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 55.15 ns | 55.83 ns | 55.99 ns | 1.46 ns | 2.65 % | 17,921,661.45 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 38.47 ns | 38.96 ns | 37.12 ns | 2.73 ns | 7.10 % | 25,768,439.87 |
| BM_Wire_WriteTrade | 5 | 1 | 32.11 ns | 32.54 ns | 31.99 ns | 1.23 ns | 3.83 % | 30,764,793.33 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 83.57 ns | 84.55 ns | 83.19 ns | 2.53 ns | 3.02 % | 11,836,380.29 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 129.44 ns | 129.21 ns | 131.15 ns | 11.56 ns | 8.93 % | 7,788,685.39 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 1,063.78 ns | 1,062.23 ns | 1,040.92 ns | 62.33 ns | 5.86 % | 943,898.87 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,481.83 ns | 1,480.03 ns | 1,493.37 ns | 34.74 ns | 2.34 % | 675,964.59 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,718.67 ns | 2,716.03 ns | 2,714.27 ns | 100.94 ns | 3.71 % | 368,598.95 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 169.84 ns | 170.58 ns | 169.76 ns | 4.20 ns | 2.47 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 24,247.18 ns | 24,595.18 ns | 24,559.57 ns | 577.88 ns | 2.38 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,528.37 ns | 2,565.03 ns | 2,495.63 ns | 63.08 ns | 2.50 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 28,021.46 ns | 28,434.60 ns | 28,026.03 ns | 1,800.03 ns | 6.42 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.21 ns | 0.21 ns | 0.20 ns | 0.00 ns | 1.72 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.21 ns | 0.21 ns | 0.21 ns | 0.00 ns | 2.05 % |
| BM_Partition_FormatDay | 5 | 1 | 142.46 ns | 144.62 ns | 141.76 ns | 3.41 ns | 2.40 % |
| BM_Partition_SegmentPath | 5 | 1 | 583.16 ns | 586.48 ns | 588.24 ns | 18.48 ns | 3.17 % |
| BM_Partition_ListSegments | 5 | 1 | 10,032.95 ns | 10,036.62 ns | 9,950.64 ns | 321.07 ns | 3.20 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 189.84 ns | 189.94 ns | 195.07 ns | 41.33 ns | 21.77 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 236.06 ns | 236.21 ns | 226.17 ns | 48.75 ns | 20.65 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 560.70 ns | 563.45 ns | 558.49 ns | 25.58 ns | 4.56 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.42 ns | 0.42 ns | 0.41 ns | 0.01 ns | 2.09 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 59.72 ns | 60.33 ns | 59.22 ns | 2.09 ns | 3.49 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 295.56 ns | 298.62 ns | 310.18 ns | 68.91 ns | 23.32 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 669.85 ns | 651.00 ns | 650.68 ns | 54.30 ns | 8.11 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 863.89 ns | 837.98 ns | 868.91 ns | 14.57 ns | 1.69 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 310.47 ns | 302.40 ns | 260.82 ns | 109.73 ns | 35.34 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategyOneWorker | 5 | 1 | 320.45 ns | 312.24 ns | 322.35 ns | 53.59 ns | 16.72 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 326.34 ns | 317.60 ns | 362.50 ns | 72.16 ns | 22.11 % |
| BM_Engine_StepTwoNoopStrategiesOneWorker | 5 | 1 | 274.78 ns | 269.13 ns | 302.07 ns | 92.96 ns | 33.83 % |
| BM_Engine_StepTwoNoopStrategiesTwoWorkers | 5 | 1 | 510.66 ns | 509.55 ns | 512.80 ns | 36.81 ns | 7.21 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_TwoRings | 5 | 1 | 132.86 ns | 132.82 ns | 131.55 ns | 8.09 ns | 6.09 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 2.48 ns | 2.48 ns | 2.48 ns | 0.07 ns | 2.90 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.77 ns | 2.78 ns | 2.59 ns | 0.40 ns | 14.33 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.72 ns | 0.72 ns | 0.73 ns | 0.07 ns | 9.15 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 2.52 ns | 2.49 ns | 2.56 ns | 0.07 ns | 2.90 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 2.64 ns | 2.61 ns | 2.51 ns | 0.34 ns | 12.74 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 374.91 ns | 371.18 ns | 369.72 ns | 14.96 ns | 3.99 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 1,078.14 ns | 1,068.53 ns | 1,088.59 ns | 32.30 ns | 3.00 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 6.59 ns | 6.54 ns | 6.50 ns | 0.33 ns | 4.99 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 4.12 ns | 4.09 ns | 4.17 ns | 0.10 ns | 2.47 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.85 ns | 0.87 ns | 0.84 ns | 0.04 ns | 5.05 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 4.14 ns | 4.26 ns | 4.12 ns | 0.08 ns | 1.83 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 2.51 ns | 2.58 ns | 2.52 ns | 0.08 ns | 3.00 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 41.41 ns | 42.34 ns | 41.42 ns | 0.42 ns | 1.02 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 52.74 ns | 52.54 ns | 53.07 ns | 2.37 ns | 4.49 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 54.97 ns | 108.69 ns | 58.49 ns | 8.94 ns | 16.27 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 58.28 ns | 287.23 ns | 59.50 ns | 2.99 ns | 5.13 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 39.75 ns | 39.64 ns | 37.82 ns | 5.79 ns | 14.57 % | 3,593,247.80 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,803,548.82 ns | 633,537.31 ns | 19,867,831.04 ns | 844,011.19 ns | 4.26 % | 0.00 | 1,578,986.67 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 438.69 ns | 442.74 ns | 442.10 ns | 10.38 ns | 2.37 % | 2,259,676,080.63 |
| BM_RunDataSource_FourPairs | 5 | 1 | 433.73 ns | 436.79 ns | 442.69 ns | 13.98 ns | 3.22 % | 9,165,433,372.37 |

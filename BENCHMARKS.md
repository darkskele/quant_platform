## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.15 ns | 0.16 ns | 0.16 ns | 0.01 ns | 7.46 % |
| BM_SimClock_Now | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 4.87 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 2.65 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 0.99 ns | 1.01 ns | 0.96 ns | 0.04 ns | 4.00 % |
| BM_Spsc_PopInt | 5 | 1 | 6.68 ns | 6.87 ns | 6.75 ns | 0.25 ns | 3.80 % |
| BM_Spsc_PushPopInt | 5 | 1 | 6.92 ns | 7.11 ns | 6.87 ns | 0.17 ns | 2.51 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 36.77 ns | 37.75 ns | 37.12 ns | 1.25 ns | 3.39 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 46.83 ns | 96.14 ns | 45.87 ns | 3.35 ns | 7.15 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.29 ns | 1.32 ns | 1.27 ns | 0.03 ns | 2.45 % |
| BM_Spmc_TryPopInt | 5 | 1 | 6.67 ns | 6.85 ns | 6.66 ns | 0.04 ns | 0.58 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 7.10 ns | 7.29 ns | 7.03 ns | 0.12 ns | 1.65 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 55.10 ns | 56.58 ns | 54.80 ns | 0.74 ns | 1.34 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 7.06 ns | 7.25 ns | 7.08 ns | 0.12 ns | 1.73 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 26.02 ns | 26.72 ns | 26.09 ns | 0.31 ns | 1.21 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 51.56 ns | 52.95 ns | 51.35 ns | 0.91 ns | 1.77 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 38.31 ns | 77.56 ns | 37.87 ns | 1.84 ns | 4.80 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 23.91 ns | 122.13 ns | 24.03 ns | 0.49 ns | 2.05 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 19.43 ns | 177.36 ns | 19.10 ns | 2.81 ns | 14.49 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 11.25 ns | 11.56 ns | 11.26 ns | 0.15 ns | 1.36 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 7.02 ns | 7.20 ns | 6.99 ns | 0.11 ns | 1.58 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 13.06 ns | 13.41 ns | 13.12 ns | 0.17 ns | 1.29 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 38.17 ns | 78.39 ns | 39.43 ns | 7.39 ns | 19.37 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 82.47 ns | 423.17 ns | 85.76 ns | 9.71 ns | 11.78 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 94.45 ns | 858.34 ns | 90.02 ns | 19.22 ns | 20.35 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 0.75 ns | 0.77 ns | 0.74 ns | 0.06 ns | 8.56 % |
| BM_ControlChannel_Poll | 5 | 1 | 47.95 ns | 49.36 ns | 48.20 ns | 1.78 ns | 3.72 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 2.65 ns | 2.72 ns | 2.64 ns | 0.09 ns | 3.38 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 2.56 ns | 2.62 ns | 2.56 ns | 0.02 ns | 0.67 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.63 ns | 0.64 ns | 0.62 ns | 0.01 ns | 2.30 % |
| BM_Portfolio_Position | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 2.33 % |
| BM_Portfolio_Equity | 5 | 1 | 282.51 ns | 290.08 ns | 282.44 ns | 2.62 ns | 0.93 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.23 ns | 1.26 ns | 1.23 ns | 0.01 ns | 0.88 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.24 ns | 1.28 ns | 1.24 ns | 0.05 ns | 3.72 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.30 ns | 1.34 ns | 1.28 ns | 0.05 ns | 3.65 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.24 ns | 1.28 ns | 1.23 ns | 0.03 ns | 2.54 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.39 ns | 0.40 ns | 0.39 ns | 0.00 ns | 0.47 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.53 ns | 0.54 ns | 0.52 ns | 0.02 ns | 4.24 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.33 ns | 0.34 ns | 0.33 ns | 0.01 ns | 2.11 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.33 ns | 0.34 ns | 0.33 ns | 0.01 ns | 1.74 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 677.65 ns | 696.21 ns | 680.82 ns | 32.92 ns | 4.86 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 42.69 ns | 43.83 ns | 42.52 ns | 0.68 ns | 1.59 % |
| BM_SymbolTable_Name | 5 | 1 | 0.61 ns | 0.62 ns | 0.61 ns | 0.02 ns | 2.81 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 6,758,134.77 ns | 6,939,211.32 ns | 4,969,176.53 ns | 2,697,395.22 ns | 39.91 % | 3,243,236.51 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 3,467,482.90 ns | 3,560,326.28 ns | 3,380,922.50 ns | 207,959.70 ns | 6.00 % | 14,082,276.99 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 3,416,520.70 ns | 3,504,822.97 ns | 3,323,140.50 ns | 145,609.40 ns | 4.26 % | 14,284,943.11 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 43.13 ns | 44.28 ns | 43.04 ns | 0.85 ns | 1.96 % | 22,589,623.01 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 33.30 ns | 34.20 ns | 33.52 ns | 1.30 ns | 3.91 % | 29,278,854.66 |
| BM_Wire_WriteTrade | 5 | 1 | 26.13 ns | 26.83 ns | 26.19 ns | 0.48 ns | 1.84 % | 37,275,085.57 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 62.50 ns | 64.17 ns | 61.91 ns | 1.75 ns | 2.79 % | 15,592,300.51 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 96.49 ns | 99.07 ns | 94.92 ns | 2.53 ns | 2.63 % | 10,099,336.91 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 775.06 ns | 795.79 ns | 779.76 ns | 17.05 ns | 2.20 % | 1,257,096.41 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,189.50 ns | 1,221.36 ns | 1,194.35 ns | 29.55 ns | 2.48 % | 819,173.52 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 2,016.20 ns | 2,070.22 ns | 2,007.40 ns | 23.68 ns | 1.17 % | 483,092.95 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 130.10 ns | 133.76 ns | 129.23 ns | 3.78 ns | 2.91 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 19,501.66 ns | 20,026.95 ns | 19,488.18 ns | 795.48 ns | 4.08 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 2,066.03 ns | 2,121.43 ns | 2,051.52 ns | 51.41 ns | 2.49 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 21,147.15 ns | 21,713.69 ns | 21,099.03 ns | 352.26 ns | 1.67 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 0.58 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 3.07 % |
| BM_Partition_FormatDay | 5 | 1 | 108.72 ns | 111.64 ns | 109.75 ns | 1.52 ns | 1.39 % |
| BM_Partition_SegmentPath | 5 | 1 | 386.85 ns | 397.27 ns | 383.37 ns | 9.81 ns | 2.54 % |
| BM_Partition_ListSegments | 5 | 1 | 7,217.70 ns | 7,404.64 ns | 7,134.78 ns | 299.26 ns | 4.15 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 243.22 ns | 249.78 ns | 242.39 ns | 15.63 ns | 6.43 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 381.22 ns | 391.49 ns | 382.93 ns | 8.26 ns | 2.17 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 481.47 ns | 494.40 ns | 475.43 ns | 17.27 ns | 3.59 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.33 ns | 0.34 ns | 0.32 ns | 0.01 ns | 3.89 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 44.57 ns | 45.76 ns | 44.85 ns | 0.68 ns | 1.54 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 394.03 ns | 404.58 ns | 386.45 ns | 20.47 ns | 5.20 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 581.33 ns | 596.91 ns | 573.98 ns | 17.75 ns | 3.05 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 642.38 ns | 659.58 ns | 633.00 ns | 16.26 ns | 2.53 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 178.89 ns | 183.68 ns | 179.18 ns | 1.27 ns | 0.71 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategy | 5 | 1 | 7.16 ns | 7.36 ns | 7.16 ns | 0.21 ns | 2.89 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 15.20 ns | 15.60 ns | 15.24 ns | 0.12 ns | 0.77 % |
| BM_Engine_StepFundingEventFullPipeline | 5 | 1 | 10.78 ns | 11.06 ns | 10.80 ns | 0.12 ns | 1.16 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_NRings<2> | 5 | 1 | 100.58 ns | 103.28 ns | 101.90 ns | 3.47 ns | 3.45 % |
| BM_BacktestInProcessTransport_NRings<4> | 5 | 1 | 209.38 ns | 215.01 ns | 208.08 ns | 13.23 ns | 6.32 % |
| BM_BacktestInProcessTransport_NRings<8> | 5 | 1 | 533.45 ns | 547.76 ns | 530.00 ns | 18.31 ns | 3.43 % |
| BM_BacktestInProcessTransport_NRings<16> | 5 | 1 | 1,334.06 ns | 1,369.84 ns | 1,362.53 ns | 50.11 ns | 3.76 % |
| BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff | 5 | 1 | 280.60 ns | 288.13 ns | 292.67 ns | 21.14 ns | 7.53 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 1.61 ns | 1.65 ns | 1.59 ns | 0.04 ns | 2.56 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 1.95 ns | 2.00 ns | 1.95 ns | 0.02 ns | 1.16 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.40 ns | 0.41 ns | 0.40 ns | 0.00 ns | 0.70 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.64 ns | 1.69 ns | 1.64 ns | 0.05 ns | 2.83 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.66 ns | 1.71 ns | 1.67 ns | 0.04 ns | 2.28 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 292.06 ns | 299.89 ns | 290.46 ns | 4.87 ns | 1.67 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 806.22 ns | 827.82 ns | 811.92 ns | 25.36 ns | 3.15 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 0.66 ns | 0.68 ns | 0.66 ns | 0.06 ns | 8.63 % |
| BM_LastTradeMatcher_OnMarketEventDiscardsPopulatedBookDiff | 5 | 1 | 0.33 ns | 0.34 ns | 0.33 ns | 0.01 ns | 3.87 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 0.96 ns | 0.99 ns | 0.95 ns | 0.03 ns | 2.68 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.65 ns | 0.67 ns | 0.64 ns | 0.02 ns | 2.94 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 2.59 ns | 2.66 ns | 2.58 ns | 0.07 ns | 2.78 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 1.95 ns | 2.00 ns | 1.94 ns | 0.05 ns | 2.57 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 30.92 ns | 31.75 ns | 30.99 ns | 0.90 ns | 2.90 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 40.91 ns | 42.01 ns | 40.76 ns | 1.58 ns | 3.86 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 47.74 ns | 97.31 ns | 46.18 ns | 5.37 ns | 11.25 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 42.20 ns | 215.00 ns | 42.21 ns | 2.60 ns | 6.15 % |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 447.49 ns | 459.54 ns | 444.57 ns | 23.84 ns | 5.33 % | 2,180,966,412.16 |
| BM_RunDataSource_FourPairs | 5 | 1 | 340.94 ns | 350.12 ns | 335.74 ns | 19.34 ns | 5.67 % | 11,452,471,589.33 |

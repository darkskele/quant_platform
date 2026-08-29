## Full benchmark suite (repetitions=5)

### SimClock

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimClock_NowAdvance | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.39 % |
| BM_SimClock_Now | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 2.00 % |
| BM_SimClock_Advance | 5 | 1 | 0.00 ns | 0.00 ns | 0.00 ns | 0.00 ns | 1.67 % |

### Spsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushInt | 5 | 1 | 1.07 ns | 1.08 ns | 1.08 ns | 0.02 ns | 1.51 % |
| BM_Spsc_PopInt | 5 | 1 | 6.55 ns | 6.59 ns | 6.53 ns | 0.06 ns | 0.95 % |
| BM_Spsc_PushPopInt | 5 | 1 | 7.11 ns | 7.15 ns | 7.10 ns | 0.17 ns | 2.41 % |
| BM_Spsc_PushPopMarketEvent | 5 | 1 | 35.95 ns | 36.44 ns | 36.28 ns | 0.84 ns | 2.34 % |
| BM_Spsc_PushPopContended/threads:2 | 5 | 2 | 47.07 ns | 96.08 ns | 47.02 ns | 1.55 ns | 3.29 % |

### Spmc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushInt | 5 | 1 | 1.29 ns | 1.32 ns | 1.29 ns | 0.02 ns | 1.49 % |
| BM_Spmc_TryPopInt | 5 | 1 | 6.86 ns | 7.03 ns | 6.98 ns | 0.18 ns | 2.55 % |
| BM_Spmc_PushTryPopInt | 5 | 1 | 7.30 ns | 7.49 ns | 7.32 ns | 0.07 ns | 0.98 % |
| BM_Spmc_PushTryPopSharedMarketEvent | 5 | 1 | 54.72 ns | 56.15 ns | 54.97 ns | 1.32 ns | 2.41 % |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 5 | 1 | 7.06 ns | 7.25 ns | 7.04 ns | 0.08 ns | 1.19 % |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 5 | 1 | 26.29 ns | 26.98 ns | 26.16 ns | 0.66 ns | 2.52 % |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 5 | 1 | 52.06 ns | 53.42 ns | 52.06 ns | 0.92 ns | 1.77 % |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 5 | 2 | 36.31 ns | 73.30 ns | 36.62 ns | 1.07 ns | 2.95 % |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 5 | 5 | 23.13 ns | 118.48 ns | 23.51 ns | 1.27 ns | 5.51 % |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 5 | 9 | 297.38 ns | 2,200.05 ns | 416.57 ns | 242.53 ns | 81.55 % |

### Mpsc

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushInt | 5 | 1 | 11.44 ns | 11.75 ns | 11.26 ns | 0.37 ns | 3.23 % |
| BM_Mpsc_TryPopInt | 5 | 1 | 7.18 ns | 7.37 ns | 7.12 ns | 0.14 ns | 1.97 % |
| BM_Mpsc_PushPopInt | 5 | 1 | 13.01 ns | 13.36 ns | 12.99 ns | 0.29 ns | 2.20 % |
| BM_Mpsc_PushPopContended<1>/threads:2 | 5 | 2 | 33.25 ns | 68.97 ns | 34.78 ns | 9.29 ns | 27.93 % |
| BM_Mpsc_PushPopContended<4>/threads:5 | 5 | 5 | 89.63 ns | 464.58 ns | 89.58 ns | 7.16 ns | 7.99 % |
| BM_Mpsc_PushPopContended<8>/threads:9 | 5 | 9 | 114.00 ns | 1,057.57 ns | 98.52 ns | 27.98 ns | 24.55 % |

### ControlChannel

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ControlChannel_RequestStop | 5 | 1 | 47.13 ns | 49.16 ns | 47.02 ns | 0.41 ns | 0.86 % |
| BM_ControlChannel_Poll | 5 | 1 | 43.74 ns | 45.71 ns | 43.68 ns | 0.48 ns | 1.10 % |
| BM_ControlChannel_RequestPumpPoll | 5 | 1 | 14.49 ns | 15.03 ns | 14.40 ns | 0.29 ns | 2.03 % |

### Portfolio

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Portfolio_ApplyFill | 5 | 1 | 2.58 ns | 2.68 ns | 2.57 ns | 0.04 ns | 1.46 % |
| BM_Portfolio_ApplyFunding | 5 | 1 | 2.53 ns | 2.62 ns | 2.53 ns | 0.03 ns | 1.25 % |
| BM_Portfolio_ApplyMarkPrice | 5 | 1 | 0.62 ns | 0.64 ns | 0.61 ns | 0.01 ns | 1.63 % |
| BM_Portfolio_Position | 5 | 1 | 0.16 ns | 0.16 ns | 0.16 ns | 0.00 ns | 1.01 % |
| BM_Portfolio_Equity | 5 | 1 | 286.09 ns | 296.76 ns | 280.62 ns | 13.97 ns | 4.88 % |

### ViewablePool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ViewablePool_Push<false> | 5 | 1 | 1.26 ns | 1.30 ns | 1.26 ns | 0.03 ns | 2.11 % |
| BM_ViewablePool_Push<true> | 5 | 1 | 1.34 ns | 1.37 ns | 1.34 ns | 0.03 ns | 1.89 % |
| BM_ViewablePool_Emplace<false> | 5 | 1 | 1.27 ns | 1.28 ns | 1.28 ns | 0.03 ns | 2.63 % |
| BM_ViewablePool_Emplace<true> | 5 | 1 | 1.33 ns | 1.35 ns | 1.32 ns | 0.01 ns | 1.13 % |
| BM_ViewablePool_ViewAccess<false> | 5 | 1 | 0.40 ns | 0.40 ns | 0.39 ns | 0.01 ns | 2.38 % |
| BM_ViewablePool_ViewAccess<true> | 5 | 1 | 0.40 ns | 0.40 ns | 0.40 ns | 0.01 ns | 2.09 % |
| BM_ViewablePool_Reset<false> | 5 | 1 | 0.58 ns | 0.58 ns | 0.59 ns | 0.03 ns | 5.57 % |
| BM_ViewablePool_Reset<true> | 5 | 1 | 0.33 ns | 0.33 ns | 0.32 ns | 0.01 ns | 3.38 % |

### GapDetector

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_GapDetector_SteadyState | 5 | 1 | 0.33 ns | 0.34 ns | 0.33 ns | 0.01 ns | 2.35 % |

### SymbolTable

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SymbolTable_InternNew | 5 | 1 | 657.92 ns | 667.70 ns | 657.68 ns | 7.62 ns | 1.16 % |
| BM_SymbolTable_InternExisting | 5 | 1 | 43.04 ns | 43.63 ns | 43.06 ns | 0.56 ns | 1.31 % |
| BM_SymbolTable_Name | 5 | 1 | 0.50 ns | 0.50 ns | 0.51 ns | 0.01 ns | 1.78 % |

### ExponentialBackoff

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ExponentialBackoff_Next | 5 | 1 | 1.02 ns | 1.01 ns | 1.02 ns | 0.03 ns | 2.70 % |
| BM_ExponentialBackoff_Reset | 5 | 1 | 0.57 ns | 0.57 ns | 0.57 ns | 0.02 ns | 2.86 % |

### FuturesAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FuturesAlignment_Brackets | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.00 ns | 1.87 % |
| BM_FuturesAlignment_Continues | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.00 ns | 1.49 % |

### SpotAlignment

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SpotAlignment_Brackets | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.00 ns | 2.14 % |
| BM_SpotAlignment_Continues | 5 | 1 | 0.17 ns | 0.17 ns | 0.17 ns | 0.01 ns | 3.42 % |

### ResyncCoordinator

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ResyncCoordinator_OnEventForward | 5 | 1 | 25.06 ns | 24.81 ns | 24.74 ns | 0.98 ns | 3.92 % |
| BM_ResyncCoordinator_OnEventBuffering | 5 | 1 | 24.76 ns | 25.27 ns | 25.10 ns | 0.53 ns | 2.14 % |
| BM_ResyncCoordinator_FindResyncPoint | 5 | 1 | 1.29 ns | 1.32 ns | 1.30 ns | 0.02 ns | 1.40 % |

### FileReplaySource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileReplaySource_SingleSymbolBookDiffs | 5 | 1 | 7,664,580.78 ns | 7,822,583.26 ns | 9,035,774.60 ns | 2,491,491.20 ns | 32.51 % | 2,852,066.51 |
| BM_FileReplaySource_SingleSymbolTrades | 5 | 1 | 3,397,937.65 ns | 3,485,147.98 ns | 3,436,212.17 ns | 140,414.15 ns | 4.13 % | 14,369,245.15 |
| BM_FileReplaySource_MultiSymbolMerge | 5 | 1 | 3,390,151.70 ns | 3,484,859.34 ns | 3,339,474.82 ns | 162,870.48 ns | 4.80 % | 14,373,018.61 |

### BinanceParser

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BinanceParser_SmallDepthUpdate | 5 | 1 | 375.11 ns | 385.60 ns | 373.65 ns | 7.10 ns | 1.89 % | 2,594,123.27 |
| BM_BinanceParser_RealDepthUpdate | 5 | 1 | 1,537.08 ns | 1,580.03 ns | 1,536.96 ns | 2.45 ns | 0.16 % | 632,899.36 |
| BM_BinanceParser_AggTrade | 5 | 1 | 275.99 ns | 283.70 ns | 277.00 ns | 7.70 ns | 2.79 % | 3,527,040.10 |

### Wire

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteSmallBookDiff | 5 | 1 | 43.22 ns | 44.43 ns | 43.41 ns | 0.72 ns | 1.66 % | 22,512,249.70 |
| BM_Wire_WriteRealBookDiff | 5 | 1 | 32.60 ns | 33.51 ns | 32.26 ns | 0.70 ns | 2.16 % | 29,851,946.21 |
| BM_Wire_WriteTrade | 5 | 1 | 25.76 ns | 26.48 ns | 25.81 ns | 0.30 ns | 1.18 % | 37,770,339.63 |
| BM_Wire_ReadRealBookDiff | 5 | 1 | 61.51 ns | 63.23 ns | 60.79 ns | 1.25 ns | 2.03 % | 15,820,139.30 |
| BM_Wire_RoundTripRealBookDiff | 5 | 1 | 94.67 ns | 97.32 ns | 93.47 ns | 2.23 ns | 2.36 % | 10,280,124.56 |
| BM_Wire_WriteFullDepthSnapshot | 5 | 1 | 803.22 ns | 825.65 ns | 803.41 ns | 8.28 ns | 1.03 % | 1,211,264.94 |
| BM_Wire_ReadFullDepthSnapshot | 5 | 1 | 1,109.49 ns | 1,157.37 ns | 1,097.71 ns | 22.71 ns | 2.05 % | 864,604.34 |
| BM_Wire_RoundTripFullDepthSnapshot | 5 | 1 | 1,961.41 ns | 2,072.37 ns | 1,926.40 ns | 79.59 ns | 4.06 % | 483,149.99 |

### ZstdCompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdCompressor_Compress | 5 | 1 | 131.08 ns | 138.77 ns | 126.95 ns | 6.92 ns | 5.28 % |
| BM_ZstdCompressor_Finish | 5 | 1 | 18,483.20 ns | 19,528.00 ns | 18,289.95 ns | 531.30 ns | 2.87 % |

### ZstdDecompressor

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdDecompressor_Decompress | 5 | 1 | 1,971.41 ns | 2,082.88 ns | 1,990.50 ns | 39.80 ns | 2.02 % |

### ZstdStream

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_ZstdStream_RoundTrip | 5 | 1 | 20,573.93 ns | 21,737.93 ns | 20,439.55 ns | 618.43 ns | 3.01 % |

### Partition

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Partition_DayKeyFor | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 1.31 % |
| BM_Partition_NeedsRotation | 5 | 1 | 0.16 ns | 0.17 ns | 0.16 ns | 0.00 ns | 2.45 % |
| BM_Partition_FormatDay | 5 | 1 | 114.00 ns | 118.19 ns | 114.70 ns | 4.31 ns | 3.78 % |
| BM_Partition_SegmentPath | 5 | 1 | 447.75 ns | 456.98 ns | 444.58 ns | 14.68 ns | 3.28 % |
| BM_Partition_ListSegments | 5 | 1 | 7,472.91 ns | 7,626.73 ns | 7,465.47 ns | 149.24 ns | 2.00 % |

### RoundRobinPool

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RoundRobinPool_OneTaskOneWorker | 5 | 1 | 219.78 ns | 224.31 ns | 217.04 ns | 6.03 ns | 2.75 % |
| BM_RoundRobinPool_FourTasksOneWorker | 5 | 1 | 340.43 ns | 347.19 ns | 339.25 ns | 6.16 ns | 1.81 % |
| BM_RoundRobinPool_FourTasksFourWorkers | 5 | 1 | 435.08 ns | 443.49 ns | 434.64 ns | 15.21 ns | 3.50 % |
| BM_RoundRobinPool_FourTasksNoPool | 5 | 1 | 0.32 ns | 0.33 ns | 0.33 ns | 0.00 ns | 1.31 % |
| BM_RoundRobinPool_WorkTaskSolo | 5 | 1 | 45.07 ns | 45.94 ns | 45.33 ns | 0.42 ns | 0.94 % |
| BM_RoundRobinPool_WorkTaskOneWorker | 5 | 1 | 380.68 ns | 388.06 ns | 379.30 ns | 6.68 ns | 1.75 % |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 5 | 1 | 571.46 ns | 582.52 ns | 565.87 ns | 20.18 ns | 3.53 % |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 5 | 1 | 638.26 ns | 650.60 ns | 634.90 ns | 13.48 ns | 2.11 % |
| BM_RoundRobinPool_FourWorkTasksNoPool | 5 | 1 | 178.91 ns | 182.37 ns | 177.70 ns | 3.30 ns | 1.85 % |

### Engine

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_Engine_StepOneNoopStrategy | 5 | 1 | 0.64 ns | 0.66 ns | 0.64 ns | 0.01 ns | 1.16 % |
| BM_Engine_StepOneStrategyFullPipeline | 5 | 1 | 0.65 ns | 0.67 ns | 0.65 ns | 0.02 ns | 2.32 % |

### BacktestInProcessTransport

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BacktestInProcessTransport_NRings<2> | 5 | 1 | 100.31 ns | 102.25 ns | 100.60 ns | 0.68 ns | 0.67 % |
| BM_BacktestInProcessTransport_NRings<4> | 5 | 1 | 232.23 ns | 230.17 ns | 234.12 ns | 4.89 ns | 2.10 % |
| BM_BacktestInProcessTransport_NRings<8> | 5 | 1 | 535.37 ns | 529.33 ns | 532.06 ns | 6.94 ns | 1.30 % |
| BM_BacktestInProcessTransport_NRings<16> | 5 | 1 | 1,110.40 ns | 1,097.83 ns | 1,112.02 ns | 15.10 ns | 1.36 % |

### FundingCarryStrategy

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FundingCarryStrategy_EntersPosition | 5 | 1 | 2.00 ns | 1.97 ns | 2.00 ns | 0.05 ns | 2.29 % |
| BM_FundingCarryStrategy_HoldsPosition | 5 | 1 | 2.00 ns | 1.97 ns | 1.99 ns | 0.02 ns | 0.89 % |
| BM_FundingCarryStrategy_IgnoresNonMatchingEvent | 5 | 1 | 0.41 ns | 0.41 ns | 0.41 ns | 0.01 ns | 2.48 % |

### BasicRiskGate

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_BasicRiskGate_ApprovesWhenFlat | 5 | 1 | 1.63 ns | 1.64 ns | 1.61 ns | 0.06 ns | 3.45 % |
| BM_BasicRiskGate_ClampsAndResizes | 5 | 1 | 1.62 ns | 1.65 ns | 1.63 ns | 0.02 ns | 1.26 % |
| BM_BasicRiskGate_OnTickNoDrawdown | 5 | 1 | 292.29 ns | 298.31 ns | 293.84 ns | 6.07 ns | 2.08 % |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 5 | 1 | 808.92 ns | 825.58 ns | 802.39 ns | 13.00 ns | 1.61 % |

### LastTradeMatcher

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_LastTradeMatcher_OnMarketEvent | 5 | 1 | 4.85 ns | 4.95 ns | 4.85 ns | 0.04 ns | 0.81 % |
| BM_LastTradeMatcher_TryFillFills | 5 | 1 | 0.96 ns | 0.99 ns | 0.96 ns | 0.02 ns | 1.73 % |
| BM_LastTradeMatcher_TryFillRejects | 5 | 1 | 0.63 ns | 0.65 ns | 0.63 ns | 0.01 ns | 0.96 % |

### SimExecution

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_SimExecution_SubmitFills | 5 | 1 | 2.26 ns | 2.34 ns | 2.26 ns | 0.04 ns | 1.65 % |
| BM_SimExecution_SubmitRejects | 5 | 1 | 1.89 ns | 1.96 ns | 1.88 ns | 0.03 ns | 1.62 % |

### FanoutSink

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FanoutSink_Record | 5 | 1 | 30.63 ns | 31.73 ns | 30.38 ns | 0.88 ns | 2.86 % |
| BM_FanoutSink_RecordAndDrain | 5 | 1 | 38.89 ns | 40.31 ns | 39.18 ns | 0.52 ns | 1.33 % |
| BM_FanoutSink_RecordContended<1>/threads:2 | 5 | 2 | 50.44 ns | 103.38 ns | 50.23 ns | 2.39 ns | 4.73 % |
| BM_FanoutSink_RecordContended<4>/threads:5 | 5 | 5 | 39.82 ns | 205.02 ns | 41.41 ns | 5.96 ns | 14.97 % |

### FileRecorder

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | dropped | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_FileRecorder_Record | 5 | 1 | 37.45 ns | 38.81 ns | 36.02 ns | 8.93 ns | 23.86 % | 3,134,856.20 |  |
| BM_FileRecorder_RecordAndDrain | 5 | 1 | 19,335,086.20 ns | 659,393.42 ns | 19,695,534.34 ns | 826,264.15 ns | 4.27 % | 0.00 | 1,517,425.96 |

### RunDataSource

| Benchmark | Reps | Threads | Time | CPU | Median | StdDev | CV | items_per_second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BM_RunDataSource_OnePair | 5 | 1 | 340.22 ns | 349.33 ns | 339.37 ns | 9.53 ns | 2.80 % | 2,863,553,966.29 |
| BM_RunDataSource_FourPairs | 5 | 1 | 346.43 ns | 352.44 ns | 344.80 ns | 4.24 ns | 1.22 % | 11,350,676,517.10 |

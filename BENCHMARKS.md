## Benchmark diff (current vs. HEAD)

| Benchmark | Baseline | Current | Δ Time | Baseline CPU | Current CPU | Δ CPU |
| --- | --- | --- | --- | --- | --- | --- |
| BM_Spsc_PushPopContended/threads:2 | 21.68 ns | 35.83 ns | +65.3% | 43.47 ns | 70.73 ns | +62.7% |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 27.36 ns | 37.51 ns | +37.1% | 54.03 ns | 74.02 ns | +37.0% |
| BM_Mpsc_PushPopContended<1>/threads:2 | 37.53 ns | 49.68 ns | +32.4% | 74.56 ns | 100.06 ns | +34.2% |
| BM_BacktestInProcessTransport_NRings<2> | 139.48 ns | 131.49 ns | -5.7% | 139.77 ns | 133.49 ns | -4.5% |
| BM_BacktestInProcessTransport_NRings<16> | 2,036.27 ns | 1,912.76 ns | -6.1% | 2,041.64 ns | 1,942.20 ns | -4.9% |
| BM_FanoutSink_RecordContended<1>/threads:2 | 59.60 ns | 55.53 ns | -6.8% | 116.67 ns | 111.81 ns | -4.2% |
| BM_BacktestInProcessTransport_NRings<4> | 311.97 ns | 279.86 ns | -10.3% | 312.70 ns | 284.09 ns | -9.1% |
| BM_RunDataSource_FourPairs | 450.26 ns | 374.05 ns | -16.9% | 448.74 ns | 377.71 ns | -15.8% |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 30.66 ns | 25.38 ns | -17.2% | 153.10 ns | 126.77 ns | -17.2% |
| BM_Spmc_PushTryPopInt<false> | 9.30 ns | 7.69 ns | -17.3% | 9.32 ns | 7.65 ns | -17.9% |
| BM_Spmc_TryPopInt | 8.74 ns | 7.22 ns | -17.4% | 8.78 ns | 7.17 ns | -18.3% |
| BM_Spmc_PushTryPopMarketEvent<false> | 52.15 ns | 42.96 ns | -17.6% | 52.29 ns | 42.89 ns | -18.0% |
| BM_Spsc_PushPopInt<false> | 8.87 ns | 7.26 ns | -18.2% | 8.89 ns | 7.09 ns | -20.2% |
| BM_Spmc_PushTryPopInt<true> | 8.93 ns | 7.29 ns | -18.3% | 8.96 ns | 7.27 ns | -18.8% |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 9.09 ns | 7.39 ns | -18.7% | 9.11 ns | 7.40 ns | -18.8% |
| BM_Spsc_PopInt | 8.51 ns | 6.90 ns | -19.0% | 8.54 ns | 6.73 ns | -21.2% |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 33.78 ns | 27.34 ns | -19.1% | 33.88 ns | 27.39 ns | -19.1% |
| BM_Spmc_PushTryPopMarketEvent<true> | 53.18 ns | 42.28 ns | -20.5% | 53.33 ns | 42.25 ns | -20.8% |
| BM_ControlChannel_Poll | 63.80 ns | 49.92 ns | -21.7% | 64.16 ns | 50.44 ns | -21.4% |
| BM_FanoutSink_RecordContended<4>/threads:5 | 45.19 ns | 35.28 ns | -21.9% | 223.92 ns | 176.18 ns | -21.3% |
| BM_Spsc_PushPopMarketEvent<true> | 8.38 ns | 6.54 ns | -22.0% | 8.41 ns | 6.45 ns | -23.2% |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 68.27 ns | 53.11 ns | -22.2% | 68.47 ns | 53.26 ns | -22.2% |
| BM_Mpsc_PushInt | 15.64 ns | 12.16 ns | -22.2% | 15.69 ns | 12.23 ns | -22.1% |
| BM_BinHistSymbolTable_IdOfUnknown | 11.18 ns | 8.64 ns | -22.7% | 11.21 ns | 8.75 ns | -22.0% |
| BM_RunDataSource_OnePair | 463.06 ns | 351.63 ns | -24.1% | 461.21 ns | 355.08 ns | -23.0% |
| BM_Mpsc_TryPopInt | 9.78 ns | 7.41 ns | -24.2% | 9.72 ns | 7.45 ns | -23.4% |
| BM_BinHistSymbolTable_IdOfLastMatch | 17.34 ns | 13.13 ns | -24.3% | 17.39 ns | 13.29 ns | -23.6% |
| BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff | 246.77 ns | 185.43 ns | -24.9% | 247.43 ns | 188.28 ns | -23.9% |
| BM_BinHistParser_ParsesKline | 219.95 ns | 163.22 ns | -25.8% | 220.54 ns | 165.20 ns | -25.1% |
| BM_BinHistSymbolTable_IdOfFirstMatch | 18.24 ns | 13.45 ns | -26.3% | 18.29 ns | 13.61 ns | -25.6% |
| BM_Mpsc_PushPopInt | 18.68 ns | 13.72 ns | -26.6% | 18.54 ns | 13.81 ns | -25.5% |
| BM_FanoutSink_Record | 12.25 ns | 8.90 ns | -27.3% | 12.21 ns | 9.02 ns | -26.1% |
| BM_SymbolTable_InternExisting | 58.58 ns | 42.19 ns | -28.0% | 58.71 ns | 42.70 ns | -27.3% |
| BM_Engine_StepFundingEventFullPipeline | 7.85 ns | 5.61 ns | -28.5% | 7.86 ns | 5.70 ns | -27.5% |
| BM_Engine_StepOneStrategyFullPipeline | 8.22 ns | 5.85 ns | -28.8% | 8.23 ns | 5.94 ns | -27.8% |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 30.38 ns | 19.93 ns | -34.4% | 269.15 ns | 178.57 ns | -33.7% |
| BM_SymbolTable_InternNew | 1,002.15 ns | 652.92 ns | -34.8% | 1,005.30 ns | 662.17 ns | -34.1% |
| BM_Portfolio_ApplyFunding | 4.51 ns | 2.70 ns | -40.1% | 4.51 ns | 2.73 ns | -39.5% |
| BM_Mpsc_PushPopContended<4>/threads:5 | 124.46 ns | 65.04 ns | -47.7% | 618.37 ns | 327.65 ns | -47.0% |
| BM_Mpsc_PushPopContended<8>/threads:9 | 199.20 ns | 90.66 ns | -54.5% | 1,730.85 ns | 775.20 ns | -55.2% |
| BM_BinHistParser_ParsesFunding | 202.77 ns | 59.40 ns | -70.7% | 203.33 ns | 60.31 ns | -70.3% |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 1,094.60 ns | 39.56 ns | -96.4% | 1,097.79 ns | 40.04 ns | -96.4% |
| BM_BasicRiskGate_OnTickNoDrawdown | 413.61 ns | 2.98 ns | -99.3% | 414.79 ns | 3.02 ns | -99.3% |
| BM_Portfolio_Equity | 380.06 ns | 0.17 ns | -100.0% | 380.01 ns | 0.17 ns | -100.0% |

35 benchmark(s) unchanged (within noise floor: <1.5 ns or <5%).

**Added** (4): BM_BinHistParser_ParsesMark, BM_BinHistParser_RejectsUnknownSymbol, BM_CsvSource_MultiStreamMerge, BM_CsvSource_SingleStreamKlines

**Removed** (31): BM_BinHistParser_RejectsUnknownSymbolFunding, BM_BinHistParser_RejectsUnknownSymbolKline, BM_FileReplaySource_MultiSymbolMerge, BM_FileReplaySource_SingleSymbolBookDiffs, BM_FileReplaySource_SingleSymbolTrades, BM_Partition_DayKeyFor, BM_Partition_FormatDay, BM_Partition_ListSegments, BM_Partition_NeedsRotation, BM_Partition_SegmentPath, BM_RoundRobinPool_FourTasksFourWorkers, BM_RoundRobinPool_FourTasksNoPool, BM_RoundRobinPool_FourTasksOneWorker, BM_RoundRobinPool_FourWorkTasksFourWorkers, BM_RoundRobinPool_FourWorkTasksNoPool, BM_RoundRobinPool_FourWorkTasksOneWorker, BM_RoundRobinPool_OneTaskOneWorker, BM_RoundRobinPool_WorkTaskOneWorker, BM_RoundRobinPool_WorkTaskSolo, BM_Wire_ReadFullDepthSnapshot, BM_Wire_ReadRealBookDiff, BM_Wire_RoundTripFullDepthSnapshot, BM_Wire_RoundTripRealBookDiff, BM_Wire_WriteFullDepthSnapshot, BM_Wire_WriteRealBookDiff, BM_Wire_WriteSmallBookDiff, BM_Wire_WriteTrade, BM_ZstdCompressor_Compress, BM_ZstdCompressor_Finish, BM_ZstdDecompressor_Decompress, BM_ZstdStream_RoundTrip

## Benchmark diff (current vs. HEAD)

| Benchmark | Baseline | Current | Δ Time | Baseline CPU | Current CPU | Δ CPU |
| --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 23.99 ns | 31.69 ns | +32.1% | 122.66 ns | 155.52 ns | +26.8% |
| BM_Wire_ReadRealBookDiff | 50.23 ns | 64.39 ns | +28.2% | 51.86 ns | 64.94 ns | +25.2% |
| BM_FanoutSink_Record | 28.00 ns | 32.13 ns | +14.8% | 29.01 ns | 32.59 ns | +12.3% |
| BM_Spmc_PushTryPopSharedMarketEvent | 52.18 ns | 57.80 ns | +10.8% | 53.59 ns | 58.19 ns | +8.6% |
| BM_Mpsc_PushPopContended<8>/threads:9 | 92.93 ns | 101.23 ns | +8.9% | 858.69 ns | 906.04 ns | +5.5% |
| BM_FileReplaySource_SingleSymbolBookDiffs | 7,085,018.80 ns | 7,698,437.79 ns | +8.7% | 7,315,960.24 ns | 7,932,390.47 ns | +8.4% |
| BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff | 251.72 ns | 271.79 ns | +8.0% | 258.75 ns | 280.04 ns | +8.2% |
| BM_FileReplaySource_MultiSymbolMerge | 3,795,489.90 ns | 4,082,866.62 ns | +7.6% | 3,919,205.55 ns | 4,202,124.61 ns | +7.2% |
| BM_Mpsc_PushPopContended<1>/threads:2 | 33.25 ns | 35.64 ns | +7.2% | 68.30 ns | 74.31 ns | +8.8% |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 37.53 ns | 40.15 ns | +7.0% | 75.81 ns | 82.05 ns | +8.2% |
| BM_FileReplaySource_SingleSymbolTrades | 3,433,054.46 ns | 3,659,736.10 ns | +6.6% | 3,545,007.28 ns | 3,770,902.25 ns | +6.4% |
| BM_Partition_ListSegments | 6,583.23 ns | 7,008.95 ns | +6.5% | 6,779.05 ns | 7,318.57 ns | +8.0% |
| BM_Wire_RoundTripRealBookDiff | 89.07 ns | 94.76 ns | +6.4% | 91.93 ns | 95.58 ns | +4.0% |
| BM_BacktestInProcessTransport_NRings<16> | 1,288.46 ns | 1,358.64 ns | +5.4% | 1,306.84 ns | 1,399.88 ns | +7.1% |
| BM_Partition_FormatDay | 106.75 ns | 111.53 ns | +4.5% | 110.16 ns | 116.46 ns | +5.7% |
| BM_Wire_WriteTrade | 25.80 ns | 24.83 ns | -3.8% | 26.64 ns | 25.04 ns | -6.0% |
| BM_ZstdCompressor_Compress | 132.51 ns | 125.08 ns | -5.6% | 136.89 ns | 129.11 ns | -5.7% |
| BM_SymbolTable_InternExisting | 44.55 ns | 41.76 ns | -6.3% | 46.01 ns | 43.03 ns | -6.5% |
| BM_ControlChannel_Poll | 50.59 ns | 47.40 ns | -6.3% | 52.12 ns | 49.49 ns | -5.0% |
| BM_Wire_WriteSmallBookDiff | 42.04 ns | 39.37 ns | -6.3% | 43.41 ns | 40.26 ns | -7.2% |
| BM_RunDataSource_OnePair | 491.05 ns | 457.08 ns | -6.9% | 508.81 ns | 471.38 ns | -7.4% |
| BM_BasicRiskGate_OnTickNoDrawdown | 313.88 ns | 291.67 ns | -7.1% | 325.99 ns | 299.99 ns | -8.0% |
| BM_Wire_WriteRealBookDiff | 32.25 ns | 29.82 ns | -7.6% | 33.30 ns | 30.07 ns | -9.7% |
| BM_Spsc_PushPopMarketEvent | 39.30 ns | 35.18 ns | -10.5% | 40.36 ns | 36.24 ns | -10.2% |
| BM_RoundRobinPool_FourTasksOneWorker | 320.43 ns | 278.43 ns | -13.1% | 329.96 ns | 290.74 ns | -11.9% |
| BM_Mpsc_PushPopContended<4>/threads:5 | 102.42 ns | 76.67 ns | -25.1% | 525.82 ns | 399.71 ns | -24.0% |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 90.93 ns | 36.64 ns | -59.7% | 762.90 ns | 301.93 ns | -60.4% |

71 benchmark(s) unchanged (within noise floor: <1.5 ns or <5%).

**Added** (4): BM_BinHistSymbolTable_IdOfFirstMatch, BM_BinHistSymbolTable_IdOfLastMatch, BM_BinHistSymbolTable_IdOfUnknown, BM_BinHistSymbolTable_NameOf

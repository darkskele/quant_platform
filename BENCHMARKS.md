## Benchmark diff (current vs. HEAD)

| Benchmark | Baseline | Current | Δ Time | Baseline CPU | Current CPU | Δ CPU |
| --- | --- | --- | --- | --- | --- | --- |
| BM_Mpsc_PushPopContended<8>/threads:9 | 132.48 ns | 199.20 ns | +50.4% | 1,172.59 ns | 1,730.85 ns | +47.6% |
| BM_RoundRobinPool_WorkTaskSolo | 63.39 ns | 91.24 ns | +43.9% | 62.86 ns | 91.29 ns | +45.2% |
| BM_RoundRobinPool_OneTaskOneWorker | 167.73 ns | 234.57 ns | +39.8% | 166.44 ns | 234.37 ns | +40.8% |
| BM_ZstdStream_RoundTrip | 27,806.50 ns | 37,882.64 ns | +36.2% | 27,658.49 ns | 37,718.04 ns | +36.4% |
| BM_Partition_SegmentPath | 601.29 ns | 752.21 ns | +25.1% | 596.87 ns | 750.85 ns | +25.8% |
| BM_RoundRobinPool_FourTasksOneWorker | 253.05 ns | 305.86 ns | +20.9% | 251.07 ns | 305.72 ns | +21.8% |
| BM_SymbolTable_InternNew | 839.52 ns | 1,002.15 ns | +19.4% | 857.31 ns | 1,005.30 ns | +17.3% |
| BM_Partition_ListSegments | 10,713.67 ns | 12,355.28 ns | +15.3% | 10,633.09 ns | 12,338.20 ns | +16.0% |
| BM_Partition_FormatDay | 147.43 ns | 168.39 ns | +14.2% | 146.39 ns | 168.02 ns | +14.8% |
| BM_Wire_RoundTripRealBookDiff | 174.13 ns | 196.53 ns | +12.9% | 173.73 ns | 197.11 ns | +13.5% |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 667.98 ns | 746.18 ns | +11.7% | 664.42 ns | 746.83 ns | +12.4% |
| BM_RoundRobinPool_FourTasksFourWorkers | 562.24 ns | 619.70 ns | +10.2% | 557.58 ns | 619.62 ns | +11.1% |
| BM_BinHistParser_RejectsUnknownSymbolKline | 53.04 ns | 58.38 ns | +10.1% | 53.06 ns | 58.55 ns | +10.3% |
| BM_RoundRobinPool_WorkTaskOneWorker | 349.66 ns | 384.66 ns | +10.0% | 346.67 ns | 384.87 ns | +11.0% |
| BM_ZstdCompressor_Compress | 158.56 ns | 173.82 ns | +9.6% | 158.09 ns | 172.92 ns | +9.4% |
| BM_Wire_ReadRealBookDiff | 92.52 ns | 99.73 ns | +7.8% | 92.37 ns | 100.02 ns | +8.3% |
| BM_FileReplaySource_MultiSymbolMerge | 3,567,790.45 ns | 3,832,970.28 ns | +7.4% | 3,603,407.41 ns | 3,842,894.97 ns | +6.6% |
| BM_FileReplaySource_SingleSymbolTrades | 2,885,553.75 ns | 3,092,961.49 ns | +7.2% | 2,923,761.98 ns | 3,100,741.63 ns | +6.1% |
| BM_BasicRiskGate_OnTickNoDrawdown | 386.32 ns | 413.61 ns | +7.1% | 386.65 ns | 414.79 ns | +7.3% |
| BM_FileReplaySource_SingleSymbolBookDiffs | 7,013,474.47 ns | 7,448,660.30 ns | +6.2% | 7,130,250.87 ns | 7,466,544.61 ns | +4.7% |
| BM_RoundRobinPool_FourWorkTasksNoPool | 237.56 ns | 252.22 ns | +6.2% | 236.34 ns | 252.56 ns | +6.9% |
| BM_BacktestInProcessTransport_NRings<16> | 1,920.61 ns | 2,036.27 ns | +6.0% | 1,949.45 ns | 2,041.64 ns | +4.7% |
| BM_ZstdDecompressor_Decompress | 2,504.69 ns | 2,639.01 ns | +5.4% | 2,492.52 ns | 2,626.30 ns | +5.4% |
| BM_Wire_WriteFullDepthSnapshot | 1,021.88 ns | 1,076.15 ns | +5.3% | 1,019.30 ns | 1,065.80 ns | +4.6% |
| BM_ZstdCompressor_Finish | 24,942.97 ns | 26,260.87 ns | +5.3% | 24,829.03 ns | 26,113.58 ns | +5.2% |
| BM_Wire_ReadFullDepthSnapshot | 1,568.53 ns | 1,649.48 ns | +5.2% | 1,563.37 ns | 1,635.60 ns | +4.6% |
| BM_Wire_RoundTripFullDepthSnapshot | 2,761.46 ns | 2,900.58 ns | +5.0% | 2,751.22 ns | 2,878.69 ns | +4.6% |
| BM_SymbolTable_InternExisting | 55.78 ns | 58.58 ns | +5.0% | 56.66 ns | 58.71 ns | +3.6% |
| BM_BacktestInProcessTransport_NRings<2> | 133.39 ns | 139.48 ns | +4.6% | 132.66 ns | 139.77 ns | +5.4% |
| BM_Mpsc_PushPopContended<4>/threads:5 | 136.82 ns | 124.46 ns | -9.0% | 673.00 ns | 618.37 ns | -8.1% |
| BM_Mpsc_PushPopContended<1>/threads:2 | 41.63 ns | 37.53 ns | -9.9% | 81.96 ns | 74.56 ns | -9.0% |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 76.87 ns | 68.27 ns | -11.2% | 74.45 ns | 68.47 ns | -8.0% |
| BM_Spsc_PushPopContended/threads:2 | 25.58 ns | 21.68 ns | -15.3% | 50.86 ns | 43.47 ns | -14.5% |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 36.70 ns | 30.66 ns | -16.5% | 175.10 ns | 153.10 ns | -12.6% |
| BM_Spsc_PushPopInt<true> | 10.25 ns | 8.56 ns | -16.5% | 10.19 ns | 8.58 ns | -15.9% |
| BM_Spsc_PushPopMarketEvent<true> | 10.07 ns | 8.38 ns | -16.7% | 10.01 ns | 8.41 ns | -16.0% |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 34.46 ns | 27.36 ns | -20.6% | 65.59 ns | 54.03 ns | -17.6% |
| BM_RunDataSource_OnePair | 587.72 ns | 463.06 ns | -21.2% | 585.45 ns | 461.21 ns | -21.2% |
| BM_BinHistParser_ParsesFunding | 398.85 ns | 202.77 ns | -49.2% | 400.05 ns | 203.33 ns | -49.2% |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 214.30 ns | 30.38 ns | -85.8% | 1,529.44 ns | 269.15 ns | -82.4% |
| BM_BinHistParser_RejectsUnknownSymbolFunding | 297.17 ns | 9.60 ns | -96.8% | 297.21 ns | 9.63 ns | -96.8% |

69 benchmark(s) unchanged (within noise floor: <1.5 ns or <5%).

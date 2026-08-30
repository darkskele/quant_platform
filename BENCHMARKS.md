## Benchmark diff (current vs. HEAD)

| Benchmark | Baseline | Current | Δ Time | Baseline CPU | Current CPU | Δ CPU |
| --- | --- | --- | --- | --- | --- | --- |
| BM_Spmc_PushTryPopContended<8>/threads:9 | 36.37 ns | 214.30 ns | +489.2% | 312.77 ns | 1,529.44 ns | +389.0% |
| BM_RunDataSource_OnePair | 458.26 ns | 587.72 ns | +28.3% | 449.22 ns | 585.45 ns | +30.3% |
| BM_Mpsc_PushPopContended<1>/threads:2 | 34.23 ns | 41.63 ns | +21.6% | 67.10 ns | 81.96 ns | +22.1% |
| BM_FanoutSink_RecordContended<1>/threads:2 | 52.87 ns | 60.60 ns | +14.6% | 102.75 ns | 119.79 ns | +16.6% |
| BM_Spmc_PushTryPopContended<4>/threads:5 | 32.25 ns | 36.70 ns | +13.8% | 157.76 ns | 175.10 ns | +11.0% |
| BM_RoundRobinPool_FourTasksOneWorker | 224.41 ns | 253.05 ns | +12.8% | 219.95 ns | 251.07 ns | +14.1% |
| BM_Mpsc_PushInt | 14.73 ns | 16.45 ns | +11.7% | 14.45 ns | 15.97 ns | +10.5% |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 69.16 ns | 76.87 ns | +11.1% | 67.80 ns | 74.45 ns | +9.8% |
| BM_BacktestInProcessTransport_NRings<2> | 126.89 ns | 133.39 ns | +5.1% | 124.38 ns | 132.66 ns | +6.7% |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 640.75 ns | 667.98 ns | +4.2% | 628.05 ns | 664.42 ns | +5.8% |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 1,084.27 ns | 1,123.29 ns | +3.6% | 1,063.05 ns | 1,125.66 ns | +5.9% |
| BM_SymbolTable_InternNew | 890.44 ns | 839.52 ns | -5.7% | 874.15 ns | 857.31 ns | -1.9% |
| BM_Wire_WriteFullDepthSnapshot | 1,085.35 ns | 1,021.88 ns | -5.8% | 1,063.95 ns | 1,019.30 ns | -4.2% |
| BM_BacktestInProcessTransport_NRings<16> | 2,048.50 ns | 1,920.61 ns | -6.2% | 2,008.33 ns | 1,949.45 ns | -2.9% |
| BM_RoundRobinPool_FourTasksFourWorkers | 600.25 ns | 562.24 ns | -6.3% | 588.35 ns | 557.58 ns | -5.2% |
| BM_ZstdDecompressor_Decompress | 2,691.25 ns | 2,504.69 ns | -6.9% | 2,637.93 ns | 2,492.52 ns | -5.5% |
| BM_BacktestInProcessTransport_NRings<8> | 789.14 ns | 732.43 ns | -7.2% | 773.68 ns | 746.53 ns | -3.5% |
| BM_BacktestInProcessTransport_NRings<4> | 330.14 ns | 306.38 ns | -7.2% | 323.65 ns | 315.55 ns | -2.5% |
| BM_Wire_ReadFullDepthSnapshot | 1,698.87 ns | 1,568.53 ns | -7.7% | 1,665.41 ns | 1,563.37 ns | -6.1% |
| BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff | 256.61 ns | 236.61 ns | -7.8% | 251.58 ns | 238.86 ns | -5.1% |
| BM_Partition_FormatDay | 162.24 ns | 147.43 ns | -9.1% | 159.03 ns | 146.39 ns | -7.9% |
| BM_Partition_ListSegments | 11,792.32 ns | 10,713.67 ns | -9.1% | 11,558.27 ns | 10,633.09 ns | -8.0% |
| BM_BinHistSymbolTable_IdOfLastMatch | 18.19 ns | 16.38 ns | -9.9% | 17.84 ns | 16.51 ns | -7.5% |
| BM_SymbolTable_InternExisting | 62.52 ns | 55.78 ns | -10.8% | 61.29 ns | 56.66 ns | -7.6% |
| BM_Wire_ReadRealBookDiff | 104.87 ns | 92.52 ns | -11.8% | 102.80 ns | 92.37 ns | -10.2% |
| BM_FileReplaySource_MultiSymbolMerge | 4,066,715.97 ns | 3,567,790.45 ns | -12.3% | 3,985,862.40 ns | 3,603,407.41 ns | -9.6% |
| BM_FileReplaySource_SingleSymbolTrades | 3,291,573.56 ns | 2,885,553.75 ns | -12.3% | 3,226,936.97 ns | 2,923,761.98 ns | -9.4% |
| BM_Partition_SegmentPath | 688.71 ns | 601.29 ns | -12.7% | 675.08 ns | 596.87 ns | -11.6% |
| BM_FileReplaySource_SingleSymbolBookDiffs | 8,036,456.15 ns | 7,013,474.47 ns | -12.7% | 7,878,920.87 ns | 7,130,250.87 ns | -9.5% |
| BM_ZstdCompressor_Compress | 181.82 ns | 158.56 ns | -12.8% | 178.51 ns | 158.09 ns | -11.4% |
| BM_ZstdCompressor_Finish | 30,725.13 ns | 24,942.97 ns | -18.8% | 30,118.80 ns | 24,829.03 ns | -17.6% |
| BM_ZstdStream_RoundTrip | 34,470.18 ns | 27,806.50 ns | -19.3% | 33,788.88 ns | 27,658.49 ns | -18.1% |
| BM_Mpsc_PushPopContended<8>/threads:9 | 174.62 ns | 132.48 ns | -24.1% | 1,488.76 ns | 1,172.59 ns | -21.2% |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 47.03 ns | 34.46 ns | -26.7% | 90.84 ns | 65.59 ns | -27.8% |
| BM_Spsc_PushPopContended/threads:2 | 38.55 ns | 25.58 ns | -33.6% | 75.57 ns | 50.86 ns | -32.7% |

71 benchmark(s) unchanged (within noise floor: <1.5 ns or <5%).

**Added** (4): BM_BinHistParser_ParsesFunding, BM_BinHistParser_ParsesKline, BM_BinHistParser_RejectsUnknownSymbolFunding, BM_BinHistParser_RejectsUnknownSymbolKline

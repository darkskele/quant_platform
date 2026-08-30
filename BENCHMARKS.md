## Benchmark diff (current vs. HEAD)

| Benchmark | Baseline | Current | Δ Time | Baseline CPU | Current CPU | Δ CPU |
| --- | --- | --- | --- | --- | --- | --- |
| BM_Wire_WriteRealBookDiff | 29.82 ns | 84.39 ns | +183.0% | 30.07 ns | 82.73 ns | +175.1% |
| BM_Wire_RoundTripRealBookDiff | 94.76 ns | 176.67 ns | +86.4% | 95.58 ns | 173.20 ns | +81.2% |
| BM_Partition_SegmentPath | 385.64 ns | 688.71 ns | +78.6% | 402.68 ns | 675.08 ns | +67.6% |
| BM_Mpsc_PushPopContended<8>/threads:9 | 101.23 ns | 174.62 ns | +72.5% | 906.04 ns | 1,488.76 ns | +64.3% |
| BM_Mpsc_PushPopContended<4>/threads:5 | 76.67 ns | 132.10 ns | +72.3% | 399.71 ns | 647.29 ns | +61.9% |
| BM_Partition_ListSegments | 7,008.95 ns | 11,792.32 ns | +68.2% | 7,318.57 ns | 11,558.27 ns | +57.9% |
| BM_Wire_WriteSmallBookDiff | 39.37 ns | 65.39 ns | +66.1% | 40.26 ns | 64.11 ns | +59.2% |
| BM_Wire_ReadRealBookDiff | 64.39 ns | 104.87 ns | +62.9% | 64.94 ns | 102.80 ns | +58.3% |
| BM_ZstdStream_RoundTrip | 21,264.88 ns | 34,470.18 ns | +62.1% | 21,946.06 ns | 33,788.88 ns | +54.0% |
| BM_ZstdCompressor_Finish | 19,048.64 ns | 30,725.13 ns | +61.3% | 19,643.45 ns | 30,118.80 ns | +53.3% |
| BM_BacktestInProcessTransport_NRings<4> | 214.83 ns | 330.14 ns | +53.7% | 221.64 ns | 323.65 ns | +46.0% |
| BM_BacktestInProcessTransport_NRings<8> | 517.83 ns | 789.14 ns | +52.4% | 533.55 ns | 773.68 ns | +45.0% |
| BM_BacktestInProcessTransport_NRings<16> | 1,358.64 ns | 2,048.50 ns | +50.8% | 1,399.88 ns | 2,008.33 ns | +43.5% |
| BM_SymbolTable_InternExisting | 41.76 ns | 62.52 ns | +49.7% | 43.03 ns | 61.29 ns | +42.4% |
| BM_Wire_ReadFullDepthSnapshot | 1,146.82 ns | 1,698.87 ns | +48.1% | 1,160.35 ns | 1,665.41 ns | +43.5% |
| BM_Partition_FormatDay | 111.53 ns | 162.24 ns | +45.5% | 116.46 ns | 159.03 ns | +36.6% |
| BM_ZstdCompressor_Compress | 125.08 ns | 181.82 ns | +45.4% | 129.11 ns | 178.51 ns | +38.3% |
| BM_Spsc_PopInt | 6.29 ns | 9.12 ns | +45.0% | 6.48 ns | 8.95 ns | +38.1% |
| BM_RoundRobinPool_FourTasksFourWorkers | 419.11 ns | 600.25 ns | +43.2% | 437.61 ns | 588.35 ns | +34.4% |
| BM_BinHistSymbolTable_IdOfUnknown | 8.14 ns | 11.59 ns | +42.3% | 8.39 ns | 11.36 ns | +35.4% |
| BM_RoundRobinPool_WorkTaskSolo | 44.18 ns | 62.59 ns | +41.7% | 45.57 ns | 61.36 ns | +34.6% |
| BM_BinHistSymbolTable_IdOfLastMatch | 12.89 ns | 18.19 ns | +41.1% | 13.29 ns | 17.84 ns | +34.3% |
| BM_Wire_RoundTripFullDepthSnapshot | 2,045.57 ns | 2,878.03 ns | +40.7% | 2,109.36 ns | 2,821.20 ns | +33.7% |
| BM_Wire_WriteTrade | 24.83 ns | 34.90 ns | +40.6% | 25.04 ns | 34.22 ns | +36.7% |
| BM_BinHistSymbolTable_IdOfFirstMatch | 13.04 ns | 18.20 ns | +39.5% | 13.44 ns | 17.84 ns | +32.8% |
| BM_Mpsc_PushPopInt | 12.92 ns | 17.95 ns | +39.0% | 13.47 ns | 17.59 ns | +30.6% |
| BM_ControlChannel_Poll | 47.40 ns | 65.74 ns | +38.7% | 49.49 ns | 64.78 ns | +30.9% |
| BM_RoundRobinPool_FourWorkTasksNoPool | 178.03 ns | 243.28 ns | +36.7% | 183.69 ns | 238.46 ns | +29.8% |
| BM_Wire_WriteFullDepthSnapshot | 805.67 ns | 1,085.35 ns | +34.7% | 812.62 ns | 1,063.95 ns | +30.9% |
| BM_SymbolTable_InternNew | 665.82 ns | 890.44 ns | +33.7% | 687.40 ns | 874.15 ns | +27.2% |
| BM_Spmc_TryPopInt | 6.80 ns | 9.09 ns | +33.7% | 6.85 ns | 8.93 ns | +30.4% |
| BM_RunDataSource_FourPairs | 338.26 ns | 450.33 ns | +33.1% | 350.73 ns | 441.42 ns | +25.9% |
| BM_RoundRobinPool_FourWorkTasksFourWorkers | 628.21 ns | 836.07 ns | +33.1% | 647.94 ns | 819.36 ns | +26.5% |
| BM_BasicRiskGate_OnTickNoDrawdown | 291.67 ns | 386.13 ns | +32.4% | 299.99 ns | 378.58 ns | +26.2% |
| BM_Mpsc_PushInt | 11.15 ns | 14.73 ns | +32.1% | 11.51 ns | 14.45 ns | +25.5% |
| BM_Mpsc_TryPopInt | 7.02 ns | 9.27 ns | +32.1% | 7.28 ns | 9.07 ns | +24.6% |
| BM_BasicRiskGate_OnTickTripsAndFlattens | 821.89 ns | 1,084.27 ns | +31.9% | 825.07 ns | 1,063.05 ns | +28.8% |
| BM_ZstdDecompressor_Decompress | 2,045.46 ns | 2,691.25 ns | +31.6% | 2,109.31 ns | 2,637.93 ns | +25.1% |
| BM_Spmc_PushAllConsumersCaughtUp<8> | 52.80 ns | 69.16 ns | +31.0% | 54.45 ns | 67.80 ns | +24.5% |
| BM_Spmc_PushAllConsumersCaughtUp<1> | 7.24 ns | 9.38 ns | +29.6% | 7.29 ns | 9.20 ns | +26.1% |
| BM_Portfolio_Equity | 288.51 ns | 373.81 ns | +29.6% | 297.61 ns | 366.42 ns | +23.1% |
| BM_Spmc_PushAllConsumersCaughtUp<4> | 27.14 ns | 34.18 ns | +25.9% | 27.34 ns | 33.50 ns | +22.5% |
| BM_BacktestInProcessTransport_NRings<2> | 101.64 ns | 126.89 ns | +24.8% | 104.87 ns | 124.38 ns | +18.6% |
| BM_RoundRobinPool_FourWorkTasksOneWorker | 544.72 ns | 640.75 ns | +17.6% | 561.90 ns | 628.05 ns | +11.8% |
| BM_Spmc_PushTryPopContended<1>/threads:2 | 40.15 ns | 47.03 ns | +17.2% | 82.05 ns | 90.84 ns | +10.7% |
| BM_FanoutSink_RecordContended<4>/threads:5 | 42.27 ns | 45.44 ns | +7.5% | 217.18 ns | 221.60 ns | +2.0% |
| BM_FanoutSink_RecordContended<1>/threads:2 | 49.45 ns | 52.87 ns | +6.9% | 100.98 ns | 102.75 ns | +1.7% |
| BM_FileReplaySource_MultiSymbolMerge | 4,082,866.62 ns | 4,066,715.97 ns | -0.4% | 4,202,124.61 ns | 3,985,862.40 ns | -5.1% |
| BM_Mpsc_PushPopContended<1>/threads:2 | 35.64 ns | 34.23 ns | -3.9% | 74.31 ns | 67.10 ns | -9.7% |
| BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff | 271.79 ns | 256.61 ns | -5.6% | 280.04 ns | 251.58 ns | -10.2% |
| BM_FileReplaySource_SingleSymbolTrades | 3,659,736.10 ns | 3,291,573.56 ns | -10.1% | 3,770,902.25 ns | 3,226,936.97 ns | -14.4% |
| BM_RoundRobinPool_OneTaskOneWorker | 199.36 ns | 176.09 ns | -11.7% | 208.17 ns | 172.61 ns | -17.1% |
| BM_Spsc_PushPopContended/threads:2 | 45.75 ns | 38.55 ns | -15.7% | 94.24 ns | 75.57 ns | -19.8% |
| BM_RoundRobinPool_FourTasksOneWorker | 278.43 ns | 224.41 ns | -19.4% | 290.74 ns | 219.95 ns | -24.3% |
| BM_Engine_StepFundingEventFullPipeline | 11.21 ns | 7.74 ns | -31.0% | 11.57 ns | 7.58 ns | -34.5% |
| BM_FanoutSink_RecordAndDrain | 39.97 ns | 23.39 ns | -41.5% | 41.22 ns | 22.93 ns | -44.4% |
| BM_Engine_StepOneStrategyFullPipeline | 15.83 ns | 8.15 ns | -48.5% | 16.33 ns | 7.98 ns | -51.1% |
| BM_Engine_StepOneNoopStrategy | 7.22 ns | 3.57 ns | -50.5% | 7.45 ns | 3.50 ns | -53.0% |
| BM_FanoutSink_Record | 32.13 ns | 12.23 ns | -61.9% | 32.59 ns | 12.02 ns | -63.1% |

39 benchmark(s) unchanged (within noise floor: <1.5 ns or <5%).

**Added** (8): BM_Spmc_PushTryPopInt<false>, BM_Spmc_PushTryPopInt<true>, BM_Spmc_PushTryPopMarketEvent<false>, BM_Spmc_PushTryPopMarketEvent<true>, BM_Spsc_PushPopInt<false>, BM_Spsc_PushPopInt<true>, BM_Spsc_PushPopMarketEvent<false>, BM_Spsc_PushPopMarketEvent<true>

**Removed** (4): BM_Spmc_PushTryPopInt, BM_Spmc_PushTryPopSharedMarketEvent, BM_Spsc_PushPopInt, BM_Spsc_PushPopMarketEvent

# Decisions

1. One package, installed editable into `qp-research`, tested with pytest on synthetic data with known answers.
2. The extension is found under `QP_BUILD_DIR` when set, else the release build.
3. Annualisation is read from the index spacing. Weekly 52, daily 365, hourly 8760. Anything else is passed explicitly or refused.
4. Sharpe uses the population standard deviation, the convention the gate's thresholds were calibrated on. The t statistic uses the sample one.
5. A series with no spread or fewer than three values has no Sharpe.
6. Drawdown is measured on summed returns from a starting peak of zero, so an opening loss counts.
7. IC is the cross-sectional rank correlation per period, dropping periods with fewer than 20 names.

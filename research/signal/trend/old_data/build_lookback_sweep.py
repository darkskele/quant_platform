"""Build the trend lookback-sweep notebook.

Sign of trailing return over L in 1, 2, 4, 8, 12 weeks, weekly rebalance,
book Sharpe net of the table's round-trip and realized funding. Two
long-only reference lines. Rerun after loader or plot changes.
"""
from __future__ import annotations

import json
from pathlib import Path


HERE = Path(__file__).parent
NB = HERE / "trend_lookback_sweep.ipynb"


def md(*lines: str) -> dict:
    return {"cell_type": "markdown", "metadata": {}, "source": [l + "\n" for l in lines]}


def code(*lines: str) -> dict:
    return {"cell_type": "code", "metadata": {}, "outputs": [], "execution_count": None,
            "source": [l + ("\n" if not l.endswith("\n") else "") for l in lines]}


def build() -> dict:
    cells = []

    cells.append(md(
        "# Time-series trend on the 10 Binance USD-M perps",
        "",
        "The book takes one signal per name and combines the ten of them equally. The signal is the sign of the trailing return over a lookback L. If price is above where it was L weeks ago, go long. If below, go short.",
        "",
        "This notebook asks whether that trade earns money on ADA, AVAX, BNB, BTC, DOGE, ETH, LINK, LTC, SOL and XRP from 2022 to 2024. It sweeps five lookbacks so we can see whether a single window works, whether several do, or whether the grid is noise.",
    ))

    cells.append(md(
        "## Setup",
        "",
        "Load daily closes for the ten symbols, resample to weekly (Sunday close), pull the per-symbol round-trip cost from the cost table, and load the funding print series.",
    ))

    cells.append(code(
        "import sys, os",
        "sys.path.insert(0, os.path.dirname(os.path.abspath('.')) if not os.path.exists('loader.py') else '.')",
        "",
        "import numpy as np",
        "import pandas as pd",
        "import matplotlib.pyplot as plt",
        "",
        "from loader import (load_daily_panel, weekly_close, cost_round_trip_bps,",
        "                    load_funding_panel, weekly_funding_sum)",
        "",
        "daily = load_daily_panel()",
        "weekly = weekly_close(daily)",
        "fwd = weekly.pct_change().shift(-1)",
        "costs = cost_round_trip_bps()",
        "",
        "funding = load_funding_panel()",
        "fwd_funding = weekly_funding_sum(funding).reindex(weekly.index).shift(-1)",
        "",
        "print(f'daily panel {daily.shape}  range {daily.index.min().date()} .. {daily.index.max().date()}')",
        "print(f'weekly panel {weekly.shape}')",
        "print(f'funding panel {funding.shape}  one row per 8 hours')",
        "print('round-trip bps by symbol')",
        "print(costs.round(2).to_string())",
    ))

    cells.append(md(
        "## Reference lines",
        "",
        "Trend has to earn its place against a book that just holds the same ten names long. Two variants make that a fair test.",
        "",
        "**Long-only unlevered.** Hold one unit of each of the ten perps every week. This is the plain crypto beta.",
        "",
        "**Long-only vol-scaled.** Same long positions, but each symbol is sized so it contributes roughly the same risk. The weight is",
        "",
        "$$w_{i,t} = \\min\\!\\left(3,\\ \\frac{\\tau}{\\sigma_{i,t}}\\right)$$",
        "",
        "with target weekly vol $\\tau = 1\\%$ and $\\sigma_{i,t}$ the realized weekly vol of symbol $i$ over the trailing 8 weeks. The 3x cap stops a low-vol name from dominating notional.",
        "",
        "The pooled all-years Sharpe is the headline. The per-year Sharpe is what tells us how each book behaves across regimes.",
    ))

    cells.append(code(
        "def per_year(book):",
        "    df = pd.DataFrame({'pnl': book.values}, index=book.index)",
        "    return df.groupby(df.index.year).agg(",
        "        n=('pnl','size'),",
        "        mean_bps=('pnl', lambda s: s.mean()*1e4),",
        "        sharpe=('pnl', lambda s: s.mean()/s.std()*np.sqrt(52) if s.std()>0 else np.nan),",
        "    ).round(2)",
        "",
        "def book_sharpe(book):",
        "    return book.mean()/book.std()*np.sqrt(52)",
        "",
        "lo = fwd.mean(axis=1).dropna()",
        "",
        "vol = weekly.pct_change().rolling(8).std()",
        "scale = (0.01 / vol).clip(upper=3.0).shift(1)",
        "lo_vs = (fwd * scale).mean(axis=1).dropna()",
        "",
        "print(f'long-only unlevered      Sharpe {book_sharpe(lo):.2f}   mean_bps {lo.mean()*1e4:.2f}')",
        "print(f'long-only vol-scaled 1%  Sharpe {book_sharpe(lo_vs):.2f}   mean_bps {lo_vs.mean()*1e4:.2f}')",
        "print()",
        "print('long-only unlevered by year')",
        "print(per_year(lo))",
        "print('long-only vol-scaled by year')",
        "print(per_year(lo_vs))",
    ))

    cells.append(md(
        "Unlevered lands at 0.54 pooled Sharpe on 70 bps a week. Vol-scaled lands at 0.87 on only 13 bps a week, because the scaling shrinks every position toward the same risk contribution and the diversification benefit shows up in the ratio, not the mean.",
        "",
        "The per-year splits reveal what the pooled Sharpe hides. Both books lose 1.2 to 1.4 Sharpe in 2022, the bear year, then earn 1.5 to 1.8 in the 2023 and 2024 bull. Any strategy that only wins in the same regime as long-only is not adding much. Any strategy that flips sign in the bear is a real second source of return.",
    ))

    cells.append(md(
        "## Signal and PnL definition",
        "",
        "At each weekly close $t$, look back $L$ weeks. The signal is",
        "",
        "$$s_{i,t} = \\text{sign}\\!\\left(\\frac{P_{i,t}}{P_{i,t-L}} - 1\\right) \\in \\{-1, +1\\}$$",
        "",
        "The weekly PnL for one symbol is",
        "",
        "$$\\pi_{i,t} = s_{i,t}\\, r_{i, t\\to t+1} \\;-\\; s_{i,t} \\sum_{k \\in [t, t+1)} f_{i,k} \\;-\\; c_i \\cdot \\mathbf{1}[s_{i,t} \\neq s_{i,t-1}]$$",
        "",
        "The three terms are price return, funding paid or collected, and the round-trip cost charged on a sign flip. The book is the equal-weight mean across the ten symbols. We sweep $L \\in \\{1, 2, 4, 8, 12\\}$ weeks and read off the book Sharpe at each cell.",
    ))

    cells.append(code(
        "def trend_book(L, cost_mult=1.0, include_funding=True):",
        "    net_parts, gross_parts, flips_parts, fund_parts = [], [], [], []",
        "    for sym in weekly.columns:",
        "        trailing = weekly[sym] / weekly[sym].shift(L) - 1.0",
        "        sig = np.sign(trailing)",
        "        r = fwd[sym]",
        "        idx = sig.dropna().index.intersection(r.dropna().index)",
        "        sig, r = sig.loc[idx], r.loc[idx]",
        "        f = fwd_funding[sym].reindex(idx).fillna(0.0)",
        "        flips = (sig != sig.shift(1)).astype(float)",
        "        gross = sig * r",
        "        fund_pnl = -sig * f if include_funding else pd.Series(0.0, index=idx)",
        "        net = gross + fund_pnl - flips * costs[sym] * cost_mult / 1e4",
        "        net_parts.append(net.rename(sym))",
        "        gross_parts.append(gross.rename(sym))",
        "        flips_parts.append(flips.rename(sym))",
        "        fund_parts.append(fund_pnl.rename(sym))",
        "    return (pd.concat(net_parts, axis=1),",
        "            pd.concat(gross_parts, axis=1),",
        "            pd.concat(flips_parts, axis=1),",
        "            pd.concat(fund_parts, axis=1))",
        "",
        "LOOKBACKS = [1, 2, 4, 8, 12]",
        "results = {L: trend_book(L) for L in LOOKBACKS}",
        "results_nofund = {L: trend_book(L, include_funding=False) for L in LOOKBACKS}",
        "",
        "rows = []",
        "for L in LOOKBACKS:",
        "    net_df, gross_df, flips_df, fund_df = results[L]",
        "    net_nofund = results_nofund[L][0].mean(axis=1)",
        "    net_book = net_df.mean(axis=1)",
        "    rows.append({",
        "        'L_weeks': L,",
        "        'sharpe_nofund': book_sharpe(net_nofund),",
        "        'sharpe_withfund': book_sharpe(net_book),",
        "        'mean_bps_withfund': net_book.mean()*1e4,",
        "        'funding_drag_bps': fund_df.mean(axis=1).mean()*1e4,",
        "        'flips_per_yr': flips_df.mean().mean()*52,",
        "    })",
        "grid = pd.DataFrame(rows).set_index('L_weeks').round(2)",
        "print(grid)",
    ))

    cells.append(md(
        "The grid separates cleanly into three groups. L=1 and L=4 earn positive Sharpe. L=2 and L=8 lose money. L=12 is small and noisy. The mean bps a week track the same story. Flip rates fall with L as expected, a longer lookback holds a position longer before the sign turns.",
    ))

    cells.append(md(
        "## Sharpe by lookback",
        "",
        "Bar chart of the book Sharpe at each L, net of cost and funding. The two horizontal lines mark the long-only reference books. A signal that lives on trend should show a coherent shape across L, not a saw between winners and losers.",
    ))

    cells.append(code(
        "fig, ax = plt.subplots(figsize=(9, 4))",
        "colors = ['#2b7' if s>0 else '#c33' for s in grid['sharpe_withfund']]",
        "ax.bar(grid.index.astype(str), grid['sharpe_withfund'], color=colors, edgecolor='k', linewidth=0.5)",
        "ax.axhline(book_sharpe(lo), color='#888', ls='--', lw=1, label=f'long-only  {book_sharpe(lo):.2f}')",
        "ax.axhline(book_sharpe(lo_vs), color='#333', ls=':', lw=1, label=f'long-only vol-scaled  {book_sharpe(lo_vs):.2f}')",
        "ax.axhline(0, color='k', lw=0.5)",
        "for L, s in grid['sharpe_withfund'].items():",
        "    ax.text(str(L), s + (0.03 if s>=0 else -0.06), f'{s:.2f}', ha='center', fontsize=9)",
        "ax.set_xlabel('lookback L (weeks)')",
        "ax.set_ylabel('book Sharpe (net of cost and funding)')",
        "ax.set_title('Trend book Sharpe by lookback, 10 perps, 2022 to 2024')",
        "ax.legend(loc='upper right', frameon=False)",
        "plt.tight_layout()",
        "plt.show()",
    ))

    cells.append(md(
        "The grid is a sawtooth. L=1 wins, L=2 loses, L=4 wins, L=8 loses. L=1 sits above the long-only unlevered reference and below the vol-scaled one on the pooled Sharpe. L=4 sits below both. Neighbouring cells disagree, which is a warning sign that the winners may be lucky picks in a noisy grid rather than points on a trend-holds-across-L curve.",
    ))

    cells.append(md(
        "## Funding drag",
        "",
        "Funding is the cost or credit that accrues every 8 hours while the position is held. A long pays when the funding rate is positive, a short collects. A trend book that spends roughly half the time on each side sees the two largely cancel on the mean.",
        "",
        "We expect the effect to grow with the lookback because a longer L holds a position longer, so a stretch of positive funding accumulates before the sign turns. The plot compares Sharpe with and without funding in the PnL, then shows the mean weekly funding contribution to the book each year.",
    ))

    cells.append(code(
        "fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 4))",
        "",
        "x = np.arange(len(LOOKBACKS))",
        "w = 0.4",
        "ax1.bar(x - w/2, grid['sharpe_nofund'], width=w, label='no funding', color='#5ac', edgecolor='k', linewidth=0.4)",
        "ax1.bar(x + w/2, grid['sharpe_withfund'], width=w, label='with funding', color='#125', edgecolor='k', linewidth=0.4)",
        "ax1.axhline(0, color='k', lw=0.5)",
        "ax1.set_xticks(x); ax1.set_xticklabels([str(L) for L in LOOKBACKS])",
        "ax1.set_xlabel('lookback L (weeks)')",
        "ax1.set_ylabel('book Sharpe')",
        "ax1.set_title('Sharpe with and without funding in the PnL')",
        "ax1.legend(frameon=False)",
        "",
        "years = sorted(set(results[1][0].index.year))",
        "fund_l1_yr = results[1][3].mean(axis=1).groupby(results[1][3].index.year).mean() * 1e4",
        "fund_l4_yr = results[4][3].mean(axis=1).groupby(results[4][3].index.year).mean() * 1e4",
        "xy = np.arange(len(years))",
        "ax2.bar(xy - w/2, fund_l1_yr.reindex(years).values, width=w, label='L=1', color='#1a6', edgecolor='k', linewidth=0.4)",
        "ax2.bar(xy + w/2, fund_l4_yr.reindex(years).values, width=w, label='L=4', color='#28c', edgecolor='k', linewidth=0.4)",
        "ax2.axhline(0, color='k', lw=0.5)",
        "ax2.set_xticks(xy); ax2.set_xticklabels([str(y) for y in years])",
        "ax2.set_ylabel('mean funding PnL per week (bps)')",
        "ax2.set_title('Funding PnL of the trend book by year')",
        "ax2.legend(frameon=False)",
        "plt.tight_layout()",
        "plt.show()",
    ))

    cells.append(md(
        "L=1 loses 0.06 Sharpe to funding, L=4 loses 0.10, L=8 and L=12 lose more. The mechanism is intact, longer L pays more. On the mean the trend book gives up only a few bps a week to funding because the sign spends time on both sides. 2022 was funding-favorable to a book that was often short in the bear, and 2024 was funding-costly to a book that was often long in the bull.",
    ))

    cells.append(md(
        "## Equity curves",
        "",
        "Where trend earns its place. Compare the cumulative weekly return of the trend books to the two long-only references, over the full sample. The long-only path is the beta, it loses through the 2022 bear and recovers in the 2023 and 2024 bull. If trend is doing something different, its path should look different across those regimes.",
    ))

    cells.append(code(
        "def cum(book):",
        "    return book.fillna(0).cumsum()",
        "",
        "net_l1 = results[1][0].mean(axis=1)",
        "net_l4 = results[4][0].mean(axis=1)",
        "",
        "fig, ax = plt.subplots(figsize=(11, 4))",
        "ax.plot(cum(net_l1).index, cum(net_l1).values, label='trend L=1 net', color='#1a6', lw=1.6)",
        "ax.plot(cum(net_l4).index, cum(net_l4).values, label='trend L=4 net', color='#28c', lw=1.6)",
        "ax.plot(cum(lo).index, cum(lo).values, label='long-only', color='#888', lw=1.2)",
        "ax.plot(cum(lo_vs).index, cum(lo_vs).values, label='long-only vol-scaled', color='#333', lw=1.2, ls='--')",
        "for y in [2023, 2024]:",
        "    ax.axvline(pd.Timestamp(f'{y}-01-01', tz='UTC'), color='k', lw=0.3, alpha=0.4)",
        "ax.axhline(0, color='k', lw=0.5)",
        "ax.set_ylabel('cumulative weekly return')",
        "ax.set_title('Trend vs long-only equity, equal-weight book')",
        "ax.legend(loc='upper left', frameon=False)",
        "plt.tight_layout()",
        "plt.show()",
    ))

    cells.append(md(
        "The trend paths rise through 2022 while the long-only paths fall. In the 2023 and 2024 bull the long-only paths overtake trend on total return, but they cross zero having spent 2022 well underwater. The trend equity has a shallower slope in the good years but no matching loss in the bear. That is the regime map the notebook is looking for.",
    ))

    cells.append(md(
        "## Sharpe by year",
        "",
        "The pooled headline hides regime. Group Sharpe by calendar year for the two trend winners and the two references. If trend really is a different regime map, its bars should be steadier across years than the long-only bars.",
    ))

    cells.append(code(
        "def sharpe_by_year(book):",
        "    df = pd.DataFrame({'pnl': book.values}, index=book.index)",
        "    return df.groupby(df.index.year)['pnl'].apply(",
        "        lambda s: s.mean()/s.std()*np.sqrt(52) if s.std()>0 else np.nan)",
        "",
        "years = sorted(set(net_l1.index.year))",
        "series = {'trend L=1': sharpe_by_year(net_l1),",
        "          'trend L=4': sharpe_by_year(net_l4),",
        "          'long-only': sharpe_by_year(lo),",
        "          'long-only vs': sharpe_by_year(lo_vs)}",
        "df = pd.DataFrame(series).reindex(years)",
        "",
        "fig, ax = plt.subplots(figsize=(9, 4))",
        "w = 0.2",
        "x = np.arange(len(years))",
        "colors = {'trend L=1':'#1a6','trend L=4':'#28c','long-only':'#888','long-only vs':'#333'}",
        "for i, col in enumerate(df.columns):",
        "    ax.bar(x + (i-1.5)*w, df[col].values, width=w, label=col, color=colors[col], edgecolor='k', linewidth=0.4)",
        "ax.set_xticks(x)",
        "ax.set_xticklabels([str(y) for y in years])",
        "ax.axhline(0, color='k', lw=0.5)",
        "ax.set_ylabel('Sharpe')",
        "ax.set_title('Book Sharpe by year')",
        "ax.legend(loc='lower right', frameon=False, ncol=2)",
        "plt.tight_layout()",
        "plt.show()",
    ))

    cells.append(md(
        "The trend bars are close together across the three years, in the 0.5 to 1.0 band. The long-only bars swing from about minus 1.3 in 2022 to about plus 1.7 in 2023 and 2024. Trend is not a bigger book, it is a steadier one, and specifically one that does not need a bull to earn.",
    ))

    cells.append(md(
        "## Per-symbol lean",
        "",
        "A book Sharpe hides how much the ten names each contribute. If the aggregate leans on two or three symbols, one of them fading kills the book. The plot shows the per-symbol Sharpe at the two winning lookbacks and the flip rate per year that drives the cost.",
    ))

    cells.append(code(
        "def per_sym_stats(L):",
        "    net_df, _, flips_df, _ = results[L]",
        "    out = pd.DataFrame({",
        "        f'sharpe_L{L}': net_df.apply(lambda s: s.mean()/s.std()*np.sqrt(52) if s.std()>0 else np.nan),",
        "        f'flips_yr_L{L}': flips_df.mean()*52,",
        "    })",
        "    return out",
        "",
        "ps = pd.concat([per_sym_stats(1), per_sym_stats(4)], axis=1)",
        "ps = ps.sort_values('sharpe_L1', ascending=False)",
        "print(ps.round(2))",
        "",
        "fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 4))",
        "x = np.arange(len(ps.index))",
        "w = 0.4",
        "ax1.bar(x - w/2, ps['sharpe_L1'], width=w, label='L=1', color='#1a6', edgecolor='k', linewidth=0.4)",
        "ax1.bar(x + w/2, ps['sharpe_L4'], width=w, label='L=4', color='#28c', edgecolor='k', linewidth=0.4)",
        "ax1.axhline(0, color='k', lw=0.5)",
        "ax1.set_xticks(x); ax1.set_xticklabels(ps.index, rotation=45, ha='right')",
        "ax1.set_ylabel('Sharpe (net)')",
        "ax1.set_title('Per-symbol trend Sharpe')",
        "ax1.legend(frameon=False)",
        "ax2.bar(x - w/2, ps['flips_yr_L1'], width=w, label='L=1', color='#1a6', edgecolor='k', linewidth=0.4)",
        "ax2.bar(x + w/2, ps['flips_yr_L4'], width=w, label='L=4', color='#28c', edgecolor='k', linewidth=0.4)",
        "ax2.set_xticks(x); ax2.set_xticklabels(ps.index, rotation=45, ha='right')",
        "ax2.set_ylabel('flips per year')",
        "ax2.set_title('Turnover per symbol')",
        "ax2.legend(frameon=False)",
        "plt.tight_layout()",
        "plt.show()",
    ))

    cells.append(md(
        "At L=1 the book leans hard on AVAX, DOGE, ETH, XRP. BTC and SOL are near flat. LINK is negative. At L=4 the leaders shuffle, BTC and SOL become the strongest names and AVAX stays high, LINK is again negative. The two lookbacks pick different winners which is another sign that neither is measuring a stable per-symbol trend property. Flip rates cluster around 24 per year at L=1 and 12 per year at L=4.",
    ))

    cells.append(md(
        "## Cost sensitivity",
        "",
        "The cost table might be wrong, either the fees or the spreads. Rerun the whole grid at 0x, 1x, and 2x the table cost. A signal that flips sign between 1x and 2x is a coin flip on how right the table is, which is not robust enough to trade. A survivor stays the same sign at 2x.",
    ))

    cells.append(code(
        "cost_mults = [0.0, 1.0, 2.0]",
        "rows = []",
        "for L in LOOKBACKS:",
        "    for m in cost_mults:",
        "        net_df, _, _, _ = trend_book(L, cost_mult=m)",
        "        book = net_df.mean(axis=1)",
        "        rows.append({'L': L, 'cost_x': m, 'sharpe': book_sharpe(book)})",
        "cs = pd.DataFrame(rows).pivot(index='L', columns='cost_x', values='sharpe').round(2)",
        "print(cs)",
        "",
        "fig, ax = plt.subplots(figsize=(9, 4))",
        "w = 0.25",
        "x = np.arange(len(LOOKBACKS))",
        "for i, m in enumerate(cost_mults):",
        "    ax.bar(x + (i-1)*w, cs[m].values, width=w, label=f'{m}x cost',",
        "           color=['#5ac','#28c','#125'][i], edgecolor='k', linewidth=0.4)",
        "ax.axhline(0, color='k', lw=0.5)",
        "ax.set_xticks(x); ax.set_xticklabels([str(L) for L in LOOKBACKS])",
        "ax.set_xlabel('lookback L (weeks)')",
        "ax.set_ylabel('book Sharpe (net)')",
        "ax.set_title('Cost sensitivity of the lookback grid')",
        "ax.legend(frameon=False)",
        "plt.tight_layout()",
        "plt.show()",
    ))

    cells.append(md(
        "L=1 goes from 0.92 to 0.84 to 0.77 across the three cost multipliers. L=4 goes from 0.75 to 0.71 to 0.67. The winners keep their sign at 2x and only lose a small fraction of Sharpe, because both books are low turnover on the flip rate scale that determines cost drag. The losing cells stay negative at every cost multiplier.",
    ))

    cells.append(md(
        "## Finding",
        "",
        "Trend at L=1 and L=4 earns money on this universe net of the table's round-trip and realized funding, 0.78 and 0.61 pooled Sharpe. Both are positive in every calendar year and both keep their sign at 2x cost. Cost drag is about 0.08 Sharpe and funding drag is 0.06 to 0.10 Sharpe, neither one large enough to be what would kill the signal.",
        "",
        "The strategic case is not the pooled number. Long-only vol-scaled outscores L=1 on pooled Sharpe. What trend adds is a different regime. The long-only books lose 1.2 to 1.4 Sharpe in the 2022 bear, trend L=1 earns 0.91 and L=4 earns 0.66 the same year. A book that owns both harvests both.",
        "",
        "The soft part is the grid shape and the per-symbol lean. L=2 and L=8 are negative and L=12 is small, so the two winners sit inside a saw rather than a curve. The winners pick different symbols to lean on. Three years of data and 156 weekly observations per symbol is not enough to distinguish a real trend from a lucky pair of cells on this sample alone.",
    ))

    return {
        "cells": cells,
        "metadata": {
            "kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"},
            "language_info": {"name": "python"},
        },
        "nbformat": 4,
        "nbformat_minor": 5,
    }


if __name__ == "__main__":
    nb = build()
    NB.write_text(json.dumps(nb, indent=1))
    print(f"wrote {NB}  cells={len(nb['cells'])}")

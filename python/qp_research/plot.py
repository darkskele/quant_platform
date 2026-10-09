"""House style for research charts."""
from __future__ import annotations

import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap

BLUE, ORANGE, AQUA, YELLOW, GREY = '#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#8a8a86'
NEUTRAL = '#f0efec'

STYLE = {'figure.dpi': 110, 'axes.spines.top': False, 'axes.spines.right': False,
         'axes.grid': True, 'grid.alpha': 0.25, 'lines.linewidth': 2}


def style() -> None:
    """Applies the house style to every chart drawn after it."""
    plt.rcParams.update(STYLE)


def diverging() -> LinearSegmentedColormap:
    """Orange below zero through neutral to blue above, for signed grids."""
    return LinearSegmentedColormap.from_list('div', [ORANGE, NEUTRAL, BLUE])

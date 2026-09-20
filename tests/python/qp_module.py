"""Locates and imports the built qp_python_backtest extension."""
from __future__ import annotations

import importlib
import sys
from pathlib import Path

REPO = Path(__file__).resolve()
while REPO != REPO.parent and not (REPO / "CMakeLists.txt").exists():
    REPO = REPO.parent

BUILD_HINT = (
    "qp_python_backtest not found. Build it first:\n"
    "  cmake --preset release -DQP_BUILD_PYTHON=ON"
    " -DPYTHON_EXECUTABLE=$HOME/miniconda3/envs/qp-research/bin/python\n"
    "  cmake --build build/release --target qp_python_backtest -j"
)


def load():
    """Imports the module, searching the build trees when it is not installed."""
    try:
        return importlib.import_module("qp_python_backtest")
    except ImportError:
        pass

    found = sorted(REPO.glob("**/qp_python_backtest*.so"))
    for so in found:
        sys.path.insert(0, str(so.parent))
        try:
            return importlib.import_module("qp_python_backtest")
        except ImportError:
            sys.path.pop(0)

    # A build for another interpreter is the common case with two pythons on
    # the box, and it looks identical to no build at all without this.
    if found:
        built = "\n".join(f"  {so.name}" for so in found)
        raise SystemExit(
            f"qp_python_backtest was built, but not for {sys.executable}\n"
            f"(python {sys.version_info.major}.{sys.version_info.minor})\n"
            f"{built}\n"
            "Rebuild with -DPYTHON_EXECUTABLE pointed at this interpreter, or run"
            " the one it was built for."
        )
    raise SystemExit(BUILD_HINT)

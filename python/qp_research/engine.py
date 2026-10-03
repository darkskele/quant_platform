"""The built qp_python_backtest extension.

Found under QP_BUILD_DIR when set, else the repo's release build.
"""
from __future__ import annotations

import importlib
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
NAME = "qp_python_backtest"


def build_dir() -> Path:
    return Path(os.environ.get("QP_BUILD_DIR", REPO / "build" / "release"))


def path() -> Path | None:
    """The extension's file, or None when it has not been built."""
    found = sorted(build_dir().glob(f"**/{NAME}*.so"))
    return found[0] if found else None


def built() -> bool:
    return path() is not None


def module():
    """The imported extension. Raises ImportError when it has not been built."""
    found = path()
    if found is None:
        raise ImportError(f"{NAME} not built under {build_dir()}")
    folder = str(found.parent)
    if folder not in sys.path:
        sys.path.insert(0, folder)
    return importlib.import_module(NAME)

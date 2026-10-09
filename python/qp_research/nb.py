"""Notebooks built from python and executed in place.

A builder lists cells with md and code, writes them, then executes the
notebook so its outputs are embedded. Execution fails loudly on any cell error.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

KERNEL = {'display_name': 'qp-research', 'language': 'python', 'name': 'python3'}


def md(*lines: str) -> dict:
    """A markdown cell, one argument per line."""
    return {'cell_type': 'markdown', 'metadata': {}, 'source': '\n'.join(lines)}


def code(*lines: str) -> dict:
    """A code cell, one argument per line."""
    return {'cell_type': 'code', 'metadata': {}, 'execution_count': None, 'outputs': [], 'source': '\n'.join(lines)}


def write(path, cells: list[dict]) -> Path:
    """Writes cells as a notebook on the qp-research kernel."""
    path = Path(path)
    doc = {'cells': cells, 'metadata': {'kernelspec': KERNEL, 'language_info': {'name': 'python'}},
           'nbformat': 4, 'nbformat_minor': 5}
    for i, c in enumerate(doc['cells']):
        c.setdefault('id', f'cell-{i}')
    path.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + '\n')
    return path


def errors(path) -> list[str]:
    """Each error an executed notebook carries, as its name and message."""
    doc = json.loads(Path(path).read_text())
    return [f"{o['ename']}: {o['evalue']}" for c in doc['cells'] if c['cell_type'] == 'code'
            for o in c.get('outputs', []) if o.get('output_type') == 'error']


def execute(path, timeout=3600) -> Path:
    """Runs every cell in place so the outputs are embedded. Raises if any cell errored."""
    path = Path(path)
    run = subprocess.run([sys.executable, '-m', 'nbconvert', '--to', 'notebook', '--execute', '--inplace',
                          '--allow-errors', f'--ExecutePreprocessor.timeout={timeout}', str(path)],
                         cwd=path.parent, capture_output=True, text=True)
    if run.returncode:
        raise RuntimeError(f'nbconvert failed on {path.name}\n{run.stderr[-2000:]}')
    found = errors(path)
    if found:
        raise RuntimeError(f'{path.name} has {len(found)} failed cells\n' + '\n'.join(found))
    return path


def build(path, cells: list[dict], run=True) -> Path:
    """Writes the notebook, then executes it unless run is False."""
    write(path, cells)
    return execute(path) if run else Path(path)

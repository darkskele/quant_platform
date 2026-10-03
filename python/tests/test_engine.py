import pytest

from qp_research import engine


def test_unbuilt_dir_reports_missing(tmp_path, monkeypatch):
    monkeypatch.setenv('QP_BUILD_DIR', str(tmp_path))
    assert not engine.built()
    with pytest.raises(ImportError):
        engine.module()


def test_found_under_override(tmp_path, monkeypatch):
    (tmp_path / 'nested').mkdir()
    (tmp_path / 'nested' / 'qp_python_backtest.cpython-312-x86_64-linux-gnu.so').touch()
    monkeypatch.setenv('QP_BUILD_DIR', str(tmp_path))
    assert engine.path().parent == tmp_path / 'nested'


@pytest.mark.skipif(not engine.built(), reason='qp_python_backtest not built')
def test_release_build_imports():
    assert hasattr(engine.module(), 'PythonBacktest')

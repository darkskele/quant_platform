"""Model registry for the funding-signal experimentation loop.

Each entry maps a name to a factory: `factory() -> predict_fn`, where
`predict_fn(train, test, feature_cols, target_col) -> np.ndarray of predictions
on test`. The factory pattern lets each name carry its own hyperparameters
without leaking them into the caller.

The notebook loops the registry through the same walk-forward CV and produces
one comparison table (rank IC, R^2, dir_acc, MSE). All scaling and fitting
happens inside the factory's closure per fold; no state is shared across
folds, no test data ever sees the fit.
"""

from __future__ import annotations

from typing import Callable

import numpy as np
import pandas as pd
from sklearn.linear_model import Ridge, Lasso, ElasticNet, BayesianRidge
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline

PredictFn = Callable[[pd.DataFrame, pd.DataFrame, list[str], str], np.ndarray]


def _with_symbol_dummies(df: pd.DataFrame, cols: list[str]) -> tuple[np.ndarray, list[str]]:
    dummies = pd.get_dummies(df["symbol"], prefix="sym", drop_first=True)
    X = pd.concat([df[cols].reset_index(drop=True), dummies.reset_index(drop=True)], axis=1)
    return X.to_numpy(dtype=float), list(X.columns)


def _make_linear(estimator_factory: Callable) -> PredictFn:
    def _predict(train, test, feature_cols, target_col):
        Xtr, _ = _with_symbol_dummies(train, feature_cols)
        ytr = train[target_col].to_numpy()
        pipe = Pipeline([("sc", StandardScaler()), ("m", estimator_factory())])
        pipe.fit(Xtr, ytr)
        Xte, _ = _with_symbol_dummies(test, feature_cols)
        return pipe.predict(Xte)
    return _predict


def persistence_factory(horizon: int) -> PredictFn:
    """Baseline: predict cum-N as horizon * funding_lag1."""
    def _predict(train, test, feature_cols, target_col):
        return (horizon * test["funding_lag1"]).to_numpy()
    return _predict


def ridge_registry(alphas=(0.01, 0.1, 1.0, 10.0, 100.0)) -> dict[str, PredictFn]:
    return {f"ridge_a{a}": _make_linear(lambda a=a: Ridge(alpha=a)) for a in alphas}


def lasso_registry(alphas=(1e-6, 1e-5, 1e-4, 1e-3, 1e-2)) -> dict[str, PredictFn]:
    # Lasso alpha scale on this standardised target is much smaller than ridge's;
    # cum-24 funding lives at ~1e-3, so alphas above ~1e-2 zero out everything.
    return {
        f"lasso_a{a:g}": _make_linear(lambda a=a: Lasso(alpha=a, max_iter=20000))
        for a in alphas
    }


def elasticnet_registry(alpha: float = 1e-4, l1_ratios=(0.3, 0.5, 0.7)) -> dict[str, PredictFn]:
    return {
        f"elasticnet_l1r{r:g}": _make_linear(
            lambda a=alpha, r=r: ElasticNet(alpha=a, l1_ratio=r, max_iter=20000)
        )
        for r in l1_ratios
    }


def bayesian_registry() -> dict[str, PredictFn]:
    return {"bayesian_ridge": _make_linear(BayesianRidge)}


def linear_registry_v1(horizon: int) -> dict[str, PredictFn]:
    """Full linear model registry for the v1 feature set sweep."""
    reg: dict[str, PredictFn] = {"persistence": persistence_factory(horizon)}
    reg.update(ridge_registry())
    reg.update(lasso_registry())
    reg.update(elasticnet_registry())
    reg.update(bayesian_registry())
    return reg


def _make_gbm(params: dict) -> PredictFn:
    import lightgbm as lgb

    def _predict(train, test, feature_cols, target_col):
        Xtr, cols = _with_symbol_dummies(train, feature_cols)
        ytr = train[target_col].to_numpy()
        dtr = lgb.Dataset(Xtr, label=ytr, feature_name=cols)
        n_rounds = params.get("num_boost_round", 400)
        p = {k: v for k, v in params.items() if k != "num_boost_round"}
        booster = lgb.train(p, dtr, num_boost_round=n_rounds)
        Xte, _ = _with_symbol_dummies(test, feature_cols)
        return booster.predict(Xte)
    return _predict


def gbm_registry() -> dict[str, PredictFn]:
    """A small sweep over GBM regularisation. Defaults first, then tighter."""
    base = dict(objective="regression", feature_fraction=0.9, bagging_fraction=0.9,
                bagging_freq=5, verbose=-1)
    return {
        "gbm_default": _make_gbm({**base, "learning_rate": 0.05, "num_leaves": 31,
                                  "min_data_in_leaf": 200, "num_boost_round": 400}),
        "gbm_tight":   _make_gbm({**base, "learning_rate": 0.02, "num_leaves": 15,
                                  "min_data_in_leaf": 500, "num_boost_round": 800}),
        "gbm_tighter": _make_gbm({**base, "learning_rate": 0.02, "num_leaves": 7,
                                  "min_data_in_leaf": 1000, "num_boost_round": 800}),
    }

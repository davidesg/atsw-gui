# drvarma — Developer Guide

This document describes the internal architecture of drvarma for contributors.

## 1. Overview

drvarma fits a general **VARMA(p,q)** model

    Φ(B) (w_t − μ) = Θ(B) a_t,   a_t ~ N(0, Σ),

to a vector series `w_t` obtained from the raw data by Box-Cox transform,
regular/seasonal differencing and optional harmonic seasonal adjustment.
Estimation is **exact Gaussian maximum likelihood** via a factorized BFGS
quasi-Newton optimiser.

The codebase is C (C99), 1-based indexing throughout, with `real` = `double`
(see `include/main.h`). Heavy linear algebra uses the **GNU Scientific Library
(GSL)**; there is **no Numerical Recipes code** (see `NR_REMOVAL_PLAN.md`).

## 2. Source layout

| File | Responsibility |
|------|----------------|
| `src/drvarma.c` | `main`: CLI parsing, I/O, and the whole pipeline. Defines `shootx()` (maps the parameter vector ↔ `struct Tvarma`, builds residuals) and the starting-value routines (`init_varma`, `hannan_rissanen_diag`). |
| `src/drvmlest.c` | `est()` — estimation driver: prepares the problem and calls the optimiser. |
| `src/qnewtopt.c` | Factorized BFGS quasi-Newton optimiser (J.A. Mauricio). Owns the convergence criteria (gradtol/steptol) and the final convergence report. |
| `src/elfvarma.c` | `elf()` — the exact log-likelihood of a VARMA(p,q); `chekma()` — MA invertibility check via eigenvalues. |
| `src/multshea.c` | Likelihood building blocks (multivariate, Shea-style). |
| `src/nlatools.c` | Linear algebra + memory + strings: LU (`ludcp`/`lusol`), Cholesky (`choldcp`/`cholfor`/`cholbak`/`cholsol`), eigenvalues (`eigenqr`, **GSL**), SVD (`svdcp`/`svsol`, **GSL**), allocators (`vector`/`matrix`/`tensor`/`free_*`), `nrerror`, `pythag`, string utils. |
| `src/diagnose.c` | Residual diagnostics (summary, ACF/PACF, Hosking Q, multivariate Jarque–Bera), Wald hypothesis tests, impulse responses and FEVD. |
| `src/forecast.c` | `forecast_model()` (point forecasts + error variances, supports an origin offset `b`), `forecast_level_variances()`. |
| `src/transform.c` | `transform_series()` (Box-Cox + differencing → stationary matrix) and `integrate_forecast()` (invert differencing + Box-Cox). |
| `src/deseason.c` | `deseasonalize_raw()` — harmonic seasonal adjustment on the differenced basis; returns level dummies for re-seasonalising forecasts. |
| `src/volatility.c` | Conditional volatility (exponential / moving window). |
| `include/main.h` | Core types (`real`, `struct Tvarma`, `struct Tseries`) and most prototypes. |
| `gui/` | GTK-3 front-end (`drvarma_gui.c`) plus Johansen cointegration and VECM helpers. |

## 3. Core data structures (`include/main.h`)

```c
struct Tvarma {           /* a normalized VARMA model */
    int m, n, p, q;       /* series, obs, AR order, MA order */
    real *mu;             /* mean vector            [1..m]            */
    real ***phi, ***theta;/* AR/MA matrices         [0..p][1..m][1..m]*/
    real **qq;            /* normalized covariance   [1..m][1..m]     */
    real **w, **a;        /* data and residuals      [1..n][1..m]     */
    real sigma2, logelf, xitol;
};
```

`struct Tseries` holds a single (univariate) series with its summary statistics,
used by the diagnostics/plot routines.

## 4. Estimation pipeline (`main` in `drvarma.c`)

1. **Parse CLI** → global flags (`global_p/q`, `global_diag_*`, `global_include_mean`,
   `trans_lambda/scale/d/D`, `do_deseason`, `forecast_horizon`, `g_estwin`, …).
2. **Read `.inp`** (raw levels).
3. **`deseasonalize_raw()`** (optional) → seasonal dummies subtracted from levels.
4. **`transform_series()`** → `datamat` (the stationary `w`), plus `bc_series`
   (Box-Cox levels, kept to integrate forecasts back).
5. **`calc_nparametrs()`**, **`init_varma()`** (OLS-based starting values;
   `hannan_rissanen_diag()` if `-twostep`).
6. **`est()`** drives **`qnewtopt`**; at each trial parameter vector it calls
   **`shootx()`** to rebuild the `Tvarma` model and **`elf()`** to evaluate the
   exact log-likelihood (Cholesky factorisations in `multshea`/`elfvarma`).
7. **Reporting**: `print_parameters`, `all_hypothesis_tests` (Wald),
   `impulse_response` + `variance_decomposition`, `multivariate_diagnostics`
   (Q, JB), `print_roots` (eigenvalues via `eigenqr`/GSL), `diagnose`.
8. **Forecasting** (if `-forecast`): `forecast_model` → `integrate_forecast`
   → re-seasonalise → `file.forecast`. With `-estwin`, a second loop produces
   `file.recursive` (see §6).

### `shootx()` (parameter ↔ model mapping)

`est()` is passed `shootx` as the objective hook. `shootx(x, &varma, &ifault,
firstx, lastx)` unpacks the flat vector `x` into `varma.mu/phi/theta/qq` honoring
the diagonal flags, copies `datamat` into `varma.w`, and (with the likelihood
code) computes residuals. `firstx`/`lastx` allocate/free the model buffers.

## 5. Conventions and gotchas

- **1-based indexing**: `vector(1,n)` yields `v[1..n]`; `matrix(1,m,1,n)` yields
  `m[1..m][1..n]`. Allocators (in `nlatools.c`) size `[0..nh]` with `calloc` and
  index from `nl`. Matrices are **not assumed tightly contiguous**; always access
  as `m[i][j]` (the GSL wrappers copy element-by-element, so layout-agnostic).
- **`real`** is `double`; keep new code type-generic via `real`.
- **Globals**: dimensions (`nser`, `nobs`, `nobs_raw`), orders and transform
  parameters are file-scope globals in `drvarma.c` and `extern` elsewhere. The
  estimation window for `-estwin` uses `g_estwin` / `g_in_est` (the latter is set
  only while the optimiser runs, so `shootx` restricts the likelihood to the
  training window without shrinking buffers).
- **Scaling**: `trans_scale` (default 100) multiplies the series after Box-Cox.
  Coefficients (Φ/Θ) are scale-invariant; `μ` scales by the factor and `Σ` by its
  square. *Standard errors of cross terms can be ill-conditioned when series have
  very different variances* (e.g. WTI vs CPI) — see the caveat in `MODELS_RESULTS.md`.

## 6. Fixed-parameter recursive forecasting (`-estwin`)

For `q = 0`, `forecast_model()` with an origin offset `b` forecasts from
observation `n − b` using only `w` up to that origin (the MA branch, which needs
residuals, is skipped). `main` therefore: estimates once on `g_estwin`
observations, then loops `b` from `nobs − g_estwin` down to 0, integrating each
forecast anchored at the corresponding raw index and re-seasonalising with the
training-window dummies. The deseasonalisation is also restricted to the training
window for a clean fixed-parameter design.

## 7. Build system

`Makefile` builds `bin/drvarma` (engine) and `bin/drvarma_gui` (GTK). GSL and
GTK+ are located via `pkg-config`. Object files go to `build/`. Cross-compilation
to static Windows binaries is supported via MXE (`make CROSS=…`).

## 8. Linear algebra / GSL boundary

Only `nlatools.c` includes GSL. Public signatures (`eigenqr`, `svdcp`, `svsol`)
are unchanged from the historical (NR-based) versions, so the rest of the code is
agnostic to the backend. When adding numerical routines, prefer GSL and keep the
1-based wrapper convention used in `nlatools.c`.

## 9. Testing / regression

`sh tests/run_tests.sh` runs both batteries; `make check` at the monorepo
root runs it with every other engine and GUI.

- `tests/banco/banco.sh`: 18 `.inp` cases over `data/` (the Group 1
  trivariate, the WTI+CPI pass-through, IPC, PSW), covering the diagonal and
  full models, `-m 2`, `-twostep`, `-volexp`, `-deseason`, `-forecast` and
  `-estwin`. The reference `tests/banco/ref/` is the 0.4.1 output, and every
  file must match byte for byte except the `Program` line.
  `--generar` rewrites it, and should only be run when a change of output is
  deliberate. The `.out` files kept in `data/` come from older binaries (scale
  1) and are not a reference.
- `tests/escalera/pruebas.sh`: the ladder. It makes explicit assertions (the
  gate, σ² and coefficients against fue's `.out`, genuine `.pre` files as
  fixed points, BUG-2, alignment at the end, rejected options, the
  pass-through, the line search on NaN/∞) and a byte regression of the 5.0
  output with the path and version lines filtered.

The `cases/*.py` scripts compute recursive forecast-error metrics.

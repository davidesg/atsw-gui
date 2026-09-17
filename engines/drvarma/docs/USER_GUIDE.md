# drvarma — User Guide

drvarma estimates **vector ARMA (VARMA)** models by exact maximum likelihood and
produces diagnostics, impulse-response analysis and forecasts. This guide covers
the command-line program; the graphical front-end mirrors the same options.

## 1. Quick start

```sh
make drvarma
./bin/drvarma data/models_group1/IPC3 3 0 -mean -deseason auto -forecast 24
```

This estimates a trivariate VAR(3) on `IPC3.inp`, with a mean term and automatic
harmonic seasonal adjustment, and forecasts 24 periods. Outputs:

- `IPC3.out` — full results (parameters, tests, IRF, FEVD, residual diagnostics).
- `IPC3.forecast` — point forecasts and 95% bands, in original units.

## 2. Input file (`.inp`)

The data file is plain text. `file` on the command line is given **without** the
`.inp` extension.

```
* Free-form comment line(s) starting with a single '*'
** Frequency (1=A, 4=Q, 12=M):
 12
** Series, observations, start (subperiod year):
 3 216 1 2002
** Series names:
 IPC_ES IPC_FR IPC_DE
** Box-Cox lambda, regular differences, annual differences:
 0.0 1 0
** Data:
 69.5300 81.9400 77.7000
 69.5900 82.0400 78.0000
 ...
```

- Header lines beginning with `**` are labels; the value(s) follow on the next line.
- **Data are raw levels** (one row per observation, one column per series). The
  engine applies the transformation itself.
- `Box-Cox lambda`: `0.0` = natural log, `1.0` = identity (no transform). Other
  values apply the Box-Cox power transform.
- `regular differences` (d) and `annual differences` (D): differencing orders;
  the seasonal lag is the frequency.

The transformation actually fed to the estimator is
`scale · BoxCox_lambda(level)` then `(1-B)^d (1-B^s)^D`, optionally with the
seasonal component removed (`-deseason`). Forecasts are inverted back to the
original level scale automatically.

## 3. Command-line options

```
drvarma file p q [options]
```

`p`, `q` are the regular AR and MA orders.

| Option | Effect |
|--------|--------|
| `-mean` | estimate a mean/drift term for the (differenced) series |
| `-diagar` | restrict the AR coefficient matrices to diagonal |
| `-diagma` | restrict the MA coefficient matrices to diagonal |
| `-diagcov` | restrict the innovation covariance to diagonal |
| `-m 1` / `-m 2` | exact (default) vs approximate maximum likelihood |
| `-twostep` | Hannan–Rissanen two-step start (only meaningful with q>0) |
| `-deseason auto` | remove harmonic seasonality from series that test seasonal (default mode) |
| `-deseason force` | remove harmonic seasonality from all series |
| `-scale f` | rescale after Box-Cox; **default 100**. Rescaling improves the conditioning of the convergence criteria; the estimates are unchanged and forecasts are inverted |
| `-forecast N` | write N-step forecasts to `file.forecast` |
| `-estwin N` | fixed-parameter recursive forecasting (see §6) |
| `-volexp [α w]` | exponential-window volatility |
| `-volmov [w]` | moving-window volatility |

A **diagonal** VAR (`-diagar -diagcov`, q=0) factorizes into independent
univariate models — useful as a validation baseline against univariate software.
The **full** model (no diagonal flags) captures cross-series dependencies.

## 4. Reading the output (`.out`)

- **Estimated Parameters** — `mu`, `phi[k]_ij` (AR), `theta[k]_ij` (MA),
  `cov[i,j]` (innovation covariance Q), each with standard error, t-stat, p-value.
- **Joint hypothesis tests (Wald)** — significance of the last lag and of the
  cross-effects (directional Granger-type tests).
- **Impulse-response & variance-decomposition** — orthogonalized (Cholesky)
  responses; variable order follows the model.
- **Multivariate residual diagnostics** — Hosking portmanteau Q and multivariate
  Jarque–Bera (normality).
- **Inverse roots** — of |Φ(B)|; all moduli < 1 ⇒ stationary.
- **Per-series residual diagnostics** — summary stats, ACF/PACF, Q.

> Note on convergence: with the default `-scale 100` the optimiser normally stops
> on the gradient criterion (gradtol). On flat / highly collinear problems it may
> stop on the step criterion (steptol); both are valid optima.

## 5. Forecasts (`file.forecast`)

For each series: `Level`, 95% band (`Low95`, `High95`) in original units, plus
the monthly and annual variation rates with their standard errors.

## 6. Recursive (multi-origin) forecasting — `-estwin N`

`-estwin N` estimates the parameters **once** on the first `N` raw observations,
then, holding them fixed, forecasts from **every** origin from observation `N` to
the end of the data, writing `file.recursive`:

```
origin series horizon level
```

This supports out-of-sample evaluation across many origins **without
re-estimating** at each origin (parity with fixed-parameter univariate tooling).
Provide the full data file (training + later observations) and use together with
`-forecast H`.

```sh
# estimate on first 216 obs, recursive forecasts (24-step) from each origin
drvarma data/passthrough/WTI_IPC_ES_ext 1 0 -mean -deseason auto -forecast 24 -estwin 216
```

## 7. Graphical interface

```sh
bin/drvarma_gui
```

Load a whitespace-separated numeric file (one column per series; series are named
`y1, y2, …`), set the options, generate the `.inp` and run the engine. The GUI
also offers Johansen cointegration and VECM tools.

## 8. Worked examples

The `data/models_group1/` and `data/passthrough/` directories contain ready-to-run
cases; `MODELS_RESULTS.md` documents the reference-model battery (univariate vs
diagonal vs full VARMA, and the WTI→CPI pass-through study) with their conclusions.

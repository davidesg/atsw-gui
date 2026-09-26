# drvarma

Maximum-likelihood estimation, diagnostics and forecasting of **vector ARMA
(VARMA)** models for multiple time series, with optional harmonic seasonal
adjustment, impulse-response / variance-decomposition analysis, and
fixed-parameter recursive (multi-origin) forecasting.

A command-line program (`drvarma`) and an optional GTK graphical front-end
(`drvarma_gui`).

## License

drvarma is **free software** released under the **GNU General Public License,
version 2 or (at your option) any later version**. See the [`COPYING`](COPYING)
file for the full text.

Copyright (C) 1995–2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.

The code is **free of Numerical Recipes**: all linear algebra that previously
relied on NR routines (eigenvalues, SVD) now uses the GNU Scientific Library
(GSL), and the dynamic-memory helpers are clean reimplementations.

## Documentation

- [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) — input format, options, reading the
  output, recursive forecasting, examples.
- [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) — architecture, source
  layout, estimation pipeline, conventions.
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — how to build, contribute and keep the
  code license-clean.

## Dependencies

| Component | Needs |
|-----------|-------|
| `drvarma` (CLI) | **GSL** (`libgsl`, `libgslcblas`), **GLib 2.0**, libm, a C compiler |
| `drvarma_gui` (GUI) | the above **+ GTK+ 3.0** |

`pkg-config` is used to locate GSL and GTK+. On Debian/Ubuntu:

```sh
sudo apt install build-essential pkg-config libgsl-dev libglib2.0-dev libgtk-3-dev
```

(The GTK package is only required for the GUI.)

## Build

```sh
make            # build both drvarma and drvarma_gui
make drvarma    # CLI only (no GTK needed)
make gui        # GUI only
make clean      # remove objects and binaries
make help       # list all targets
```

Binaries are written to `bin/`. Cross-compilation to static Windows binaries
via MXE is supported (`make CROSS=x86_64-w64-mingw32.static-`; see `make help`).

## Command-line usage

```
drvarma file p q [options]
```

`file` is the input data file **without** the `.inp` extension; `p` and `q` are
the regular AR and MA orders. Results are written to `file.out` (and
`file.forecast` / `file.recursive` when forecasting).

| Option | Meaning |
|--------|---------|
| `-mean` | estimate a mean / drift term |
| `-diagar` / `-diagma` / `-diagcov` | restrict AR / MA / innovation covariance to diagonal |
| `-m 1\|2` | exact (1, default) or approximate (2) maximum likelihood |
| `-twostep` | two-step (Hannan–Rissanen) initialization (only with q>0) |
| `-deseason [auto\|force]` | harmonic seasonal adjustment of the raw series (auto = only significant series; default mode auto) |
| `-scale factor` | rescale the series after Box-Cox (**default 100**); improves the conditioning of the convergence criteria; forecasts are inverted back to original units |
| `-forecast N` | forecast N periods ahead → `file.forecast` |
| `-estwin N` | estimate parameters on the first N raw observations, then write `file.recursive` with **fixed-parameter** forecasts from every origin to the end of the data (use with `-forecast`); enables out-of-sample comparison across origins without re-estimating |
| `-volexp [α window]`, `-volmov [window]` | exponential / moving-window volatility |

### The ladder: `.pre` input (5.0)

```
drvarma A.pre B.pre [C.pre ...] p q [-diagcov] [-redet] [-fixarma] [-m 1|2] [-o NAME]
```

Each series brings its univariate model from fue (Box-Cox, deterministic
terms, differencing, mean and ARMA factors). The VARMA keeps each model on
its **diagonal**; `p` and `q` are the orders of the **cross** dynamics. Before
anything cross is estimated, the diagonal gate checks that the joint cast
reproduces the univariate models exactly, and stops the program if it does
not. Deterministic terms stay at the `.pre` values unless `-redet`. The series
must share their frequency and their last date (BUG-2). `-forecast H` forecasts
every series in its level, and `-estwin N` adds fixed-parameter forecasts from
every origin, with an out-of-sample evaluation: the univariate models (the
diagonal system) are the yardstick a VARMA has to beat. See
`docs/DESIGN-v5-ladder.md`.

### Input file format (`.inp`)

```
* optional comment line(s)
** Frequency (1=A, 4=Q, 12=M):
 12
** Series, observations, start (subperiod year):
 <nser> <nobs> <start_subperiod> <start_year>
** Series names:
 name1 name2 ...
** Box-Cox lambda, regular differences, annual differences:
 <lambda> <d> <D>
** Data:
 <row 1: nser values>
 ...
```

Data are given as **raw levels**; the engine applies the Box-Cox transform
(`lambda`, e.g. 0 = log), regular/seasonal differencing (`d`, `D`) and the
optional seasonal adjustment, and inverts everything for the forecasts.

### Example

```sh
# trivariate VAR(3) with mean and automatic seasonal adjustment, 24-step forecast
drvarma data/models_group1/IPC3 3 0 -mean -deseason auto -forecast 24
```

## Graphical interface

```sh
bin/drvarma_gui
```

Load a whitespace-separated numeric data file (one column per series; the GUI
assigns generic names `y1, y2, …`), choose the options, generate the `.inp` and
run the engine.

## Repository layout

- `src/`, `include/` — engine sources and headers.
- `gui/` — GTK front-end (plus Johansen / VECM cointegration helpers).
- `data/` — example datasets and case studies.
- `cases/` — reference-model evaluation scripts and outputs.
- `MODELS_PLAN.md`, `MODELS_RESULTS.md` — reference-model battery (plan + results).
- `NR_REMOVAL_PLAN.md` — notes on the Numerical Recipes removal.

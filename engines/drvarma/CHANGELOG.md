# Changelog

All notable changes to drvarma are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project aims to follow
[Semantic Versioning](https://semver.org/).

Versions up to 0.4 were maintained as directory snapshots (`drvarma_v.01` …
`drvarma_v.04`); 0.4.1 is the first release tracked in git.

## [5.0.0] — unreleased

drvarma joins the ladder fue → drtran → drvarma: it reads fue's `.pre` files.
Design: `docs/DESIGN-v5-ladder.md`.

### Added
- **`.pre` input (ladder mode).** `drvarma A.pre B.pre [...] p q [-diagcov]
  [-redet] [-fixarma] [-m method] [-o NAME]`. Each series carries its
  univariate model from fue on the **diagonal** of the VARMA: Box-Cox,
  deterministic terms, non-stationary operator, mean, and ARMA factors
  (seasonal and fixed-frequency ones included). `p` and `q` are the orders
  of the **cross** dynamics. The deterministic terms stay at their `.pre`
  values unless `-redet`.
- **The diagonal gate, and it closes.** Before anything cross is estimated,
  each series is fitted alone and the diagonal system is evaluated at those
  optima. If it does not match the sum of the univariate log-likelihoods,
  the program stops (exit code 5). The gate also reports how far each
  `.pre` moves: a genuine `.pre` is a fixed point. An LR test compares the
  cross dynamics against the diagonal system.
- **Declared version** in `include/version.h`, with the git commit the
  binary was built from (`5.0.0 (git <hash>[-dirty])`). It is used by
  `-version`, the `Program` line of the `.out` (both paths) and the GUI
  title.
- `tests/banco/`: byte regression of the `.inp` path against 0.4.1.
  `tests/escalera/`: the assertions of the ladder and its regression.

### Fixed
- **BUG-2**: series are crossed by date, not by position. They must share
  the frequency and the last date, or they are rejected (`lib/fuepre`,
  shared with drtran).
- The line search (`lnsrch`) never returned on a NaN or infinite objective;
  `objcfunc` now returns 1.0 for a non-finite objective. `lnsrch` now lives
  in `lib/optim/lnsrch.c`, one source for drvarma, drtran, fue and fuf.
- Heap overflow in the residual histogram (`File_HistSer`) with three-digit
  counts.

### Changed
- The `.pre` reader, `struct Tusmodel`, the univariate transformations and
  the date helpers come from `lib/` (`fuepre`, `prewhiten`, `dates`): the
  same code that drtran and its GUI use. The copy of `ObsToDate` in
  `diagnose.c`, which was identical, is gone.
- The `.inp` path is unchanged: the only difference in its `.out` is the
  `Program` line.

## [0.4.1] — 2026-06-24

First GPL-ready, Numerical-Recipes-free release; prepared for public
distribution on GitHub.

### Removed
- **All Numerical Recipes code** (concentrated in `src/nlatools.c`). The codebase
  now contains no NR routines or copyright markers.

### Changed
- **Linear algebra now uses the GNU Scientific Library (GSL):** eigenvalues
  (`eigenqr` via `gsl_eigen_nonsymm`) and SVD (`svdcp`/`svsol` via
  `gsl_linalg_SV_decomp`/`SV_solve`). Public signatures are unchanged, so callers
  are unaffected. Verified numerically transparent (identical objective,
  parameters, eigenvalue moduli and Wald statistics on the reference cases).
- **Dynamic-memory routines** (`vector`/`matrix`/`tensor`/`free_*`/`nrerror`)
  reimplemented cleanly (Treadway & Guerrero 2009).
- **Default rescale factor is now 100** (`-scale`). Rescaling improves the
  conditioning of the convergence criteria (per J.A. Mauricio); the optimum and
  forecasts are unchanged and the optimiser converges on the gradient criterion.
- **GUI** manages generic series names (`y1, y2, …`) consistently across the
  interface, the generated `.inp` and the log output.

### Added
- **`-estwin N`**: fixed-parameter recursive (multi-origin) forecasting. Estimate
  once on the first N observations, then forecast from every origin to the end of
  the data, written to `file.recursive` — for out-of-sample evaluation without
  re-estimating at each origin.
- **GPL licensing**: `COPYING` and `LICENSE` (GNU GPL v2 or later) and a GPL
  notice in every source file.
- **Documentation**: `README.md`, `docs/USER_GUIDE.md`, `docs/DEVELOPER_GUIDE.md`,
  `CONTRIBUTING.md`, plus the reference-model notes (`MODELS_PLAN.md`,
  `MODELS_RESULTS.md`, `NR_REMOVAL_PLAN.md`).
- **Continuous integration**: `.github/workflows/ci.yml` builds the engine and
  GUI, runs a smoke-test estimation/forecast, and scans for any reappearance of
  Numerical Recipes copyright markers.

### Removed (dead code)
- Unused NR eigenvalue helpers (`tred2`, `balanc`, `elmhes`, `eigenql`) and the
  now-redundant `eigenql` prototype.

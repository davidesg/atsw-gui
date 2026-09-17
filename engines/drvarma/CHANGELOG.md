# Changelog

All notable changes to drvarma are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project aims to follow
[Semantic Versioning](https://semver.org/).

Versions up to 0.4 were maintained as directory snapshots (`drvarma_v.01` …
`drvarma_v.04`); 0.4.1 is the first release tracked in git.

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

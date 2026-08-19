# drvec — VEC model EML estimation (Mauricio 2006)

Exact maximum likelihood estimation of **partially nonstationary vector ARMA models**
via the transformation described in:

> Mauricio, J.A. (2006). "Exact maximum likelihood estimation of partially
> nonstationary vector ARMA models." *Computational Statistics & Data Analysis*,
> 50, 3644–3662.

## Architecture

```
drvec/
├── src/
│   ├── drvec.c          ← VEC front end (vec_shootx, init_guess, main)
│   ├── elfvarma.c       ← exact log-likelihood, Algorithm AS 311 (untouched)
│   ├── drvmlest.c       ← estimation driver (untouched)
│   ├── qnewtopt.c       ← factored quasi-Newton optimiser (untouched)
│   ├── nlatools.c       ← linear algebra and memory management (untouched)
│   ├── fue_pre_reader.c ← reader for the suite's parameter files
│   ├── fue_bridge.c     ← operator expansion and deallocation
│   └── diagnose_mv.c    ← multivariate residual diagnostics
├── include/
├── docs/                ← documentation; start at docs/README.md
├── literature/          ← the works cited, listed in docs/REFERENCES.md
├── datasets/            ← data, including synthetic cases of known rank
├── tests/               ← run_tests.sh, fixtures, and the .pre reader harness
├── tools/               ← comparison against the reference implementation
├── Makefile
└── README.md
```

**Documentation.** This file covers installation and the command line. The
model, the inferential basis, the measured results and the development history
are documented in [`docs/`](docs/README.md).


## The Mauricio transformation

The core insight of Mauricio (2006) is that a partially nonstationary VARMA in
VEC form can be transformed into a **standard stationary VARMA** on:

```
Ybar_t = (nabla Y_{2t}',  W_t')'
```

where:
- `s = M - r` variables are not normalised (Y_{2t} in levels)
- `r` cointegrating relations: W_t = Y_{1t} + B_2' Y_{2t}

The transformation matrices are:

```
Cbar = [ 0_{s x r}    I_s      ]    Cbar^{-1} = [ -B_2'    I_r ]
      [    I_r       B_2'     ]                 [ I_s       0   ]

Hbar = [ 0_{s x s}   0_{s x r} ]
      [ 0_{r x s}      I_r     ]
```

such that `Cbar * nabla Y_t = Ybar_t - Hbar * Ybar_{t-1}`.

The resulting VARMA on Ybar_t is **stationary** and estimable by standard EML.

## Usage

```
drvec file p q r [-mean] [-case 1|2|3] [-diagar] [-diagma] [-diagcov] [-m 1|2]
                 [-differenced] [-fixb2 [v]] [-lrtest]
                 [-writeres pfx] [-writeinp pfx] [-seed pfx] [-seedybar pfx]
                 [-alpha file] [-weakex i] [-eval]
```

| Argument | Description |
|----------|-------------|
| `file`   | Data file name (without `.inp`) |
| `p`      | AR order of stationary VARMA on Ȳ_t |
| `q`      | MA order |
| `r`      | Cointegration rank, `0 <= r < M`; ignored with `-lrtest`. `r = 0` is the no-cointegration model and is a first-class configuration: it is the ladder's diagonal rung |

| Option | Description |
|--------|-------------|
| `-mean` | Include mean in Ȳ_t |
| `-case 1\|2\|3` | Deterministic specification (Remark 6) |
| `-diagar` / `-diagma` / `-diagcov` | Diagonal restrictions |
| `-m 1\|2` | Exact (1) or approximate (2) ML |
| `-differenced` | Legacy layout: cols 1..s already hold ∇Y₂ (see below) |
| `-fixb2 [v]` | Hold B₂ fixed instead of estimating it (see below) |
| `-lrtest` | Sequential LR test for the cointegration rank (not with `-differenced`) |
| `-writeres pfx` | Write one `pfx.<i>.inp` per equation holding the conditional-regression residuals, for `ART`/`fue` to identify and estimate. Then stop |
| `-writeinp pfx` | The same, but one file per **component of Ȳ**. For identification; **not** a seeding source (see below) |
| `-seed pfx` | Read `pfx.<i>.pre` (or `.inp`) written by `fue` from `-writeres` output, and seed the MA block Θ |
| `-seedybar pfx` | Seed the **whole univariate block** (Φ*, Θ̄, Σ) from `-writeinp` output, undoing the transformation. Valid at `r = 0`; above that it is measured to make the start worse, and `drvec` warns |
| `-alpha file` | Impose `α = Aψ` with `A` read from `file` (Johansen–Swensen H₁(r)), and report the LR against the free model |
| `-weakex i` | Shorthand for the `A` that declares equation `i` weakly exogenous |
| `-eval` | Evaluate the likelihood **at the starting point** and stop, without optimising |

### The bridge to the suite

`drvec` writes `.inp` files — a *specification* — and never a `.pre`, which is a
claim that the values are an optimum and may only be made by the program that
optimised. `fue` estimates each file and leaves the `.pre`; `drvec` reads the
numbers back. It never calls `fue`'s cast at run time, which is deliberate: that
cast keeps its state in module globals and is not reentrant.

At `r = 0` with diagonal structure — the ladder's diagonal rung — the two
contracts of the suite hold and `drvec` checks them itself: the joint likelihood
evaluated at the stored `.pre` values equals the sum of the univariate ones
(agreement 1.8e-5, the format's own rounding), and the fitted value cannot be
below the evaluated one (certificate +2.4e-7 ≥ 0). **Above that rung the
univariate information does not transport**: the marginal of a component of Ȳ is
not the joint's diagonal block, and the AR is over-determined. Measured, at
r = 1 the seed starts 17 units below the cold start.

Details, numbers and provenance in
[docs/SUITE_INTEGRATION.md](docs/SUITE_INTEGRATION.md).

## Input format (.inp)

```
* comments
12                              ← frequency (1=A, 4=Q, 12=M)
2 124 1 1965                    ← M nobs start_sub start_year
houses_sold housing_starts      ← M series names, in column order (Y₂ then Y₁)
1.0 0 0                         ← lambda d D (usually 1.0 0 0)
<data: nobs rows, M columns>
```

Column order is always **Y₂ block first, Y₁ block second**, where `Y₁` is the
`r`-dimensional block normalised in `B = [I_r; B₂]` and `Y₂` the remaining
`s = M − r`:

| | cols 1..s | cols s+1..M |
|---|---|---|
| **default** | **Y₂ in levels** (differenced internally) | **Y₁** in levels |
| `-differenced` | ∇Y₂ (pre-differenced) | Y₁ in levels |

**Supply levels.** They are the default because `-differenced` leaves the levels
of Y₂ unknown, so drvec has to reconstruct them by cumulating from an arbitrary
zero origin. That shifts `W_t` by `B₂'c`: harmless in cases 2 and 3, where the
free `E[W]` absorbs it, but **wrong in case 1**, where `E[W] = 0` leaves nothing
to absorb it (drvec warns). It also makes the reported `Ê[W]` incomparable with
published values, and it rules out `-lrtest`. The levels layout consumes one
observation to form the differences.

The layout actually used is echoed on the console and in the `.out` header, so a
file read the wrong way is visible in the result:

```
Layout : all series in levels (61 of 62 observations used)
```

> **Note for files written before 2026-08-17.** The default used to be the
> pre-differenced layout. `datasets/mauricio/mink_muskrat.inp` has been
> regenerated in levels; `data/AL.inp` is still in the legacy layout and is
> annotated as needing `-differenced`. Check any other `.inp` before use.

## Rank testing

```
drvec file p q 0 -case 2 -lrtest
```

Estimates r = 0..M−1 and reports, per rank, the number of parameters, the
log-likelihood, AIC and BIC, and for each consecutive pair the statistic
`2·[L(r+1) − L(r)]` (Mauricio 2006, Remark 5 and Table 3) against the
non-standard asymptotic critical values (λ-max form; cases 1 and 2 only).

Incompatible with `-differenced`: the column split is `s = M − r`, so a file that
is already differenced cannot be re-read at another rank.

`r = 0` is the no-cointegration null (Π = 0, a plain VARMA on ∇Y), so the first
comparison is the one that matters most: is there cointegration at all? `r = M`
would be a stationary process in levels and is not expressible, so the sequence
ends at `r = M−1`. `r = 0` can also be run on its own, which is what makes the
diagonal rung inspectable.

A **negative** statistic is flagged as not interpretable — rank `r` is nested in
`r+1`, so it proves one of the two fits did not converge.

## Holding the cointegrating vector fixed (`-fixb2`)

`-fixb2` removes B₂ from the parameter vector (`npar` drops by `s·r`). Two uses,
and the difference matters:

```
drvec file p q r -case 2 -fixb2 0      # B₂ pinned at 0 — an a-priori restriction
drvec file p q r -case 2 -fixb2        # B₂ pinned at its static-OLS estimate
```

- **With a value**, every entry of B₂ is pinned at it. The restriction is chosen
  a priori, so `2·[L(free) − L(fixed)]` **is** a valid LR test, χ² with `s·r`
  degrees of freedom. This is how Mauricio (2006, Table 5) tests `B = [1,0]′`
  — which in this parameterisation is `-fixb2 0`.
- **Without a value**, B₂ is held at the static-OLS estimate that `init_guess`
  computes. Useful as a **warm start** and as a conditioning check: the
  restricted fit often converges cleanly where the free one stops on a failed
  line search. But the restriction is then **data-chosen**, so the LR statistic
  against the free model is *not* a valid test. The `.out` says which case it is.

## Deterministic cases (Mauricio 2006, Remark 6)

| Case | E[nabla Y_2] | E[W] | Description |
|------|-------------|------|-------------|
| 1    | = 0         | = 0  | No drift, zero-mean equilibrium |
| 2    | = 0         | ≠ 0  | No drift, non-zero equilibrium mean (-mean) |
| 3    | ≠ 0         | ≠ 0  | Restricted drift, non-zero equilibrium (-mean) |

## Building

```sh
make          # build bin/drvec
make clean    # remove objects
make rebuild  # clean + build
```

Requires: **GSL** (`libgsl-dev`), gcc.

## License

GPL v2 or later. Based on code by J.A. Mauricio (1995–2006),
A.B. Treadway, and D.E. Guerrero.

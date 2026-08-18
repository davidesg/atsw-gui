# Getting started

## Build

```sh
make          # needs gcc and GSL (pkg-config gsl)
make test     # 54 checks; takes a couple of minutes
```

`make test` builds a second small binary, `bin/pre_probe`, which the suite uses
to inspect `.pre` files. Nothing else is generated.

## A first model

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2
```

The four positional arguments are the file (without `.inp`), then `p`, `q`, `r`:
the AR and MA orders of the stationary VARMA on `Ȳₜ`, and the cointegration rank.
The result goes to the console and to `mink_muskrat.out`.

## Reading the `.out`

```
DRVEC — VEC(1) EML Estimation (Mauricio 2006)

M = 2, r = 1, s = M-r = 1
Stationary VARMA(2,1) on Ȳ_t
Case   : 2
Layout : all series in levels (61 of 62 observations used)
```

**Check the layout line first.** A file read with the wrong layout gives a
plausible fit of a different model, and this line is where that is visible.

```
  OPTIMIZER CONVERGED after 66 iterations
  Convergence criterion: scaled distance between last two steps <= steptol
```

The three possible endings and what to make of them are in
[CONVERGENCE.md](CONVERGENCE.md). This one is benign; *"last global step failed
to locate a lower point"* is the common one and needs more care.

```
sigma2 :    0.0480498305
logelf :    6.4786201604
```

`logelf` is the exact unconditional log-likelihood. `sigma2` is the covariance
scale that the concentrated objective factors out — see below.

```
E[W] =
      7.662021  (sd =   0.509958)
Lambda (M x r) =
      0.833411
      0.651188
```

`E[W]` is the level of the equilibrium relation (case 2 estimates it, case 1
forces it to zero). `Λ` is the adjustment matrix — Johansen's `α`: how strongly
each equation responds to a deviation from equilibrium.

```
F[1] (M x M) =        Theta[1] (M x M) =
   0.458269 -0.542023    -0.042477 -0.907771
  -0.793693  0.033748    -1.030168  0.103040
```

The short-run dynamics on `∇Y`, and the moving-average matrix. Both in the
convention `I − F₁L − …` and `I − Θ₁L − …`.

```
Q (M x M, lower triangle; Q[1][1] = 1 by normalisation) =
      1.000000
      0.513584    1.329699
Sigma = sigma2 * Q  (innovation covariance of A_t) =
      0.048050
      0.024678    0.063892
```

**`Q` is not the covariance.** The likelihood is concentrated, so only the
*ratios* in `Q` are identified and the scale is reported through `sigma2`. The
innovation covariance is the product, printed underneath. Reading `Q` as `Σ`
gives the right shape and the wrong magnitude.

```
B2 (s x r) =
     -0.240516

Cointegration matrix B = [I_r; B2] (M x r) :
  row 1:     1.000000
  row 2:    -0.240516
```

The cointegrating vector, normalised on the `Y₁` block. Here it says that
`log(mink) − 0.2405·log(muskrat)` is stationary. **Note the sign convention:**
`Wₜ = Y_{1t} + B₂′Y_{2t}`, so this `B₂` is Johansen's `β` with the opposite sign.

## Three things worth doing next

**Test the rank instead of assuming it:**

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 0 -case 2 -lrtest
```

**Ask who adjusts** — the economic question the model exists to answer:

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2 -weakex 2
```

```
  H(r)  libre        : logL =    6.4786201604
  H1(r) restringido  : logL =    4.6838606765
  LR = 3.589519, 1 g.l., p = 0.058145
```

Mink does not adjust, at the 5 % level: it is weakly exogenous, and the muskrat
equation carries the correction.

**Get a second opinion on a fit that stopped badly:**

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2 -fixb2
```

Holding the cointegrating vector fixed often converges where the free model does
not, and the gap between the two log-likelihoods tells you how much of the
difficulty lives in that one direction.

## Where to go from here

* [USAGE.md](USAGE.md) — every option and the input format
* [MODEL.md](MODEL.md) — what is being estimated, and the conventions that bite
* [CONVERGENCE.md](CONVERGENCE.md) — **read before trusting a fit**

# Homologation register

*Which cases `drvec` has been run against, what it reproduces, to what tolerance,
and — the part that makes this a register rather than an advertisement — what it
does not reproduce. Measured 2026-08-18 on the build of that date; every row can
be re-run with the command shown.*

A note on what "reproduces" can mean here. Three different things are being
checked, and they are not equally strong:

* **an identity** — something that must hold by construction, so a mismatch is a
  defect with no other explanation;
* **an external result** — an independent program's answer on the same data;
* **a published number** — a value from a paper, whose exact configuration is
  usually not fully recoverable.

The third is the weakest, and it is where the open item lives.

---

## 1. Identities — these must hold exactly

| what | measured | status |
|---|---|---|
| the transformation against the closed form of Mauricio (2005) for M=2, r=1 | difference **0.000e+00**, term by term | ✔ |
| `elf` against a multivariate normal computed by hand | **2·10⁻⁸** | ✔ |
| factorisation: joint `r=0` all-diagonal = sum of the univariate fits, `q = 0` | worst over the bank **3e−11** | ✔ |
| the same identity with `q ≥ 1` | worst **1.4e−4**, and it is the `ξ` truncation: see §1b | ✔ |
| `Σ = P D P′` reconstructs `Σ` (M=3) | worst entry **< 1e−5** | ✔ |
| a `Θ = 0` seed reproduces the cold start | **bit for bit** | ✔ |
| the two layouts of the same model agree on \|Σ̂\| | within **0.00005** | ✔ |
| `-weakex i` equals the equivalent `-alpha` file | **exactly** | ✔ |

```sh
make test          # all of the above, plus 56 more checks
```

## 1b. The one identity that is not exact, and why

The factorisation identity holds in algebra, and with `q = 0` it holds in the
machine too — worst disagreement over the twelve bank series **3·10⁻¹¹**. With
`q ≥ 1` it does not, and the reason is not a defect: `elf` truncates the `ξ`
sequence when the sum of the absolute values of its term falls below `xitol`
(`elfvarma.c`, `cxi` [1]), and **the joint system and the univariate ones do not
truncate at the same term** — the joint sums `m` entries where each univariate
sums one. So the two sides agree only as far as the approximation reaches.

Measured by lowering `xitol` and rebuilding, on the largest gap in the bank
(Milan, `p = 2, q = 1`, the diagonal rung):

| `xitol` | `joint − sum` |
|---|---|
| `1e−3` (the default) | 1.385e−04 |
| `1e−8` | 3.100e−09 |

Five orders of tolerance, five orders of gap: the disagreement **is** the
truncation, at a factor of about 0.15, and not a fault upstream of it.

This corrected the check rather than the estimator. The gate's threshold was a
fixed `1e−4`, which declared Milan **NOT VERIFIED** while passing Angers, whose
*relative* disagreement (6.3e−6) is three times larger than Milan's (2.1e−6) —
it was ranking cases by the size of their log-likelihood instead of by their
agreement. The tolerance is now the truncation itself: `xitol` when `q > 0`,
and `1e−6` when `q = 0`, where the identity must hold exactly. Both halves are
in the suite, and the strict half is what keeps the loose half honest. No
estimate moves: the change is to a verdict, not to a fit.

## 2. External results

### 2.1 Cointegration rank against `urca::ca.jo`

| data | `drvec -lrtest` | reference | |
|---|---|---|---|
| `mink_muskrat` (M=2) | LR(0→1) = **25.62** vs 15.67 at 5 % → **r = 1** | r = 1, the published analysis of these data | ✔ |
| `urca_UKconsumption` (M=3) | LR(0→1) = 25.52 (5 %), LR(1→2) = **70.12** (1 %) → **r = 2** | `ca.jo(type="eigen")` gives r = 2 | ✔ |

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 0 -case 2 -lrtest
```

The critical values are the non-standard Johansen ones (λ-max form), from
Osterwald-Lenum (1992), as implemented in the `urca` package (Pfaff, 2008). MA terms do not affect the asymptotic
distribution (Yap and Reinsel, 1995, Theorem 3; see Mauricio, 2006, Remark 5). **Case
3 is not tabulated here**, so a case-3 rank test reports the statistic without
critical values.

### 2.1b The cointegrating vector against Johansen, on eight pairs

The rank rows above compare a decision. This compares **the estimate itself**,
which is a sharper test: `drvec` with `q = 0` and Johansen's reduced-rank
regression fit the *same model*, by two routes with nothing in common — a
closed-form eigenvalue problem against a numerical optimisation of the exact
likelihood.

Measured on eight annual wheat-price pairs (European markets against London,
1700–1813, n = 90–113), same files on both sides, cointegrating vector
renormalised on the same variable:

| | agreement |
|---|---|
| mean \|difference\| in the cointegrating coefficient | **0.019** |
| worst pair | **0.052** |
| best pair | 0.0003 |

Two implementations with no shared ancestry agreeing to 0.02 on eight real
samples is the strongest external check this program has, and it is on the
quantity the program exists to produce.

It also calibrates what comes next: **the same comparison with `q = 1` — the MA
that Johansen has no way to represent — differs by 0.223 on average and 0.402 at
worst**, i.e. an order of magnitude more than the difference between estimation
methods. Raising Johansen's lag order to approximate the MA does not close it:
the rank test loses the cointegration by `k = 3–4` on these sample sizes before
the approximation converges.

The agreement is established **across orders**, not at one point: sweeping
`k = 1, 2, 3` against `p = k+1` gives 24 comparisons, all between 0.0003 and
0.052. That also confirms the `p = k+1` correspondence empirically — a wrong
mapping would agree at one order and nowhere else.

**At each one's own optimum they diverge**, and the reason is structural rather
than numerical: the lags Johansen needs to whiten the residuals cost `m²` each,
and by `k = 3` the rank test has collapsed. At its own AIC order Johansen reaches
a usable rank in 2 of 8 pairs; `drvec` with `q = 1` gives `r = 1` with clean
residuals in all eight, on 14 parameters against Johansen's 16 at `k = 3`.

Full tables, the caveats, and what it does **not** establish — including that
AIC/BIC are not comparable across the two programs, since one likelihood is
conditional and the other exact — are in
[COMPARISON_JOHANSEN.md](COMPARISON_JOHANSEN.md).

*Source of the data and the full tables:*
`~/Dropbox/Cycles/Analysis/EJERCICIO_DRVEC.md`, and
`comparar_johansen.py` beside it re-runs the comparison.

### 2.2 Cointegration rank against known truth

The rows above compare against another program. These compare against **the
truth**, because the data was generated to have it — which is a stronger claim
and the only way to ask whether the test invents relations that are not there.

| data | truth | `drvec` | |
|---|---|---|---|
| `datasets/synthetic/rank0.inp` — three independent random walks | r = 0 | LR(0→1) = **4.06** vs 19.77 at 10 % → not rejected | ✔ |
| `datasets/synthetic/rank2.inp` — three series, one common trend | r = 2 | LR = **88.95** and **73.06**, both at 1 % | ✔ |
| `datasets/synthetic/badnorm.inp` — one relation plus an independent walk | r = 1 | LR(1→2) = **20.27** vs 20.20 at 1 % → **r = 2** | ✘ over-rejects, *just* |

Both ends of the range are recovered. The third is the interesting one, and it
is not a defect of the program — it is the finite-sample size of the test, which
the next row measures.

```sh
bin/drvec datasets/synthetic/rank0.inp 2 0 0 -case 2 -lrtest
```

### 2.3 The size of the rank test in finite samples — measured

Over **60 replications** of a process with a true rank of 1, at n = 120, with the
sequential test at the 5 % level, comparing the asymptotic tables against
`-bootstrap 100`:

| rank selected | asymptotic | bootstrap |
|---|---|---|
| r = 0 | 1 (1.7 %) | 1 (1.7 %) |
| **r = 1 — correct** | 41 (**68.3 %**) | 47 (**78.3 %**) |
| r = 2 — over-rejection | 18 (**30.0 %**) | 12 (**20.0 %**) |

So the asymptotic test over-rejects at **six times** its nominal 5 %, and the
bootstrap cuts that to four times. The improvement is real rather than noise:
the comparison is paired, and of the 6 replications where the two disagree, **all
6 go the bootstrap's way** (McNemar exact, p = 0.031).

**A size of 20 % against a nominal 5 % remains substantially distorted.** The
bootstrap improves the calibration without correcting it. On samples of this length a
rank decision near a critical value stays undecided whichever route produced it.

*A correction to an earlier figure in this register.* The first version of this
row reported over-rejection of ~15 %, from 20 replications. Extending to 60 puts
it at 30 % — the first 20 gave 3 and the next 40 gave 15. With n = 20 the
standard error of such a proportion is about 8 points, so the first estimate was
a favourable draw rather than a
different measurement. The 60-replication figure is the one
to quote, and it carries about 4.6 points of standard error itself.

**This is the measurement that justified the parametric bootstrap**, which now
exists as `-bootstrap N`. It replaces the tables with percentiles simulated from
the fitted model, and they are visibly different — on mink–muskrat, 14.90 / 17.41
/ 21.16 against the tabulated 13.75 / 15.67 / 20.20. It also supplies critical
values for **case 3**, which has none tabulated here.

The four cases with known or external rank are recovered by **both** routes:

| case | truth | asymptotic | bootstrap |
|---|---|---|---|
| `mink_muskrat` | r = 1 | r = 1 | r = 1 |
| UK consumption (M=3) | r = 2 (`ca.jo`) | r = 2 | r = 2 |
| `synthetic/rank0` | r = 0 | r = 0 | r = 0 |
| `synthetic/rank2` | r = 2 | r = 2 | r = 2 |

Whether the bootstrap actually *corrects* the over-rejection is a separate
question from whether it runs, and it needs its own Monte Carlo — see
[PLAN_BETA.md](PLAN_BETA.md) F4.1.

### 2.4 The univariate constants, three independent ways

The factorisation gate compares the joint `r = 0` diagonal fit against two
univariate exact-ML ARMA(1,1) fits with no mean. Those two constants are now
established by three programs of different lineage:

| source | ∇log muskrat | ∇log mink |
|---|---|---|
| `drvarma`'s Python port | −20.0580 | −14.5698 |
| `fue` 1.13, μ fixed at 0 | −20.057954 | −14.569865 |
| `drvec` itself, evaluating the `.pre` model with `elf` | −20.057976 | −14.569847 |

Agreement to **6·10⁻⁵**, which is the rounding of the `%.6f` coefficients stored
in the `.pre`.

*A trap this exposed, worth keeping:* with the mean **free** `fue` reaches
−19.9081 on the muskrat series, 0.15 better. Reading that as a discrepancy is a
mistake that was actually made during development. The gate's model has no mean,
because the `drvec` run it is compared against is `-case 1`.

## 3. Published numbers — the open item

`mink_muskrat` is Table 2/4/5 of Mauricio (2006), and this is where the register
has to be honest.

| | published | `drvec` | |
|---|---|---|---|
| rank | r = 1 | **r = 1** | ✔ |
| Λ̂₁ (Table 4) | 0.8392 | **0.8371** | ✔ close |
| EML log-likelihood (Table 5) | 15.61 / 15.13 | **6.4786** | ✘ **not reproduced** |
| \|Σ̂\| accompanying it | 0.001788 | 0.002461 | ✘ |

What is established about the gap: it is **not** in the engine (validated
independently), **not** in the transformation (verified against the paper's own
closed form), and **not** in the data (checked against the canonical source). It
is localised in the `Σ̂` that the published log-likelihood implies, and it cannot
be closed with the material available — that would need the original program or
correspondence with the author.

So `drvec` is **not** homologated against those log-likelihoods. It is
homologated against a criterion on `|Σ̂|`, which is invariant across the
representations, and that criterion is:

| | \|Σ̂\| | |
|---|---|---|
| global search over the same model and data | ~0.00230 | the target |
| the paper's own CML column | 0.002312 | agrees with the target |
| Chan & Wallis (1978), AR(4)+MA(1) | 0.00246 | calibrates magnitude; **not** the target — different model, different sample |
| `drvec`, four equivalent configurations, one start | 0.00246 – 0.00251 | agree with each other, ~8 % above |
| **`drvec`, the same four with `-multistart 60`** | **0.002344 – 0.002358** | **spread 0.000014, and 1.6 % above** |

The single-start row is what the program reports by default, and is the
appropriate figure for a single unattended run. The multi-start row is what it gives when asked to look
properly, and it is the one to quote: the spread across four mathematically
equivalent set-ups collapses to 0.000014 and the level lands within 1.6 % of the
global-search reference.

Not closed, and the residue is stated: 0.002346 against 0.002311 with the
invertibility gate, or 0.002294 without it. Both `drvec` and the reference stop
*at* that gate — measured, `max|λ(Θ₁)| = 1.000050` in both — so what separates
them is which point of the boundary is reached, not the boundary itself.

That the estimate rests on the gate is no longer a fact recorded only here: every
fit now reports the moduli of the roots of both operators and marks any root on
the unit circle. It has also been established to be general rather than
particular to one set-up. Cases 1, 2 and 3, with and without `-diagma`, and with
sixty restarts, all stop with the smallest modulus of `Θ(B)` equal to 0.99995;
only `p = 1`, which fits far worse, escapes it. The reported optimum for these
data is therefore a **constrained** one, which is the reason the unconstrained
Hessian cannot be formed there and `-fdhess` declines to report standard errors
from it ([CONVERGENCE.md](CONVERGENCE.md) §2b).

## 3b. The specification searches, redone with `q ≥ 2`

Until 2026-08-19 no model with `q ≥ 2` could be estimated: an allocation defect
in the engine's supporting library corrupted the heap and aborted the run
whenever the likelihood routine requested its cross-covariance array with a
negative lower bound, which it does for two or more moving-average lags. The
defect was not particular to `drvec` and was not new — it had been diagnosed and
fixed elsewhere in the suite on 2026-06-15, and `drvec` was carrying the pre-fix
copy ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5). Every specification
search recorded here was therefore bounded at `q ≤ 1` by a defect rather than by
a modelling decision. They have since been redone.

**The rebuilt exercise reproduces the recorded one exactly.** Before extending
anything, the eight pair files were rebuilt from the same sources and the
recorded fits re-run: all sixteen cointegrating coefficients and all sixteen
portmanteau p-values at `q = 0` and `q = 1` reproduce to four decimals, as does
the rank statistic quoted for Milan. What follows is therefore an extension of
the same measurement, not a different one.

### The extension itself

At the specification the record uses, `p = 2`, adding the second moving-average
lag raises the log-likelihood on all eight pairs and leaves the residuals white:

| pair | `q=1`: B̂₂, portm., npar | `q=2`: B̂₂, portm., npar | Δ logL |
|---|---|---|---|
| Milan | −0.4465, 0.66, 14 | −0.4912, 0.80, 18 | +5.45 |
| Strasbourg | −0.6955, 0.93, 14 | −0.5137, 0.88, 18 | +4.38 |
| Utrecht | −0.6248, 0.95, 14 | −0.6265, 0.98, 18 | +3.78 |
| Vienna | −0.5698, 0.97, 14 | −0.5530, 0.98, 18 | +2.96 |
| Aix | −0.7594, 0.78, 14 | −0.7315, 0.84, 18 | +5.20 |
| Arévalo | −0.6174, 0.88, 14 | −0.6012, 1.00, 18 | +5.06 |
| Angers | −0.6355, 0.64, 14 | −0.5480, 0.95, 18 | +3.71 |
| Penn | −1.1013, 0.76, 14 | −1.1228, 0.66, 18 | +9.37 |

The improvement is not free: 18 parameters against 14, and the Schwarz criterion
prefers `q = 1` on seven of the eight.

### What a stated criterion selects

A criterion applied identically to all eight, over the grid `p ∈ {1,2,3}`,
`q ∈ {0,1,2}` with 40 restarts: **the most parsimonious specification whose
residuals pass the portmanteau test at 5 %, ties broken by the Schwarz
criterion.** It is adequacy first and parsimony second, which is the rule the
recorded exercise applied informally when it declined to select on an information
criterion alone.

| pair | selected | `npar` | portm. | B̂₂ | smallest MA root |
|---|---|---|---|---|---|
| Milan | `p=1, q=1` | 10 | 0.058 | −0.5459 | 1.905 |
| Strasbourg | `p=1, q=2` | 14 | 0.756 | −0.4065 | **1.000** |
| Utrecht | `p=1, q=0` | 6 | 0.080 | −0.8377 | — |
| Vienna | `p=1, q=0` | 6 | 0.428 | −0.8833 | — |
| Aix | `p=1, q=0` | 6 | 0.268 | −0.8758 | — |
| Arévalo | `p=1, q=0` | 6 | 0.267 | −1.0300 | — |
| Angers | `p=1, q=0` | 6 | 0.299 | −1.1058 | — |
| Penn | `p=1, q=1` | 10 | 0.108 | −1.0982 | **1.000** |

**Opening `q ≥ 2` changes the selected specification for exactly one pair of the
eight**, Strasbourg, which is also the one pair the record identifies as needing
an MA term that `q = 1` fails to supply. So the bound the defect imposed was
real but narrow, and the recorded conclusions do not turn on it.

### What the extension does change: the roots

The searches also had to be redone because `drvec` now reports the roots of the
estimated operators, and what they show bears on how the recorded columns should
be read.

**At the recorded specification `p = 2, q = 1`, seven of the eight pairs have a
moving-average root on the invertibility boundary** (moduli 0.99996 to 1.00000).
Those fits are constrained optima: the standard errors reported beside them are
not defined along the binding direction. This was not visible when the columns
were measured, because the program did not report roots. At `q = 2` it holds for
all eight.

**It is over-specification of the moving-average operator, not a property of the
series, and the entry gate localises it.** At the diagonal rung — `r = 0` with
diagonal `Φ`, `Θ` and `Σ`, where the likelihood factorises into the univariate
models — not one of the sixteen roots is anywhere near the unit circle:

| smallest MA root modulus | Milan | Stras. | Utrecht | Vienna | Aix | Arévalo | Angers | Penn |
|---|---|---|---|---|---|---|---|---|
| **entry gate**: `r=0`, `Θ` diagonal | 1.200 | 1.147 | 1.135 | 1.095 | 1.101 | 1.199 | 1.040 | 1.192 |
| `r=0`, `Θ` free | 1.354 | **1.000** | 1.136 | 1.597 | 1.060 | 1.356 | **1.000** | **1.000** |
| `r=1`, `Θ` diagonal | **1.001** | **1.000** | **1.000** | **1.000** | **1.000** | 1.455 | **1.001** | **1.000** |
| `r=1`, `Θ` free (the tabulated fit) | **1.000** | 1.195 | **1.000** | **1.000** | **1.000** | **1.000** | **1.000** | **1.000** |

The gate was estimated as the suite's ladder prescribes and not from a cold
start: `-writeinp` emitted the two component files, `fue` estimated them and
wrote the `.pre`, and the gate was then seeded from those. It was also re-run
with the deterministic components of the **article's own univariate `.pre`
files** subtracted, which moves the London root from 1.199 to 1.215. All three
routes agree, and none produces a root on the boundary.

Two further checks make the reading firm. The gate's first root is London's, and
London is common to all eight pairs, so it must depend only on the London
subsample — and it does: the four pairs sharing the `n = 113` sample all return
1.19863, while Aix and Angers, both `n = 90`, return 1.10095 and 1.10093. And
where the article's own univariate models carry a moving-average term the gate
reproduces them closely: Vienna 1.095 against the article's 1.148, Aix 1.260
against 1.302, Angers 1.040 against 1.068.

The table then localises the condition to the two things that add moving-average
parameters the series do not support. Five of the nine article models carry **no
moving-average term at all**, so a free `2 × 2` `Θ₁` is heavily over-specified on
these data; freeing the off-diagonal entries at `r = 0` already puts three pairs
on the boundary, and raising the rank puts almost all of them there. It is not
autoregressive–moving-average cancellation either: at `p = 2, q = 1` the
autoregressive roots stay between 1.30 and 2.63 while the moving-average root
sits at one. Consistently with all of this, the criterion of the previous section
selects `q = 0` for five of the eight pairs.

### And a limit on the rank test that this exposed

Running the sequential rank test across `q` put a further limitation on record.
With 20 restarts the test selects `r = 1` for six of the eight pairs at `q = 1`;
Aix and Penn do not reject `r = 0`. Aix, examined directly, is **optimiser-limited
rather than data-limited**: the two log-likelihoods give `LR = 10.53` at 20 and
60 restarts and `LR = 16.65` — rejection at 5 % — at 150, because the `r = 1` fit
does not reach its optimum until then. Penn does not reject even at 150.

This does not contradict the record, which states that `drvec` fitted at `r = 1`
leaves clean residuals on all eight, and that reproduces exactly. It does show
that the sequential test's verdict on these samples can depend on how hard the
optimiser is asked to look, which is a caution the earlier tables did not carry.

## 4. Cases run without an external reference

These establish that the program handles the shape, not that the answer is right.

| case | | |
|---|---|---|
| `urca_denmark`, M = 5, r = 2 | logL 828.8447, converges | the only case where `s > 1` **and** `r > 1`, so it is the only one where a transposed read of `B₂` is detectable at all. Mixes logarithms with interest rates, which is what makes it the test of the variance-ratio seeding (+79.26) |
| `data/AL.inp`, legacy layout | logL −318.8131 | regression baseline only; the file is annotated as needing `-differenced` |
| `datasets/synthetic/badnorm.inp` | normalisation share **0.6 %** | built so the normalisation alarm has something to fire on. Its rank is 1 by construction, and it is also the draw where the rank test over-rejects (§2.2) |

## 4b. The seeding baseline: route (C) as it stands

*Step 2 of [VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md) §8. Taken **before**
any change to the seeding, because a change with no baseline behind it cannot be
evaluated and the work gets repeated. Re-run in full with
`tools/measure_seeding_bank.sh`, which is the same instrument route (B) will be
measured with — the same bank, the same five quantities, one extra flag.*

Route (C) is what the program does today: `init_guess` runs a conditional
regression for every block from scratch, ignoring the certified gate. Measured
2026-08-19, `-multistart 20`:

| case | spec | logL₀ | logL | term | best/20 | logL spread | MAmin |
|---|---|---|---|---|---|---|---|
| Milan | `2 1 1 -case 2 -mean` | 76.4950 | 93.2880 | step | 6 | 92.854 .. 93.290 | 1.0003 |
| Strasbourg | `2 1 1 -case 2 -mean` | 23.0287 | 34.5355 | lower | 3 | 33.975 .. 36.088 | 1.0000 |
| Utrecht | `2 1 1 -case 2 -mean` | 68.5727 | 82.2956 | lower | 9 | 81.844 .. 82.383 | 1.0000 |
| Vienna | `2 1 1 -case 2 -mean` | 19.9811 | 33.9528 | lower | 8 | 29.430 .. 34.118 | 1.0000 |
| Aix | `2 1 1 -case 2 -mean` | 55.5842 | 68.9414 | lower | 18 | 68.250 .. 69.179 | 1.0000 |
| Arévalo | `2 1 1 -case 2 -mean` | −16.3068 | −3.5626 | lower | 5 | −6.305 .. −3.544 | 1.0000 |
| Angers | `2 1 1 -case 2 -mean` | 15.5477 | 29.0937 | lower | 4 (19 ok) | 20.489 .. 29.214 | 1.0000 |
| Penn | `2 1 1 -case 2 -mean` | 28.5950 | 41.3726 | lower | 4 | 23.517 .. 42.206 | 1.0000 |
| `mink_muskrat` case 1 | `2 1 1 -case 1` | −283.3315 | 3.6856 | lower | **1** | −11.574 .. 3.686 | 1.0000 |
| `mink_muskrat` case 2 | `2 1 1 -case 2 -mean` | −8.7426 | 6.4786 | step | 6 | −8.692 .. 6.637 | 1.0000 |
| `mink_muskrat` case 3 | `2 1 1 -case 3 -mean` | −8.6607 | 6.5140 | lower | 6 | −9.975 .. 6.799 | 1.0000 |
| `rank2`, r = 2 (the truth) | `2 0 2 -case 2 -mean` | −477.9053 | −477.6141 | grad | **1** (4 ok) | −477.620 .. −477.614 | n/a |
| `rank2`, r = 1 | `2 0 1 -case 2 -mean` | −517.2713 | −514.1466 | grad | 2 (7 ok) | −831.094 .. −514.147 | n/a |
| `rank0`, r = 1 (no rank to find) | `2 0 1 -case 2 -mean` | −842.9403 | −841.6278 | grad | **1** | −841.628 .. −841.628 | n/a |
| `badnorm`, r = 1 | `2 0 1 -case 2 -mean` | −376.2501 | −359.5140 | grad | 4 (14 ok) | −360.792 .. −359.514 | n/a |

`logL₀` is the likelihood **at the starting point** (`-eval`), which is what
separates a bad seed — it starts lower — from an optimiser that from a better
seed ends worse, which is the surface. `best/20` is the restart the best point
was found at, so `best = 1` means the cold start already reached it. `MAmin` is
the smallest moving-average root modulus at the optimum; `1.0000` is the
invertibility boundary, where the optimum is a constrained one.

**Five readings, and they are the baseline (B) has to beat.**

1. **The start is far.** The cold start gives away **11.5 to 16.8** units of
   log-likelihood on the eight pairs, and **287** on `mink_muskrat` case 1. With
   `q = 0` it is 0.3 to 3.1 — the whole distance is the moving-average block.
2. **One cold start is usually not enough.** The best of 20 beats the single
   cold start in **10 of the 15 cases**, by up to 1.55 (Strasbourg) and 0.83
   (Penn). The cold start reaches the best point first in only **3 of 15**, and
   on Aix it takes 18 restarts.
3. **The spread is the diagnostic, and it is wide.** Penn 18.7 units between the
   worst and the best converged start, `mink_muskrat` about 15 in all three
   cases, `rank2` at the wrong rank 317. On a well-behaved surface every start
   lands in the same place; these do not.
4. **The termination is honest and it is poor.** Only the four `q = 0` cases
   stop on the gradient. Every `q = 1` case stops on `steptol` (2) or on a line
   search that could not improve (9) — the termcode 3 already documented in
   [CONVERGENCE.md](CONVERGENCE.md), here counted rather than described.
5. **Ten of the eleven `q = 1` fits sit on the invertibility boundary.**
   Consistent with §3b: at `p = 2, q = 1` on these data the optimum is
   constrained, and the standard errors do not apply along the binding
   direction.

The bank also supplied, without being constructed for it, the case the plan
asked for — a gate that fails its own contract. Milan failed it, the failure was
reported at rung 0 as intended, and the diagnosis is §1b: the check was wrong,
not the gate.

## 4c. Route (B) against route (C): the measurement, and it falls against (B)

*Steps 3 and 4 of [VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md) §8. Same bank,
same five quantities, same instrument as §4b — `tools/measure_seeding_bank.sh`
with `-seedgate` — so the two columns are the same measurement twice.*

Route (B) estimates the `r = 0` rung, holds `F`, `Θ` and `Σ` there, fits `Λ` and
`B₂` on that base, and then releases everything. Route (C) is the cold
conditional regression.

| case | (C) logL | (B) logL | Δ | (C) best of 20 | (B) best of 20 | Δ |
|---|---|---|---|---|---|---|
| Milan | 93.288 | 81.856 | **−11.43** | 93.290 | 84.429 | **−8.86** |
| Strasbourg | 34.535 | 35.459 | +0.92 | 36.088 | 35.481 | −0.61 |
| Utrecht | 82.296 | 76.837 | −5.46 | 82.383 | 82.689 | +0.31 |
| Vienna | 33.953 | 30.601 | −3.35 | 34.118 | 30.941 | −3.18 |
| Aix | 68.941 | 72.231 | **+3.29** | 69.179 | 72.242 | **+3.06** |
| Arévalo | −3.563 | −7.323 | −3.76 | −3.544 | −7.323 | −3.78 |
| Angers | 29.094 | −8.417 | **−37.51** | 29.214 | −8.417 | **−37.63** |
| Penn | 41.373 | 30.363 | −11.01 | 42.206 | 34.891 | −7.31 |
| `mink_muskrat` c1 | 3.686 | 2.895 | −0.79 | 3.686 | 2.942 | −0.74 |
| `mink_muskrat` c2 | 6.479 | 5.269 | −1.21 | 6.637 | 6.500 | −0.14 |
| `mink_muskrat` c3 | 6.514 | 6.883 | +0.37 | 6.799 | 6.883 | +0.08 |
| the four `q = 0` cases | — | — | **0.000** | — | — | ~0 |

**(B) wins three of eleven and loses seven.** The `q = 0` cases are unaffected to
ten decimals, which is the check that the option is not doing something else
entirely: with no moving-average block both routes reach the same optimum.

Two things (B) measured on the way, and they outlast it.

**The entry is one-sided, and it is expensive.** With `F`, `Θ`, `Σ` at the
`r = 0` optimum and `Λ` at the conditional-regression value, the transformed
system comes out **non-stationary** — `elf` answers `ifault = 3` and the
optimiser does not start at all. That is the other face of §3 of the plan: at
`Λ = 0` the root is exactly 1, and of the two directions leaving that point only
one is admissible, which the conditional regression's sign has no reason to be.
The option therefore scans multiples of that `Λ`, ±1 down to ±0.01, and enters
at the best admissible one. What the entry costs is the finding:

| case | `r = 0` optimum | admissible entry | after profiling `Λ`, `B₂` | released fit |
|---|---|---|---|---|
| Milan | 77.781 | −24.093 | −1.338 | 81.856 |
| `mink_muskrat` c2 | −6.331 | −81.640 | −60.369 | 5.269 |
| Angers | 18.080 | −68.716 | −8.422 | **−8.417** |

Stepping off the boundary costs 60 to 100 units of log-likelihood, and profiling
`Λ` and `B₂` on the held base recovers only part of it. On Angers the released
fit then moves by 0.005 and stops: the entry point is a trap, not a start.

**The `r = 0` dynamics do not transfer.** That is the substantive result. `F`,
`Θ` and `Σ` are the same objects at both ranks, but their `r = 0` optima are not
near their `r = 1` optima, and holding them while the error-correction term is
introduced puts the fit somewhere the released optimiser cannot leave — these
surfaces stop on `steptol` or on a failed line search, so a bad start is not
recoverable. Carrying an optimum up a rung, which works everywhere else in this
construction, is exactly what does not work across this one.

## 4d. Does a bad `B₂` seed drive the moving average to the boundary? Measured: no

*A conjecture worth testing, since route (B)'s `B₂` pre-estimates are far from
the optimum — on Arévalo even positive — and almost every fit on this data ends
with a moving-average root on the invertibility boundary. The instrument is
`-seedb2 v`, which starts `B₂` at `v` and estimates it **free** (`-fixb2` pins
it). 38 fits, four datasets.*

**Where the seed moves the answer, it does not move the boundary.** Ten `B₂`
seeds from `+0.5` to `−2.8`, everything else cold:

| seed | Milan logL / `B̂₂` / MAmin | Angers | Arévalo |
|---|---|---|---|
| +0.5 | 93.274 / −0.445 / 0.99995 | 28.304 / −0.619 / 0.99995 | −3.721 / −0.644 / 0.99995 |
| −0.25 | 93.290 / −0.447 / 0.99995 | 28.319 / −0.691 / 0.99995 | −3.597 / −0.623 / 0.99996 |
| −0.6 | 93.290 / −0.447 / 1.00000 | 28.083 / −0.611 / 0.99995 | −3.544 / −0.617 / 1.00000 |
| −1.5 | 93.290 / −0.447 / 1.00000 | 28.959 / −0.661 / 0.99996 | −3.587 / −0.622 / 0.99995 |
| −2.8 | 91.979 / −0.438 / 0.99995 | **18.192 / −2.635** / 0.99995 | −3.983 / −0.644 / 0.99995 |

On the pairs the fit is nearly indifferent to where `B₂` starts — Milan lands
within 0.02 log-likelihood units and at `B̂₂ = −0.445 ± 0.01` from a seed of the
wrong sign as readily as from a good one — and **the smallest moving-average
root is 0.99995 to 1.00000 in every single run, good seeds and bad alike.** The
boundary is not something a bad seed causes; it is where the maximum is.

**And the relation runs the other way.** Pinning `B₂` and profiling shows when
the moving average *does* come off the boundary:

| `B₂` pinned | Milan logL / MAmin | Arévalo logL / MAmin |
|---|---|---|
| −0.25 | 90.456 / 0.99995 | −5.542 / 0.99996 |
| **−0.45** | **93.230 / 0.99995** | −4.954 / 0.99996 |
| **−0.60** | 90.165 / 1.17577 | **−3.742 / 0.99996** |
| −1.00 | 86.012 / 1.32024 | −7.421 / **1.18036** |
| −1.50 | 82.695 / 1.30733 | −10.915 / 1.18410 |

The moving average leaves the boundary exactly where `B₂` is pushed **past** its
optimum, at a cost of 3 to 7 log-likelihood units. Comfortable invertibility is
the signature of a `B₂` that is wrong, not of one that is right. The same shows
up in the seed runs on `mink_muskrat`, where the only two fits with a
comfortably invertible root (MAmin 12.7 and 3.5) are also the two worst fits by
far (logL −22.7 and −26.4).

**What a bad `B₂` seed does instead is kill the run outright.** On the synthetic
`rank2` at `r = 1`, seeding `B₂ = +0.5` makes the *starting point* non-stationary
— `elf` answers `ifault = 3`, `est` refuses to begin, and there is no fit at all.
That is the same wall route (B) hit from the other side (§4c): the damage a bad
seed does is on the **stationarity** side, and it is fatal rather than gradual.

Two corollaries. Route (B)'s failure cannot be blamed on its `B₂`
pre-estimates: seeded with `B₂ = −0.25`, close to (B)'s Angers value of −0.265,
the cold fit still reaches 28.3 where (B) reaches −8.4. What is toxic in (B)'s
starting point is the **held `F`, `Θ`, `Σ`** and the boundary entry, not `B₂`.
And `mink_muskrat` remains the case where the surface, not the seed, is the
problem: there the ten seeds spread the answer from −26.4 to +5.5, which is the
sensitivity already recorded in [CONVERGENCE.md](CONVERGENCE.md).

## 4e. The canonical seed, measured without the moving average and with it

*Route (D) of [VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md) §8b: seed `B₂` with
the canonical reduced-rank solution — Johansen's eigenvalue problem, closed form
— instead of the static OLS regression. `-seedjoh`. Everything else, `Λ`, `F`,
`Σ`, `E[W]`, then follows from the conditional regression **with that `W`**,
which is Johansen's own `α` formula, so `α` is not computed twice.*

The implementation is `drvec`'s own (GSL's symmetric-definite generalized
eigenproblem), not a call to anything external. Checked against `statsmodels`'
`coint_johansen` on the eight pairs, `p = 2` ↔ `k = 1`: **agreement 0.001 to
0.021** in `B₂`, the residue being the deterministic treatment (an unrestricted
constant in the auxiliary regressions here, restricted to the relation there).

### Without the moving average (`q = 0`): the seed is essentially the answer

| pair | cold start | canonical start | the optimum both reach |
|---|---|---|---|
| Milan | 76.495 | **76.628** | 76.650 |
| Strasbourg | 23.029 | **24.123** | 24.156 |
| Utrecht | 68.573 | **70.605** | 70.641 |
| Vienna | 19.981 | **20.920** | 20.979 |
| Aix | 55.584 | **60.817** | 60.841 |
| Arévalo | −16.307 | **−13.596** | −13.590 |
| Angers | 15.548 | **16.161** | 16.171 |
| Penn | 28.595 | **31.532** | 31.542 |
| `mink_muskrat` | **−8.743** | −9.839 | −8.681 |

Mean distance from the optimum: cold **1.93**, canonical **0.025** — seventy-five
times closer, and on Arévalo the canonical seed starts 0.006 away. Both routes
converge on the gradient to the same optimum to seven decimals, which is the
check that the seed is changing the *start* and not the model. `mink_muskrat` is
the exception in both halves: there the canonical seed starts 1.16 *below* the
cold one.

### With the moving average (`q = 1`): it splits

| pair | cold | canonical | Δ |
|---|---|---|---|
| Milan | 93.288 | 93.252 | −0.04 |
| **Strasbourg** | 34.535 | **36.088** | **+1.55** |
| Utrecht | 82.296 | 82.379 | +0.08 |
| Vienna | 33.953 | 33.478 | −0.47 |
| Aix | 68.941 | 68.958 | +0.02 |
| Arévalo | −3.563 | −3.560 | +0.002 |
| Angers | 29.094 | 28.589 | −0.51 |
| Penn | 41.373 | 41.353 | −0.02 |
| `mink_muskrat` c1/c2/c3 | 3.686 / 6.479 / 6.514 | −25.564 / −1.501 / −0.838 | **−29 / −8 / −7** |

Four wins and four losses on the pairs, and a rout on `mink_muskrat` — where the
canonical `B₂` is **positive** (+0.38) against `drvec`'s optimum at −0.24, so the
two estimators genuinely disagree on that data rather than the seed being bad.

**Strasbourg is the case that matters.** The canonical seed reaches 36.088 from a
single start — exactly the value the cold route only finds with twenty restarts —
converging **on the gradient**, the only `q = 1` fit in the bank that does, and
with a smallest moving-average root of **1.195**, comfortably invertible. The
boundary fit at 34.535 that the cold start converges to is a **local** optimum.

### The diagonal moving average

| pair | logL | MAmin | termination |
|---|---|---|---|
| Milan | 91.314 | 1.00097 | gradient |
| Strasbourg | 34.281 | 1.00014 | gradient |
| Utrecht | 81.150 | 0.99995 | lower |
| Vienna | 31.623 | 0.99996 | steptol |
| Aix | 69.169 | 0.99995 | lower |
| **Arévalo** | −11.467 | **1.45543** | gradient |
| Angers | 23.124 | 1.00020 | gradient |
| **Penn** | 35.906 | **1.27678** | gradient |

Restricting `Θ` to be diagonal makes the optimiser behave — five of eight
converge on the gradient against nearly none with a full `Θ` — and three sit
comfortably off the invertibility boundary. It also fits much worse: Arévalo
−11.47 against −3.56, an LR of 15.8 on 2 degrees of freedom.

## 4f. Does the moving average "inherit" the univariate structure? Partly, and only below `r = 1`

*A conjecture with real content: `Θ` is the same object as the univariate
moving averages of the components, so with a diagonal `Θ` it should reproduce
them rather than run to the unit circle.*

The eight pairs each carry the two `fue` univariate models they were built from,
so the univariate structure is on file rather than inferred. For every pair the
London component's univariate model is `MA(1)` with **θ = 0.7482**; the market
component's has **no moving average at all** in five of the eight.

| pair | `Θ` diag at `r = 0` | `Θ` diag at `r = 1` | univariate (`fue`) |
|---|---|---|---|
| Milan | 0.8332, −0.5515 | −0.4711, **0.9990** | 0.7482, none |
| Strasbourg | 0.8343, 0.8722 | −0.4356, **0.9999** | 0.7482, none |
| Utrecht | 0.8343, −0.2682 | 0.3833, **1.0000** | 0.7482, none |
| Vienna | 0.8343, 0.9132 | 0.6293, **1.0000** | 0.7482, 0.8714 |
| Aix | 0.9083, 0.7936 | 0.7267, **1.0000** | 0.7482, 0.7682 |
| Arévalo | 0.8343, −0.6294 | 0.2019, −0.6871 | 0.7482, none |
| Angers | 0.9083, 0.9618 | −0.1962, **0.9998** | 0.7482, 0.9364 |
| Penn | 0.8152, −0.9120 | −0.7494, 0.7832 | 0.7482, none |

**Below the rung it inherits.** At `r = 0` the differenced London component is
estimated at 0.815 to 0.908 against `fue`'s 0.7482 — the same structure, which
is what the gate's factorisation contract already proves in another form.

**At `r = 1` it stops.** The equation order flips — with `r = 1` the first
equation is the market and the second the differenced London — and that second
coefficient goes to **0.999–1.000 in six of the eight pairs**. A diagonal `Θ`
entry of one is the factor `(1 − B)` sitting on `∇`London, which is the model
saying that series should not have been differenced.

**And it is not the seed.** Seeded with `fue`'s own univariate values through
`-seedybar` — 0.7482 for London, zero for the market — the fit walks the same
coefficient back to 1.0000 in **six of the eight** pairs (Penn stops at 0.783
and Angers at 0.898). With twenty restarts and a free `Θ`, **seven of the eight**
best fits are still on the boundary. So the boundary is where the likelihood
sends it, from a good seed as readily as from a bad one — with Strasbourg, above,
the measured exception where the boundary fit is only a local optimum.

## 4g. The moving average is not free: it inherits. And that is where the specification was wrong

*Corollary 2 of the BVECM paper (`Article_Multivariate Convergence/
cointegration_convergence/Legacy`, WARMA–VEC equivalence with MA). `-mawarma`.*

The question that opened this: in a VEC model the error-correction term is a
**stationary regressor**, and adding a stationary regressor cannot move a moving
average from 0.83 to 1.00. Either the data are extraordinary or the embedding is
wrong. Four measurements, in the order they were taken.

### The embedding is right — verified, not assumed

Mauricio's transformation as coded (`vec_shootx` [4]–[6]) was checked against
Eqs. (10)–(18) term by term and then **numerically**: simulating a VEC process
with a general, non-scalar `Θ` and testing the identity

`Ȳ_t − ΣΦ*_kȲ_{t−k} = A*_t − ΣΘ*_kA*_{t−k}`,

with `Φ*_k = C̄Φ̄_k`, `Θ*_k = C̄Θ_kC̄⁻¹`, `A*_t = C̄A_t`, gives a maximum error of
**4.2e−15**. The three plausible alternatives fail by 0.56 to 2.5, so the test
discriminates. The cast is not the fault.

### The estimator is right too, when the surface is

Simulated from a VEC with `Θ = 0`: fitted with `q = 1` it returns
`Θ̂ ≈ [[−0.05, −0.09],[0.08, 0.05]]`, moving-average roots at 14.2, clean
convergence. Simulated from the **WARMA** process of Corollary 2 with `θ = 0.5`,
`β = 0.5`, `n = 8000`, and fitted with a free `Θ`:

| | truth | free fit |
|---|---|---|
| `B₂` | −0.5 | **−0.49993** |
| `Λ` | (0.30, −0.20) | (0.384, −0.200) |
| `Θ` | `[[0.5, −0.25],[0, 0]]` | `[[0.397, −0.216],[−0.009, 0.0005]]` |

The free estimator **finds the inherited structure on its own** — the bottom row
comes out at 1e−3 — which is the strongest evidence that neither the cast nor
the optimiser is at fault.

### What the theory says the structure is

Corollary 2: if `{z_t}` admits a WARMA representation — `Φ(B)w_t = Θ(B)a_t` for
the cointegrating block, `Δz₂ₜ = γw_{t−1} + … + η_t` for the differenced one —
then the VEC error is `ε_t = [β'η_t + Θ(B)a_t ; η_t]`. Regrouping on
`A_t = [a_t + β'η_t ; η_t]`, which is an invertible transformation of the noise,
gives `ε_t = A_t − Θ̃₁A_{t−1}` with

`Θ̃₁ = [[Θ₁, −Θ₁β'],[0, 0]] = [[Θ₁, Θ₁B₂'],[0, 0]]`  (`B₂ = −β`).

Verified numerically on the simulated WARMA: **4.4e−16** for that form, **2.0**
for the sign-flipped one. With `M = 2, r = 1` it leaves **one** free
moving-average parameter where `drvec` was estimating **four**.

### On the real data, the free version is the one misbehaving

| pair | free `Θ̂` | free MAmin | inherited `Θ̂₁₁` | MAmin | free `B̂₂` | inherited `B̂₂` | Johansen `β₂` |
|---|---|---|---|---|---|---|---|
| Milan | bottom row −0.75, 1.11 | 1.0003 | −0.415 | **2.410** | −0.448 | **−0.566** | −0.554 |
| Strasbourg | −0.75, 1.15 | 1.0000 | | **4.931** | −0.476 | **−0.640** | −0.652 |
| Utrecht | −0.42, 1.04 | 1.0000 | | **7.534** | −0.627 | **−0.789** | −0.810 |
| Vienna | −1.32, 1.54 | 1.0000 | | **1.630** | −0.590 | **−0.866** | −0.889 |
| Aix | 1.11, 0.80 | 1.0000 | | **1.381** | −0.762 | **−1.029** | −1.081 |
| Arévalo | 1.51, 1.14 | 1.0000 | | **12.549** | −0.627 | **−0.995** | −0.994 |
| Angers | **5.09**, −0.07 | 1.0000 | | **6.005** | −0.618 | **−1.030** | −1.036 |
| Penn | −1.43, 2.05 | 1.0000 | | **1.717** | −1.088 | **−1.088** | −1.079 |

Three things at once, and they point the same way.

1. **The boundary pathology disappears.** All eight pairs go from a smallest
   moving-average root of 1.0000 to between 1.38 and 12.55.
2. **All eight converge on the gradient**, where the free version stops on
   `steptol` or a failed line search in every one.
3. **`B̂₂` lands on the canonical estimate.** Under the restriction it agrees
   with Johansen's `β` to **0.001–0.052** — the same agreement §2.1b measures at
   `q = 0`, where the two fit the same model. Under the free `Θ` it was 0.1 to
   0.4 away.

Meanwhile the free `Θ̂` on these data carries entries of 1.5 and **5.09** with a
`(2,2)` entry of 1.0–2.0 — which is what puts the root on the unit circle —
where on simulated WARMA data it returns a bottom row of 1e−3.

### The likelihood prefers the free version, and that comparison is not a test

The LR is 13 to 26 on 3 degrees of freedom, so read naively the restriction is
rejected everywhere. **It cannot be read naively**: the unrestricted optimum
sits ON the invertibility boundary in all eight cases, and an LR statistic whose
unrestricted estimate is on a constraint does not have its `χ²` distribution —
the same caveat §3b already attaches to the standard errors there. What can be
said without a distribution is what the table says: the restricted fits are
interior, converge on the gradient, and agree with an independent estimator,
while the unrestricted ones do none of the three.

`-mawarma` is therefore offered and not imposed, and the register records the
comparison rather than a verdict.

## 5. What is not in the register, and why

* **The Census Housing example** of the AddOn (Hillmer & Tiao 1979): the data
  have not been located. Without them that comparison cannot be attempted.
* **Finite-sample critical values.** Everything above uses asymptotic values. The
  paper's own recommendation for this class of model is a parametric bootstrap,
  which is not implemented.
* **Standard errors against a published table.** `drvec` reports them from the
  Hessian; they have not been compared with an external source.

---

*Re-measuring this register is `make test` plus the four commands quoted above.
If a row moves, either the program changed or the input did — and the second is
the more frequent cause, so the input should be checked first.*

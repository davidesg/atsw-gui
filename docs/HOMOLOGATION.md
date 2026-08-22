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

> **Which parameterisation each figure belongs to.** On 2026-08-20 the default
> moving-average class changed: with `q ≥ 1` and `r ≥ 1` `drvec` now estimates
> `Θ = [T₁₁ T₁₂ ; 0 0]`, and the free `Θ` that was the default is `-mafree`
> (`SPECIFICATION_PLAN.md` §10; the reason is §4r below). **Every figure in this
> register measured with `q ≥ 1` before that date belongs to the free class**,
> and is reproduced by adding `-mafree` to the command quoted beside it. Figures
> at `q = 0` are unaffected — there is no `Θ` to structure — and so are §4g
> through §4p, which are explicitly about comparing classes and name theirs.
> Sections written after that date state the class in the command. Every `.out`
> names it on a `MA :` line in the header.

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
| the operator roots against `zroots` (Laguerre, `Root-1.01`, byte-identical in `ART_18.1`) | `1.000262` and `2.486237` against `1.00026` and `2.48624` | ✔ |
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
| **`drvec`, the same four with `-multistart 60`** | **0.002344 – 0.002372** | **spread 0.000028, and 1.9 % above** |

> **Re-measured on 2026-08-20, and one of the four figures was wrong.** The
> program now prints `|Σ̂|` itself (P2 of `PLAN_PRODUCCION.md`); until then it had
> to be computed by hand from a matrix rounded to six decimals, which is where
> this row lost a digit. The four, with the commands that produce them — note
> `-mafree`, because the default class changed in §4r and these figures belong to
> the free one:
>
> ```sh
> bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2 -mafree -multistart 60
> bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 3 -mafree -multistart 60
> bin/drvec <legacy> 2 1 1 -case 2 -differenced -mafree -multistart 60
> bin/drvec <legacy> 2 1 1 -case 3 -differenced -mafree -multistart 60
> ```
>
> | configuration | recorded | re-measured |
> |---|---|---|
> | levels, case 2 | 0.002346 | **0.002372** ✘ |
> | levels, case 3 | 0.002347 | 0.002347 ✔ |
> | legacy, case 2 | 0.002344 | 0.002344 ✔ |
> | legacy, case 3 | 0.002358 | 0.002358 ✔ |
>
> Three of the four reproduce **to the digit**. The `legacy` layout is derived
> from `mink_muskrat.csv` exactly as the suite derives it, so the earlier guess
> that those two rows needed an input that no longer exists was wrong — they
> reproduce fine. Only `levels, case 2` does not, and it has not reproduced since
> the beta close either: the binary rebuilt at `e6a9431` gives the same
> 0.002372. The recorded 0.002346 is therefore a transcription error, corrected
> above, and the criterion is slightly weaker than it was written to be —
> spread 0.000028 rather than 0.000014, level 1.9 % rather than 1.6 %. The
> qualitative conclusion does not move. The suite now carries these four as a
> `SLOW=1` regression so the figure cannot drift again unnoticed.

The single-start row is what the program reports by default, and is the
appropriate figure for a single unattended run. The multi-start row is what it gives when asked to look
properly, and it is the one to quote: the spread across four mathematically
equivalent set-ups collapses to 0.000028 and the level lands within 1.9 % of the
global-search reference.

Not closed, and the residue is stated: 0.002355 (the mean of the four) against 0.002311 with the
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

## 4. Cases run without an external reference, and the specification work

Section 4 grew into the record of one investigation, so here is its map. The
question it answers, in one line: **the fitted model has to be a model of the
rank it was estimated at, and by default it was not.**

| | |
|---|---|
| **4** | cases run without an external reference |
| **4b** | the seeding baseline: route (C) as it stands |
| **4c** | route (B) against (C) — and it falls against (B) |
| **4d** | does a bad `B₂` seed drive the moving average to the boundary? No |
| **4e** | the canonical seed, without the moving average and with it |
| **4f** | does the moving average inherit the univariate structure? Below `r = 1`, yes |
| **4g** | **the moving average is not free: it inherits** — the specification error |
| **4h** | **the rank condition**, and what Mauricio (2006) leaves unguarded |
| **4i** | the inherited moving average, tested with the distribution it has |
| **4j** | which half of the structure the data reject, and the specification that follows |
| **4k** | what the BVECM theorems actually cover, and what the equivalence does not prove |
| **4l** | the theorems' class estimated in its own coordinates, and back again |
| **4m** | the specification ladder, read on four cases |
| **4n** | **the rank test**, and what the moving average was doing to it |
| **4o** | the default, decided and not moved |
| **4p** | the autoregressive half of the class, tested — and it survives |

The theory these rest on is [THEORY.md](THEORY.md); the work they set up and
closed is [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md).

### Cases run without an external reference

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

## 4h. The rank condition, and what the paper leaves unguarded

*The question this answers: `-mawarma` fixes the pathology, but it does so by
narrowing the model class. Is there something narrower still that is wrong — a
condition the class itself needs and nobody is enforcing? There is.*

### The condition

For a VEC model with moving-average errors to represent an I(1) process with
cointegrating rank **exactly** `r`, the Granger representation requires

`G = σ_min(Λ⊥′ Θ(1) B⊥)`  to be bounded away from zero,

because `C(1) = B⊥(Λ⊥′ΓB⊥)⁻¹Λ⊥′Θ(1)` is what carries the `M−r` stochastic
trends. Where `G` degenerates, `C(1)` loses rank: the **fitted** model says `r`
and its parameters leave fewer trends than `r` implies.

Mauricio (2006) assumes partial nonstationarity **of the true process** (§2) and
refers to Yap and Reinsel (1995) for identifiability, but the estimation imposes
nothing of the kind. The engine checks that the roots of `Φ*` and `Θ*` are not
**inside** the unit circle; this degeneracy lives exactly **on** it, in the
permitted edge. `drvec` now reports `G` at every fit, next to the roots, and
`-rankadm [tol]` refuses points below `tol` the way a non-positive-definite `Σ`
is refused.

### Where the free fits actually sit

| pair | free `G` | `\|Θ̂(1)\|` free | `-mawarma` `G` |
|---|---|---|---|
| Milan | 0.0855 | 0.00037 | 0.995 |
| Strasbourg | 0.0454 | −0.00005 | 0.996 |
| Utrecht | 0.1333 | −0.00004 | 0.979 |
| Vienna | 0.0158 | −0.00005 | 0.945 |
| Aix | 0.0790 | −0.00003 | 0.993 |
| Arévalo | 0.0566 | −0.00007 | 0.962 |
| Angers | 0.0194 | 0.12343 | 0.901 |
| Penn | 0.0177 | −0.00004 | 0.925 |
| **simulated WARMA, `n = 8000`** | **1.010** | | 0.999 |

Two things to read carefully, and the second is a correction of the first
impression. `Θ̂(1)` is **singular to working precision** in seven of the eight,
and the direction in which it is singular is aligned with `Λ⊥` at 0.946 to
0.9996 — so the degeneracy is essentially in the direction that must stay
integrated. But `G` itself is **not** zero: it is 0.016 to 0.133. The free
optimum does not sit *on* the degenerate set; it sits **one to two orders of
magnitude into its neighbourhood**, against a value of ≈ 1 both under the
inherited structure and at a known truth in simulation.

### What enforcing it does, on Milan

Only `G ≥ tol` is imposed; `Θ` stays free.

| `tol` | logL | `G` | MAmin | `B̂₂` |
|---|---|---|---|---|
| — (free) | 93.288 | 0.086 | 1.000 | **−0.448** |
| 0.05 | 93.290 | 0.087 | 1.000 | −0.447 |
| 0.10 | 92.674 | 0.100 | 1.007 | −0.464 |
| 0.20 | 89.505 | 0.200 | 1.288 | **−0.597** |
| 0.30 | 87.250 | 0.300 | 1.736 | −0.585 |
| 0.50 | 86.119 | 0.500 | 2.791 | −0.580 |
| 0.70 | 84.634 | 0.700 | 3.349 | −0.578 |
| 0.90 | 82.492 | 0.900 | 3.277 | −0.552 |
| `-mawarma` | 81.313 | 0.995 | 2.410 | −0.566 |
| Johansen's canonical `β` | | | | **−0.554** |

**`B̂₂` has two regimes.** Inside the near-degenerate region it is −0.447. The
moment the rank condition is enforced at any non-trivial level it settles at
**−0.55 to −0.60** and stays there across a twenty-fold range of `tol`, at the
value Johansen's canonical estimator and the inherited structure both give. The
same happens on the whole bank at `tol = 0.3`: `B̂₂` moves from the free values
to within 0.02–0.31 of the inherited ones, and the smallest moving-average root
moves from 1.000 to 1.30–2.42 everywhere.

So the quantity this program exists to produce depends on whether the fit is
allowed into the near-degenerate neighbourhood — and **outside it every route
agrees**: the canonical estimator, the inherited moving average, and the free
one under a rank floor.

### What is missing from the paper, stated plainly

1. **The admissibility condition is never written down.** The assumptions are
   about the process; the parameter space optimised over includes points that
   violate them, and the pathology is at the boundary the engine permits.
2. **The identification conditions are assumed by reference and never
   translated** into the parameters the paper proposes estimating
   (`Λ, B₂, F_i, Θ_i, Σ`), so an implementer of Remark 1 has no way to know
   which points are admissible.
3. **Remark 5's sequential rank test has a boundary null** — at `Λ = 0` the
   transformed system has an AR root of exactly one and `B₂` is unidentified,
   since `Π = ΛB′ = 0` for any `B₂` — which the paper does not say.
4. **No finite-sample evidence.** One bivariate illustration; §2.3 here measures
   the asymptotic rank test over-rejecting six-fold at `n = 120`.

None of this makes the transformation wrong — §1 and §4g verify it to 4e−15.
It makes the *estimation problem* less well posed than the paper's statement of
it suggests.

### What is not settled

Whether these data want a moving average richer than the WARMA class allows. The
LR prefers the free version by 13 to 26 on 3 degrees of freedom, and that
comparison is exactly the one that cannot be made, because the unrestricted
optimum is in the near-degenerate region where neither the standard errors nor
the LR have their usual distributions. Settling it needs boundary-aware
inference — a parametric bootstrap under the restricted model, which
`-bootstrap` could already supply.

## 4i. The inherited moving average, tested with the distribution it actually has

*`-matest N`. The comparison §4g left open, done with a parametric bootstrap
under the restricted model instead of the `χ²` that does not apply. 100
replications, two fits each.*

| case | LR | `χ²(3)` p | **bootstrap p** | boot 10% | boot 5% | boot 1% |
|---|---|---|---|---|---|---|
| Milan | 23.95 | 0.0000 | **0.0099** | 10.31 | 11.86 | 15.54 |
| Strasbourg | 20.17 | 0.0002 | **0.0198** | 13.13 | 15.17 | 16.64 |
| Utrecht | 22.82 | 0.0000 | **0.0099** | 11.85 | 13.46 | 17.21 |
| Vienna | 24.76 | 0.0000 | **0.0099** | 12.86 | 14.21 | 18.10 |
| Aix | 13.63 | 0.0035 | **0.0808** | 12.85 | 17.73 | 25.45 |
| Arévalo | 20.01 | 0.0002 | **0.0198** | 12.07 | 14.33 | 19.25 |
| Angers | 25.61 | 0.0000 | **0.0200** | 15.70 | 18.11 | 37.40 |
| Penn | 17.80 | 0.0005 | **0.0396** | 13.11 | 15.44 | 19.69 |
| `mink_muskrat` c1 | 27.37 | 0.0000 | **0.0118** | 8.73 | 13.94 | 24.94 |
| `mink_muskrat` c2 | 12.93 | 0.0048 | **0.1000** | 12.37 | 16.16 | 23.20 |
| `mink_muskrat` c3 | 12.97 | 0.0047 | **0.1011** | 12.93 | 16.13 | 24.77 |

`χ²(3)` has 6.25 / 7.81 / 11.34 at 10 / 5 / 1 %.

**The asymptotic test over-rejects, by a factor of 1.5 to 2.3 in the critical
value.** The bootstrap 5 % value runs from 11.9 to 18.1 against `χ²`'s 7.81 —
the same finding §2.3 records for the rank test, on a different statistic.

**And with the right distribution the restriction is still rejected in most
cases**: at 5 %, in eight of the eleven. It is not rejected on Aix (0.081) or on
`mink_muskrat` in cases 2 and 3 (0.100, 0.101). Four of the eight pairs sit at
the floor `1/(B+1) = 0.0099`, so their evidence is "stronger than 100
replications can resolve" and no more; resolving 1 % needs `B ≥ 999`.

### What that settles, and what it does not

It settles the question §4g left open: **these data do want a moving average
richer than the WARMA class**, in most of the bank, and the earlier `χ²` reading
of that was overstated by a factor of two in the critical value rather than
simply invalid.

It does **not** endorse the free fit. Rejecting `H₀` says the restricted model
is too narrow; it says nothing in favour of an alternative whose own optimum
sits where the rank condition degenerates (§4h). Both can be true at once, and
here both are: the inherited structure is too strong for these data, and the
free `Θ` is inadmissible as a rank-`r` I(1) model. What the two measurements
together point at is the middle — a free `Θ` under a rank floor, which is what
`-rankadm` estimates and where `B̂₂` agrees with the canonical estimator across a
twenty-fold range of the floor.

## 4j. Which half of the structure the data reject, and the specification that follows

*Corollary 2 imposes two things at once — the lower-left block of `Θ` zero
**and** the differenced block carrying no moving average of its own, with the
cross block determined by `B₂`. §4i rejected the pair of them together in eight
of eleven cases without saying which half did the rejecting. These are the
rungs that separate them.*

| flag | `Θ` | params (`M=2, r=1, q=1`) |
|---|---|---|
| `-mawarma` | `[T₁₁  T₁₁B₂′ ; 0  0]` | 1 |
| `-marow` | `[T₁₁  T₁₂ ; 0  0]` | 2 |
| `-matri` | `[T₁₁  T₁₂ ; 0  T₂₂]` | 3 |
| — (default) | free | 4 |

### The zero that matters is not the one that looks structural

`-matri` zeroes the lower-left block and leaves `T₂₂` free. **It does not remove
the pathology**: `G` stays at 0.02–0.12 and the smallest moving-average root
stays at 1.000 in eight of eleven cases, exactly as in the free fit.

`-marow` zeroes `T₂₂` as well and frees the cross block. That does remove it:

| pair | free logL / `G` / MAmin / `B̂₂` | `-marow` | `-mawarma` |
|---|---|---|---|
| Milan | 93.288 / 0.086 / 1.000 / −0.448 | 82.656 / **0.522** / **10.86** / −0.609 | 81.313 / 0.995 / 2.41 / −0.566 |
| Strasbourg | 34.535 / 0.045 / 1.000 / −0.476 | 25.757 / **0.382** / **1.82** / −0.738 | 24.451 / 0.996 / 4.93 / −0.640 |
| Utrecht | 82.296 / 0.133 / 1.000 / −0.627 | 70.918 / **0.942** / **4.97** / −0.791 | 70.888 / 0.979 / 7.53 / −0.789 |
| Vienna | 33.953 / 0.016 / 1.000 / −0.590 | 23.011 / **0.271** / **1.75** / −0.942 | 21.572 / 0.945 / 1.63 / −0.866 |
| Aix | 68.941 / 0.079 / 1.000 / −0.762 | 63.769 / **0.215** / **1.40** / −1.437 | 62.128 / 0.993 / 1.38 / −1.029 |
| Arévalo | −3.563 / 0.057 / 1.000 / −0.627 | −13.455 / **0.793** / **7.81** / −1.004 | −13.570 / 0.962 / 12.55 / −0.995 |
| Angers | 29.094 / 0.019 / 1.000 / −0.618 | 20.289 / **1.323** / 1.000 / −1.045 | 16.291 / 0.901 / 6.01 / −1.030 |
| Penn | 41.373 / 0.018 / 1.000 / −1.088 | 33.618 / **1.200** / **1.39** / −1.098 | 32.475 / 0.925 / 1.72 / −1.088 |
| `mink` c2 | 6.479 / 0.184 / 1.000 / −0.241 | 2.304 / **0.388** / **1.28** / −0.151 | 0.013 / 0.264 / 1.00 / −0.095 |
| `mink` c3 | 6.514 / 0.175 / 1.000 / −0.279 | 2.307 / **0.388** / **1.28** / −0.151 | 0.027 / 0.259 / 1.00 / −0.096 |

`-marow` converges **on the gradient in nine of the ten** (Angers is the
exception), against none of the free fits.

So the parameter that carries the inadmissibility is `T₂₂`: the moving average
of the **already differenced** block. Left free, the optimiser takes it to one,
which is `(1 − B)` sitting on `∇Y₂` — the model undoing its own differencing.
`Θ(1)` then loses the identity in its lower block and the rank condition goes
with it. That is the mechanism behind the conjecture that opened §4g, located.

### And that is where nearly all the "evidence" against the structure came from

| pair | LR(`marow` vs `mawarma`), 1 df | LR(free vs `marow`), 2 df |
|---|---|---|
| Milan | 2.69 | 21.26 |
| Strasbourg | 2.61 | 17.56 |
| Utrecht | 0.06 | 22.76 |
| Vienna | 2.88 | 21.88 |
| Aix | 3.28 | 10.34 |
| Arévalo | 0.23 | 19.78 |
| Angers | 8.00 | 17.61 |
| Penn | 2.29 | 15.51 |
| `mink` c2 / c3 | 4.58 / 4.56 | 8.35 / 8.41 |

The admissible half — freeing the cross block, which `B₂` determines under the
corollary — is worth **0.06 to 8.0** on 1 degree of freedom, and is **not
rejected at 5 % in seven of the ten**. The other half — freeing `T₂₁` and `T₂₂`
— is worth 8 to 23, and it is bought entirely in the direction that makes the
model inadmissible.

Two of the free fits (Aix 68.94, Penn 41.37) come out **below** the nested
`-matri` fits (71.82, 41.55). A restricted model cannot beat the model that
contains it, so those two free fits did not reach their own optimum — more
evidence about that surface, from the arithmetic rather than from an opinion.

### The compromise this supports

`-marow` is what the measurements point at, and it is a specification rather
than a tolerance: `Θ = [T₁₁  T₁₂ ; 0  0]`.

* **Admissible by construction.** `Θ(1)` keeps the identity in its lower block,
  so the rank condition cannot degenerate and no floor has to be chosen.
* **It frees exactly what the data rejected** — the cross block, whose
  determination by `B₂` is the half that is mostly not rejected but is also the
  half that costs nothing to release.
* **It behaves**: gradient convergence in nine of ten, moving-average roots at
  1.4–10.9, `B̂₂` in the same region as the canonical estimator.
* **What it gives up** relative to the free fit is bought in the inadmissible
  direction, so it is not a loss that can be defended by a likelihood ratio.

## 4k. What the BVECM theorems actually cover, and what the equivalence does not prove

*Read after §4g–§4j, because it says where those restrictions belong. The
theorems are proved for Phillips' triangular model in the WARMA
parameterisation; the equivalence with the general VARMA-VEC is **asserted**,
and it does not carry the same restrictions.*

### The class the theorems cover

Definition 3 of the BVECM appendix specifies

`w_t = α′z_t`,  `Φ(B)w_t = a_t`,  `Δz₂ₜ = γw_{t−1} + Σψ_i w_{t−i} + η_t`,

with Corollary 2 adding `Θ(B)` to the middle equation only. Read what that says
about the second block: `Δz₂` is driven **only by lagged `w` and its own white
noise** — no lags of `Δz₂` anywhere, and no moving average. And the first
equation makes `w` depend only on its own lags.

Its VEC image, from the proofs themselves, therefore carries two restrictions
the general VEC does not have:

* **`Γ_i = M_l α′`** — the short-run matrices have rank ≤ `r` and their row space
  is `α′`, because every lag enters through `w`. With `M = 2, r = 1` that is 2
  free parameters where Mauricio's `F₁` has 4.
* **the moving-average structure of §4g**, `Θ̃₁ = [[Θ₁, Θ₁B₂′],[0,0]]`.

### The converse is asserted, not proved

Theorem 1 closes with "conversely, any VEC representation with `Π = Aα′` of rank
`r` can be written in WARMA form with appropriate normalization", and Corollary
2 repeats it for the moving-average case, referring to Mauricio (2005) for the
construction. That reference cannot supply what is missing: **Mauricio's
transformation is an algebraic rearrangement** — it maps a VEC to a stationary
VARMA on `Ȳ` and back, identically, verified here to 4e−15 (§4g). An identity
cannot turn a free `Γ_i` of rank `M` into one of rank `r`. The forward direction
(WARMA ⟹ VEC) is proved constructively; the backward one holds only for the VEC
models whose `Γ_i` and `Θ` already have the image structure.

So the theorems' conclusions — on the adjustment matrix, on the invariance under
the convergence operator, on what the BEC form means — are established **for the
triangular class**, and applying them to a fit produced by a free VARMA-VEC is a
step the papers do not license.

### The restrictions read in the coordinates where they are simple

`Θ* = C̄ΘC̄⁻¹` is the moving average of the transformed system `Ȳ = [∇Y₂ ; W]`,
which is where the triangular model is stated. The ladder of §4j becomes, with
`b = B₂`:

| in VEC coordinates | `Θ*` in `Ȳ = [∇Y₂ ; W]` |
|---|---|
| free `Θ` | free |
| `-matri`  `[T₁₁ T₁₂ ; 0 T₂₂]` | `[[T₂₂, 0],[·, T₁₁]]` — **the `∇Y₂` equation keeps a moving average** |
| `-marow`  `[T₁₁ T₁₂ ; 0 0]` | `[[0, 0],[T₁₂ − bT₁₁, T₁₁]]` — first row zero |
| `-mawarma`  `[T₁₁ T₁₁B₂′ ; 0 0]` | `[[0, 0],[0, T₁₁]]` — **diagonal** |

That is the answer to why `-matri` changed nothing (§4j): in the coordinates
that matter it leaves the differenced equation with its own moving average,
which is the one that runs to the unit circle. And `-mawarma` is exactly
Phillips' triangular form **with the diagonal parameterisation the theorems
assume** — not an approximation of it.

It also says where the restriction belongs. In `Ȳ` coordinates it is a pattern
of zeros; in VEC coordinates the same restriction couples `Θ` to `B₂`
(`T₁₂ = T₁₁B₂′`) and has to be rebuilt at every likelihood evaluation. The
legacy `drv_project` parameterises `Ȳ`'s VARMA directly and forms `W` by
**subtraction** — `β` enters only through the data, like the input of a transfer
function — which is why its surface is better conditioned
([ESTUDIO_BVECM_vs_DRVEC.md](ESTUDIO_BVECM_vs_DRVEC.md) §3.1).

### Are these data in the triangular class in the AR block? Indication: no

`F̂₁ = m·α′` would make `F̂₁` rank 1 with row space `α′`. On the admissible
(`-marow`) fits:

| pair | `σ₂/σ₁` of `F̂₁` | \|cos(row space, `α′`)\| |
|---|---|---|
| Milan | 0.073 | 0.163 |
| Strasbourg | 0.097 | 0.089 |
| Utrecht | 0.862 | 0.636 |
| Vienna | 0.005 | 0.315 |
| Aix | 0.744 | 0.219 |
| Arévalo | 0.111 | 0.435 |
| Angers | 0.003 | 0.508 |
| Penn | 0.952 | 0.964 |

Five of the eight are near rank one, which the restriction predicts — but the
row space is `α′` in only one of them. So the reduction happens in a direction
the triangular class does not allow.

**This is an indication and not a test**, and §4p now shows it was a
**misleading** one. `F̂₁` is estimated freely and its row space is a weakly
determined direction; imposing the restriction and testing it properly reverses
the reading in seven of the eight pairs.

## 4l. The theorems' class, estimated in its own coordinates

*`-warma`. §4k said the restrictions belong in the coordinates the triangular
model is stated in. This estimates them there: `Φ*_k = [0  Ψ_k ; 0  Φ_k]`,
`Θ*_k` in the `W` block only, and `B₂` entering **only** through
`W = Y₁ + B₂′Y₂` — by subtraction, like the input of a transfer function, with
no parameter depending on it. No `C̄`, no `Φ̄` recursion: the parameters already
are the transformed system's.*

### It recovers its own truth

Simulated from the WARMA process of Corollary 2, `n = 8000`, `p = 1`:

| | truth | `-warma` |
|---|---|---|
| coefficients of `W_{t−1}` | (γ, φ) = (0.20, 0.60) | (0.209, 0.518) |
| `Θ*` | 0.50 | 0.404 |
| `B₂` | −0.50 | **−0.49994** |

### And the two casts agree to nine decimals

With `p = 1` there are no `F` lags, so `-warma` and `-mawarma` describe **the
same family** by two routes that share no code — one transforms VEC → VARMA at
every likelihood evaluation, the other writes the VARMA directly. On the WARMA
simulation they give −22598.3542436254 and −22598.3542436269; on Utrecht,
64.1597098018 and 64.1597098014. That identity is in the suite, and it is the
strongest check either cast has.

### On the bank, at `p = 2`

`-warma` adds to `-mawarma` the autoregressive half of the class — every lag
entering through `W`, i.e. `Γ_i = M_lα′` in VEC terms.

| pair | `-warma` logL / `B̂₂` / MAmin / term | `-mawarma` | LR of the AR half, 2 df |
|---|---|---|---|
| Milan | 65.918 / −0.622 / 3.98 / **grad** | 81.313 / −0.566 / 2.41 | **30.79** |
| Strasbourg | 22.428 / −0.668 / 25.06 / **grad** | 24.451 / −0.640 / 4.93 | 4.05 |
| Utrecht | 65.905 / −0.864 / 6.97 / **grad** | 70.888 / −0.789 / 7.53 | **9.96** |
| Vienna | 20.275 / −0.875 / 1.93 / **grad** | 21.572 / −0.866 / 1.63 | 2.59 |
| Aix | 61.198 / −0.957 / 1.35 / **grad** | 62.128 / −1.029 / 1.38 | 1.86 |
| Arévalo | −14.422 / −1.007 / 14.33 / **grad** | −13.570 / −0.995 / 12.55 | 1.70 |
| Angers | 18.196 / −0.712 / 1.00 / **grad** | 16.291 / −1.030 / 6.01 | **−3.81** |
| Penn | 30.092 / −1.100 / 18.07 / **grad** | 32.475 / −1.088 / 1.72 | 4.77 |

**Eight of eight converge on the gradient** — no other specification in this
register does — with moving-average roots from 1.0 to 25.1 and `B̂₂` in the
canonical region throughout.

The autoregressive half of the class is **not rejected at 5 % in five of the
eight** (`χ²(2)` is 5.99); it is rejected on Milan and Utrecht. And Angers gives
a **negative** LR: the *less* restricted `-mawarma` fit is 3.8 units **worse**
than the more restricted `-warma` one, though both stopped on the gradient. A
nested model cannot beat the model containing it, so what that measures is the
VEC-coordinate parameterisation getting stuck where the `Ȳ`-coordinate one does
not — the conditioning difference of
[ESTUDIO_BVECM_vs_DRVEC.md](ESTUDIO_BVECM_vs_DRVEC.md) §3.1, now with a number
on it.

### The bank, with `-multistart 20`

Same instrument as §4b and §4c, `tools/measure_seeding_bank.sh -warma`, five
seconds for the lot:

| case | logL₀ | logL | term | best/20 | logL spread | (C)'s spread | MAmin |
|---|---|---|---|---|---|---|---|
| Milan | 16.526 | 65.918 | grad | 6 (17 ok) | **0.000** | 0.435 | 3.98 |
| Strasbourg | −32.182 | 22.428 | grad | 1 (19 ok) | 0.985 | 2.113 | 25.06 |
| Utrecht | 30.834 | 65.905 | grad | 8 | **0.056** | 0.539 | 6.97 |
| Vienna | −65.931 | 20.275 | grad | 2 (19 ok) | 1.323 | 4.688 | 1.93 |
| Aix | 55.410 | 61.198 | grad | 2 | **0.000** | 0.929 | 1.35 |
| Arévalo | −41.289 | −14.422 | grad | 17 | 4.017 | 2.761 | 14.33 |
| Angers | −11.860 | 18.196 | grad | 7 | 1.944 | 8.725 | 1.00 |
| Penn | −9.432 | 30.092 | grad | 8 | **0.306** | 18.688 | 18.07 |
| `mink_muskrat` c1 | −347.847 | −179.321 | lower | 11 | 190.653 | 15.259 | 1.000 |
| `mink_muskrat` c2 | −203.382 | −13.737 | step | 11 | 179.239 | 15.329 | 1.000 |
| `mink_muskrat` c3 | −203.098 | −36.438 | lower | 13 | 175.140 | 16.774 | 1.000 |

**On the eight pairs the surface is a different object.** Seven of the eight
spreads are smaller than route (C)'s, and on Milan and Aix the spread is
**exactly zero** — every one of the converged starts lands on the same point,
which is the definition of a well-behaved surface the multi-start block prints
for itself. Penn goes from a spread of 18.7 to 0.31. And the starting point no
longer matters: `logL₀` is 30 to 90 units below the optimum on several of them
(the seed is shrunk until admissible, which throws it far) and the fit converges
on the gradient anyway. Under the free parameterisation a start that bad decides
the answer.

**And on `mink_muskrat` the class simply does not fit.** Not a seeding artefact:
the fits converge (11 to 13 of 20 usable starts, all 20 admissible once the
multi-start jitters the shrunk seed rather than the raw one) and land at −179,
−14 and −36 against the free fits' 3.7, 6.5 and 6.5. The triangular class, which
carries the wheat pairs comfortably, is wrong for the predator–prey data — where
Johansen and `drvec` already disagreed about `B₂` (§4e), and where the register
has recorded a hard surface since the beginning.

That split is worth more than an average would be: the specification the
theorems cover works where the theorems were aimed, and fails visibly where it
does not apply, instead of degrading quietly everywhere.

### The same fit in VEC coordinates

Step 1 of [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md). The transformation is
inverted **once**, at the end, from `Φ̄_k = C̄⁻¹Φ*_k` and the recursion of Theorem
1: `F₁ = (Φ̄₁ − C̄⁻¹H̄)C̄ + Π`, `F_i = (Φ̄_i + F_{i−1}C̄⁻¹H̄)C̄`, and the equation
left over, `0 = Φ̄_p + F_{p−1}C̄⁻¹H̄`, determines `Λ`. Solved as an affine
least-squares problem in `Λ` so that no case analysis in `p` is needed and so
that the **residual** is available.

| pair | inversion residual | `Λ̂` | `G` | `B̂₂` |
|---|---|---|---|---|
| Milan | 4.2e−17 | (0.377, −0.195) | 0.997 | −0.622 |
| Strasbourg | 3.9e−17 | (0.307, −0.226) | 0.999 | −0.668 |
| Utrecht | 3.9e−17 | (0.570, −0.290) | 0.971 | −0.864 |
| Vienna | 0.0 | (0.453, −0.204) | 0.957 | −0.875 |
| Aix | 5.6e−17 | (0.583, −0.467) | 0.996 | −0.957 |
| Arévalo | 1.4e−17 | (0.328, −0.214) | 0.978 | −1.007 |
| Angers | 0.0 | (0.945, −0.072) | 0.856 | −0.712 |
| Penn | 8.8e−17 | (0.487, −0.412) | 0.991 | −1.100 |

**The residual is machine zero everywhere**, which says the two coordinate
systems describe the same fit and not an approximation of it. It is printed
because a residual away from zero would mean the fitted point is outside the
image of the map and the `Λ` shown is a projection — the failure mode this
program has published twice by accident
([DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) §8d, §8h), now checkable in one
number.

Two things come out of the inversion **by themselves**, and both are the theory
appearing without being imposed: the recovered `Θ̂` has its differenced block
exactly zero (Theorem 6, condition 6), and the recovered `F̂₁` has rank one with
row space `B′` (condition 5) — on the simulated WARMA data, rows
`(0.0325, −0.0163)` and `(0.00033, −0.00016)`, both proportional to `(1, −0.5)`.

### Where this leaves the specification question

Three routes now put `B̂₂` in the same region — the canonical estimator, the
inherited moving average, and the triangular parameterisation — and all three
are admissible. The free VEC fit is the one that disagrees, and it is the one
whose optimum denies its own rank. That is as far as the measurements go: they
do not say the triangular class is true, they say the answer stops moving as
soon as the fit is required to be a model of the rank it claims.

## 4m. The specification ladder, read on four cases

*`-specs`. Step 2 of [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md): the five
nested specifications in one run, with the admissibility column first.*

```
Milan                              Arévalo
spec    npar     logL  term      G  MAmin      B2 adm   |  spec      logL      G  MAmin      B2 adm
warma      9  65.9177  grad  0.997  3.982 -0.6224 yes   |  warma  -14.4221  0.978 14.326 -1.0070 yes
mawarma   11  81.3133  grad  0.995  2.410 -0.5663 yes   |  mawarma-13.5699  0.962 12.549 -0.9948 yes
marow     12  82.6558  grad  0.522 10.857 -0.6085 yes   |  marow  -13.4548  0.793  7.806 -1.0043 yes
matri     13  91.7596  lower 0.087  1.000 -0.4552 NO    |  matri  -11.1788  0.862  1.493 -1.0076 yes
free      14  93.2880  step  0.086  1.000 -0.4475 NO    |  free    -3.5626  0.057  1.000 -0.6272 NO
```

**The two regimes of `B̂₂`, now in one table.** On Arévalo every admissible rung
gives −0.99 to −1.01 and the inadmissible one gives −0.63; on Milan the
admissible ones give −0.57 to −0.62 and the inadmissible −0.45. The estimate
does not drift across the ladder — it **jumps** when the fit leaves the class.

**Penn shows the other failure the column is for.** Its `matri` rung fits 41.55
against the free model's 41.37, so the ladder prints
`matri -> free  −0.360  NEGATIVE: the wider fit is worse, so it did not
converge`. A nested model beating the model that contains it is arithmetic, not
evidence, and the ladder says so rather than ranking them.

**`mink_muskrat` again splits from the pairs.** Four of its five rungs are
admissible, the free one is not, and the admissible `B̂₂` runs −0.10 to −0.20
against the free −0.24 — the same shape as the pairs, on a dataset where the
triangular rung fits 20 units worse.

## 4n. The rank test, and what the moving average was doing to it

*Step 3 of [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md), and the measurement
with the most at stake: `-lrtest` fits a free `Θ` at every rank, and those are
exactly the fits §4h shows landing where the model denies its own rank. All
p-values below are **bootstrap** ones, 100 replications, because the asymptotic
table is measured to over-reject six-fold (§2.3).*

| pair | `q = 0`: LR / p / reps | `q = 1` free | `q = 1` admissible |
|---|---|---|---|
| Milan | 25.06 / **0.011** / 90 | 31.02 / **0.022** / 89 | 23.45 / **0.024** / 83 |
| Strasbourg | 25.86 / **0.010** / 97 | 21.98 / 0.067 / 88 | 17.37 / 0.095 / 73 |
| Utrecht | 30.74 / **0.010** / 97 | 17.09 / 0.238 / 100 | 12.69 / 0.250 / 79 |
| Vienna | 21.87 / **0.010** / 95 | 26.68 / **0.011** / 90 | 9.17 / 0.318 / 62 |
| Aix | 27.39 / **0.010** / 95 | 10.05 / 0.598 / 96 | 7.51 / 0.644 / 86 |
| Arévalo | 44.19 / **0.010** / 95 | 29.56 / **0.044** / 89 | 19.95 / 0.185 / 64 |
| Angers | 32.39 / **0.011** / 91 | 22.03 / 0.068 / 73 | 1.81 / 0.813 / 47 |
| Penn | 42.17 / **0.010** / 96 | 10.06 / 0.575 / 79 | 7.09 / 0.623 / 76 |

**Without a moving average the eight pairs cointegrate, unanimously and at the
resolution floor.** Every bootstrap p-value is 0.010–0.011, which with `B = 100`
is `1/(B+1)` — "stronger than a hundred replications can resolve". There is no
ambiguity about these data at `q = 0`.

**Adding a free moving average destroys that evidence**, and now there is a
reason rather than a shrug. By Theorem 3 of [THEORY.md](THEORY.md) the rank is
`r` only while `Λ⊥′Θ(1)` keeps full row rank, and §4h measures the free `r = 1`
fits landing with `Θ̂(1)` singular in almost exactly the `Λ⊥` direction — the one
that annihilates the common trend. **The moving average absorbs the very thing
the rank test is measuring.** So the `q = 1` free column is not a weaker test of
the same hypothesis; it is a comparison in which the alternative is not a
rank-one model.

**Enforcing admissibility does not restore the evidence either**, and it should
not be expected to: the constraint binds on `L(1)` and leaves `L(0)` untouched —
there is no rank condition at `r = 0` — so the statistic can only fall. It falls
a long way: Vienna from 26.7 to 9.2, Angers from 22.0 to 1.8. What that column
establishes is not a verdict but a bound: **whatever evidence the `q = 1` fit
appeared to give, most of it was bought in the inadmissible region.**

### The protocol that follows

**Select the rank with `q = 0`, then fit the moving average at the selected
rank.** At `q = 0` the instrument is clean — no `Θ`, so no admissibility
question, and the answer here is unanimous — while at `q = 1` neither column is
a usable test: the free one compares against an alternative outside the model
class, and the constrained one biases the statistic down by construction.

This is the same phenomenon §3b of this register recorded from the other side,
where the lags needed to whiten the residuals destroyed the rank test. Both are
the alternative's fit being spent on something other than cointegration.

*[open] A test of the rank at `q ≥ 1` that is neither inadmissible nor
downward-biased is not built. It would need the alternative estimated in a class
that cannot degenerate — `-marow` or `-warma` — but those restrictions are
defined relative to the `r/s` split and **collapse at `r = 0`**: with `r = 0`,
`-mawarma` and `-marow` annihilate `Θ` entirely (measured: Milan `r = 0`
`-marow` gives 64.1210, exactly the `q = 0` fit) and `-warma` has no dynamics
left at all and fails to estimate. So the null and the alternative cannot be
expressed in the same restricted family, which is a structural obstacle and not
an oversight.*

## 4o. The default, decided and not moved

Step 4 of [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md). **No row of this
register changed**, and that was verified rather than assumed: the default
computation is untouched and the suite is green on every golden value.

What changed is that a fit which denies its own rank now says so on the
terminal, not only in the `.out`, with the command that shows the alternatives.
The reasoning for keeping the free model as the default — and for refusing both
a restricted class and `-rankadm` in that role — is §8 of the plan, and it rests
on two measurements in this register: §4i, where the inherited class is rejected
in eight of eleven cases, and §4n, where the constrained fit is shown to be a
downward-biased bound rather than an answer.

> **Superseded by §4r.** Both legs of that reasoning survive as statements about
> `-rankadm` and fall as statements about the class of Theorem 6. There is no
> bound to bind in that class — Corollary 6.3 makes `𝒞` the whole parameter
> space, so it is a **parameterisation** and not a restriction — and §4i's
> rejection was measured against an alternative whose optimum is on the boundary,
> where the statistic has no distribution. The default is decided again in §4r,
> and this time it moves.

## 4p. The autoregressive half of the class, tested — and it survives

*`-artest N`. Step 5 of [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md).
`H₀`: `Γ_i = M_iα′`, every lag entering through `W`, which is `-warma`.
`H₁`: `F_i` free, which is `-mawarma`. The moving-average structure is held in
both, so the comparison isolates the autoregressive half. 100 replications.*

**Why this one is bootstrapped is not the reason §4i had.** Here the
unrestricted model **is** admissible — the rank condition reads 0.90 to 1.00
across the bank — so this is not a boundary of the model class. It is that
`Γ_i = M_iα′` is a **reduced-rank** restriction on `F_i`, and the likelihood
ratio for a rank restriction is not `χ²` when the true rank may be below the one
the restriction allows: in five of the eight pairs `F̂₁` comes out near rank one
already, and `M_i` is then close to unidentified.

| pair | LR | `χ²(2)` p | **bootstrap p** | boot 10% | 5% | 1% |
|---|---|---|---|---|---|---|
| Milan | 30.79 | 0.0000 | **0.0102** | 6.53 | 10.17 | 17.40 |
| Strasbourg | 4.05 | 0.1322 | 0.3646 | 8.71 | 10.47 | 29.23 |
| Utrecht | 9.96 | 0.0069 | 0.0606 | 7.21 | 10.12 | 15.41 |
| Vienna | 2.59 | 0.2732 | 0.4421 | 9.81 | 14.28 | 23.78 |
| Aix | 1.86 | 0.3943 | 0.6289 | 6.95 | 11.00 | 15.63 |
| Arévalo | 1.70 | 0.4265 | 0.5149 | 8.02 | 11.22 | 21.64 |
| Angers | **−3.81** | 1.0000 | 1.0000 | 7.47 | 9.33 | 23.80 |
| Penn | 4.77 | 0.0923 | 0.2551 | 7.34 | 8.77 | 47.64 |

`χ²(2)` has 4.61 / 5.99 / 9.21.

**The restriction is not rejected in seven of the eight pairs.** Only Milan
rejects, at the resolution floor; Utrecht is marginal at 0.061. Angers returns a
negative statistic — the *unrestricted* fit is worse — which is the `-mawarma`
non-convergence already recorded in §4l, and the bootstrap reports it as
`p = 1` rather than dressing it up.

The bootstrap 5 % values run 8.8 to 29.2 against `χ²(2)`'s 5.99, a factor of 1.5
to 4.9 — the same over-rejection of the asymptotic reference that §2.3 and §4i
measure on two other statistics.

### This corrects §4k

§4k measured that `F̂₁`'s dominant row direction matches `α′` in only one of the
eight pairs and called it an indication that these data are not in the class in
the autoregressive block. **Tested, that reading is wrong.** The row space of a
freely estimated `F̂₁` is a weakly determined direction, and a distance between
two weakly determined directions is not evidence. Imposing the restriction and
simulating its distribution reverses the conclusion in seven of the eight cases.
The lesson is the one this record keeps relearning: an eyeballed distance is not
a test, and the difference is not a detail here — it is the difference between
"the class does not fit these data" and "the class fits seven of the eight".

### And the class splits cleanly in two

| half of the triangular class | verdict on the bank |
|---|---|
| the moving-average half (`Θ` inherited, §4i) | **rejected** at 5 % in eight of eleven |
| the autoregressive half (`Γ_i = M_iα′`, here) | **not rejected** in seven of eight |

So the theorems' class fails on its moving-average half and survives on its
autoregressive one. What these data reject is the assumption that the
differenced block carries no moving average of its own — not the assumption that
the short-run dynamics enter through the cointegrating combination.

## 4q. When is the moving average estimable at all? The sign of the coefficient

*`tools/sim/ma_identification.py`, five modes. The question: the program
estimates a free `Θ` badly on the bank and well on some simulated data, and
nothing in this register said **when**. The answer is not the sample size, the
number of moving-average parameters, or the optimiser. It is which side of the
real axis the moving-average zero sits on.*

> **Sign convention, because the whole section turns on it.** Box and Jenkins,
> which is this program's own (`README.md`: the model carries
> `(I − Θ₁L − …)Aₜ`). So `θ = 0.8` **is** the operator `(1 − 0.8B)`, and a
> negative `θ` below means `(1 + |θ|B)`. Every table prints the operator next to
> the coefficient.

### The instrument

The DGP is `sim_vec.py`'s — `B₂ = −0.5`, `Λ = (0.30, 0.10)`, `F₁ = 0.2I`,
`Θ₁ = θI`, `Σ = I` — and its AR roots do not depend on `θ`. They are known in
closed form rather than estimated: with `Φ(L) = (I − F₁L)(1−L) + Λβ′L` and
`c = Λ₁ + Λ₂B₂ = 0.25`,

```
det Φ(L) = a(L)·(a(L) + cL),      a(L) = (1 − 0.2L)(1 − L)
```

so the roots are **1** (the unit root), **5.0**, **1.5746** and **3.1754** — all
real and positive. Knowing they are real is what makes a signed comparison with
the moving-average zero legitimate.

### The measurement

```sh
python3 tools/sim/ma_identification.py sign 15
```

Matched magnitudes, both signs, 15 replications per cell, `Θ` **free**
(`p = 2, q = 1, r = 1, -case 1`). Median and interquartile range of `Θ̂₁₁`:

| \|`θ`\| | operator | MA zero | dist. to nearest AR root | `n` | `Θ̂₁₁` median | **IQR** | on the gate |
|---|---|---|---|---|---|---|---|
| 0.1 | `(1−0.1B)` | 10.00 | 5.00 | 120 | 0.039 | **1.175** | 47 % |
| 0.1 | `(1+0.1B)` | −10.00 | 11.57 | 120 | −0.029 | **0.405** | 13 % |
| 0.2 | `(1−0.2B)` | 5.00 | 0.00 | 120 | −0.051 | **0.984** | 60 % |
| 0.2 | `(1+0.2B)` | −5.00 | 6.57 | 120 | −0.107 | **0.239** | 13 % |
| 0.3 | `(1−0.3B)` | 3.33 | 0.16 | 120 | −0.099 | **1.461** | 47 % |
| 0.3 | `(1+0.3B)` | −3.33 | 4.91 | 120 | −0.236 | **0.198** | 7 % |
| 0.5 | `(1−0.5B)` | 2.00 | 0.43 | 120 | 0.274 | **0.661** | 33 % |
| 0.5 | `(1+0.5B)` | −2.00 | 3.57 | 120 | −0.496 | **0.081** | 0 % |
| 0.8 | `(1−0.8B)` | 1.25 | 0.32 | 120 | 0.453 | **0.552** | 20 % |
| 0.8 | `(1+0.8B)` | −1.25 | 2.82 | 120 | −0.808 | **0.115** | 27 % |

At `n = 250` the same split holds and widens where it matters: `(1−0.5B)` gives
0.233 with IQR 1.127 against a truth of 0.5, while `(1+0.5B)` gives −0.521 with
IQR 0.072.

**At every matched magnitude the negative-coefficient form is three to seven
times tighter, and lands on the invertibility gate far less often.** With
`(1 + |θ|B)` the program recovers a completely free 2×2 moving average at
`n = 120`: −0.496 against −0.500, IQR 0.081. With `(1 − θB)` it does not recover
it at any magnitude or at either sample size.

Two readings the table rules out:

* **not the number of parameters.** The same four free parameters are recovered
  to three decimals in the negative rows and not at all in the positive ones.
* **not the sample size.** `n = 120` succeeds in the negative rows where
  `n = 250` fails in the positive ones.

And in every row, whatever the moving average does, `|B̂₂ + 0.5| ≈ 0.005`.

### The reading this refutes, and it was measured here first

```sh
python3 tools/sim/ma_identification.py separation 15
```

This section first ran a different design: hold the AR roots fixed and vary the
**distance** from the true MA zero to the nearest of them, on the reasoning that
what fails is left coprimality — the assumption Mauricio (2006) and
Yap–Reinsel (1995) make by reference and never impose
([DEMOSTRACIONES.md](DEMOSTRACIONES.md) §7). That table separates cleanly, and
it is **an artefact**: every well-separated cell in it had `θ < 0` and every
close cell had `θ > 0`, because the AR roots are all positive real. Sign and
distance were collinear by construction.

The control that breaks the collinearity is in the table above: `(1 − 0.1B)`
puts its zero at 10.00, a distance of **5.00** from every AR root — further than
`(1 + 0.8B)`'s 2.82 — and is estimated with IQR **1.175** against that row's
**0.115**. And `(1 − 0.2B)` puts its zero **exactly on** the AR root at 5.0,
an exact common factor, and is no worse than `(1 − 0.3B)` at distance 0.16.

The `separation` mode is kept, and labelled, because the failure is the useful
part: a design in which the treatment is confounded with a nuisance produces a
clean-looking dose–response for the wrong variable, and the only thing that
caught it was running the control.

### Where the evidence does point

With `θ > 0` the moving-average zero lies on the **positive** real axis — the
same ray that carries the differencing operator's own zero at `L = 1`, the
surface `det Θ(1) = 0` that Theorem 4 of [DEMOSTRACIONES.md](DEMOSTRACIONES.md)
shows the likelihood reaches and rewards, and the overdifferencing signature that
Corollary 3.2 characterises (Plosser–Schwert 1977; Hillmer–Tiao 1979). There is a
continuous path from the truth to that surface along which the likelihood does
not fall away. With `θ < 0` the zero is on the negative axis and that path is not
available: reaching `L = 1` would require crossing zero.

**Stated as what it is: a reading consistent with the measurements and with the
theory already proved, not an isolated mechanism.** The metric has not been
identified — root distance is refuted, and "which side of the axis" is a
statement about this DGP's real zeros, not a quantity. The control that would
test it is a complex moving-average zero at a fixed modulus and varying argument,
which `sim_vec.py` cannot currently generate.

### It is not the optimiser

```sh
python3 tools/sim/ma_identification.py multistart 15
```

The same cells with `-multistart 20`. In the negative rows the medians are
unchanged to three decimals in five of six cells. In the positive rows nothing is
repaired: `θ = 0.5, n = 250` goes 0.233 → 0.236, `θ = 0.6, n = 250` goes
0.286 → 0.228, `θ = 0.4167, n = 120` goes 0.281 → 0.357. At `n = 4000` the 20
starts span **60.6 log-likelihood units** and the estimate does not move.

**And the restarts land on the gate more often, not less** — 7 → 13, 33 → 47,
13 → 27 across the positive rows. That is Theorem 4 appearing as a measurement:
the likelihood is finite and continuous on `det Θ(1) = 0` and **rewards** it, so
searching harder finds it more reliably. The optimiser is not failing; it is
succeeding at maximising something whose maximiser is not the truth.

### And the admissibility statistic does not see it

```sh
python3 tools/sim/ma_identification.py gap 25
```

`n = 4000`, `θ = 0.5` free, 25 replications, split at the median AR–MA root
distance. That distance is the metric the section above discredits, so it is used
here only to order the replications; the columns that matter are measured
directly:

| half | mean root gap | median max error in `Θ̂` | median `G` | median \|`B̂₂`+0.5\| |
|---|---|---|---|---|
| smaller gap | 0.0572 | **0.471** | **0.339** | 0.0008 |
| larger gap | 0.2424 | **0.181** | **0.236** | 0.0006 |

Whatever orders them, the half with the **larger** error has the **higher** `G`.
The rank condition `σ_min(Λ⊥′Θ(1)B⊥)` catches the catastrophic degeneracies —
one replication has `G = 0.012` with an error of 18.6 — and is blind to the
ordinary ones, which clear any floor comfortably while the estimate is useless.
`G` is a guard against a specific failure, not a measure of how much the
short-run estimates can be trusted.

### The restricted class does not escape it either

```sh
python3 tools/sim/ma_identification.py inherited 15
```

The WARMA truth of BVECM Corollary 2 (`Θ₁ = [[0.50, −0.25],[0, 0]]`), fitted free
and under the two restrictions. **Note where that DGP sits**: `sim_warma.py`
fixes `φ = 0.6` and the canonical run uses `θ = 0.5`, both positive, so the `w`
block is `(1 − 0.6B)w = (1 − 0.5B)a` — the hard regime twice over, a
near-common factor *and* both zeros on the positive axis.

| `n` | spec | `Θ̂₁₁` median (IQR) | `Θ̂₂₁` median | \|`B̂₂`+0.5\| | smallest MA root | `G` |
|---|---|---|---|---|---|---|
| 120 | free | −0.244 (1.716) | 0.282 | 0.0057 | **1.000** | 0.306 |
| 120 | `-mawarma` | −0.418 (0.366) | 0 | 0.0051 | 1.948 | 0.954 |
| 120 | `-marow` | −0.341 (0.749) | 0 | 0.0045 | 1.849 | 0.845 |
| 250 | free | 0.022 (2.940) | −0.022 | 0.0026 | 1.112 | 0.163 |
| 250 | `-mawarma` | 0.280 (0.830) | 0 | 0.0021 | 2.222 | 0.939 |
| 250 | `-marow` | 0.388 (1.154) | 0 | 0.0023 | 1.840 | 0.826 |
| 1000 | free | 0.303 (0.764) | 0.050 | 0.0014 | 1.525 | 0.995 |
| 1000 | `-mawarma` | 0.220 (0.845) | 0 | 0.0014 | 2.093 | 0.958 |
| 1000 | `-marow` | −0.047 (0.927) | 0 | 0.0013 | 2.143 | 0.928 |

The truth is `Θ₁₁ = +0.50`. At `n = 120` the restricted fit is tight (IQR 0.366)
and **centred on the wrong sign**; at `n = 1000` it is at 0.220. `-mawarma` buys
**interiority and admissibility** — the smallest MA root never approaches the
gate and `G` sits at 0.94–0.97, against a free fit on the gate at `n = 120` with
`G = 0.31` — and it does **not** buy a recovered `θ`.

### What the bank looks like through this lens

The free fits at `p = 2, q = 1, r = 1`, moduli as the program prints them:

| case | AR | MA |
|---|---|---|
| mink–muskrat | 1.176, 1.176, 1.744 | 1.064, 0.99995* |
| Penn | 2.046, 2.046, 3.334 | 0.99997*, 3.161 |
| Arévalo | 4.031, 1.419, 7.528 | 0.99996*, 1.739 |
| Aix | 4.048, 1.594, 1.594 | 0.99995*, 3.720 |
| Milan | 1.996, 1.996, 1.715 | 1.00026, 2.486 |
| Angers | 1.744, 1.300, 1.300 | 0.99996*, 0.99996* |
| Strasbourg | 2.821, 2.821, 1.632 | 0.99996*, 8.940 |
| Utrecht | 1.796, 1.796, 1.419 | 0.99995*, 4.119 |
| Vienna | 1.893, 1.893, 1.487 | 0.99995*, 12.53 |

**Eight of the nine have a root pinned at 0.99995**, which is exactly the
threshold `chekma` enforces (`elfvarma.c` raises `ifault = 1` when the companion
eigenvalue reaches 1.00005), so the gate — not the data — is what stops them
there. These are differenced price and population series whose moving averages
are of the `(1 − θB)` kind: **the bank sits in the hard regime by its nature, not
by accident**, which is why every symptom in §4g–§4h appears on it at once.

### What this settles, and what it does not

**Settles.** The moving-average difficulty is not a defect of the cast, the
engine or the optimiser, and not a consequence of leaving `Θ` free: the same free
`Θ` is recovered to three decimals at `n = 120` in the `(1 + |θ|B)` regime.
`-multistart` makes it worse, not better, in exactly the way Theorem 4 predicts.
`G` does not measure it. `B̂₂` is unaffected by all of it, in every cell of every
table here. And the restricted classes buy admissibility, not accuracy.

**Does not settle.** One DGP family (`M = 2`, `r = 1`, `p = 2`, `q = 1`), 15 to
25 replications per cell, real moving-average zeros only, and no metric: "which
side of the axis" describes this design, it does not generalise on its own. The
threshold, if there is one, is not calibrated.

**And a correction to this section's own first reading**, kept because the
register is worth more for what it retracts than for what it asserts: the
distance between the AR and MA roots was measured, produced a clean
dose–response, and is **not** the cause. The design that produced it confounded
the treatment with the sign of `θ`, and only the matched-magnitude control
exposed it. The `separation` mode remains in the instrument, labelled, for that
reason.

**Where it stands against the external review.**
[external_review.md](external_review.md) reaches the same conclusion about what
is *not* at fault, and its account of the boundary — T4 and C3.2, the likelihood
rewarding `det Θ(1) = 0` and overdifferencing as the signature — is **the reading
these measurements support**, against this section's first one. Two places where
the measurements still qualify it. Its §1.3 makes the defect the free `Θ`'s
`q·M²` parameters against the class's `q·r²`: the `sign` table estimates those
four parameters to three decimals when the regime is favourable, and the
restricted class misses its own truth when it is not, so the parameter count is
not what fails. And its S1 — an admissibility floor inside the cast — acts on a
statistic the `gap` table measures to be higher in the worse half.

**Two caveats on the proofs, found while using them.** T3, C3.2 and T4 were
followed step by step and hold. T4's parenthesis says the gate "rejects only the
points strictly inside": it rejects what is more than 5·10⁻⁵ inside and admits a
thin non-invertible layer, which is the margin `chekma` enforces and which the
external review's H4 states correctly. And T6's necessity argument for (5) is
informal where its sufficiency argument is not: necessity follows from the direct
construction only if the VEC representation of a given process is unique, and
that uniqueness **is** the identification assumption §7 of the same document
records as assumed by reference.

**And it corrects one reading already in the register.** §4g's inference from the
WARMA simulation — that the free estimator finds the inherited structure by
itself — holds, but on a DGP with `φ = 0.6` against `θ = 0.5`, both positive, and
at `n = 8000`. It is weaker evidence about the estimator than it reads as.

## 4r. The class of Theorem 6 recovers its own truth, and the pathology cannot occur in it

*The question §4q left open: if the free parameterisation cannot be trusted in
the regime the bank lives in, is there one that can? Corollary 6.3 of
[DEMOSTRACIONES.md](DEMOSTRACIONES.md) says the answer is algebraic rather than
numerical, and this section measures both halves of it.*

### The algebra, checked and not only proved

```sh
python3 tools/sim/ma_identification.py algebra
```

With the bottom `s` rows of every `Θ̃ₖ` zero — the condition (6) of Theorem 6, and
also the weaker class `-marow` imposes, whose cross block is free:

| `r` | `s` | `q` | \|`det Θ̃(1) − det(I_r − ΣT_k)`\| | max \|eig − eig of the `r×r` block\| |
|---|---|---|---|---|
| 1 | 1 | 1 | 0.00e+00 | 0.00e+00 |
| 2 | 3 | 1 | 0.00e+00 | 0.00e+00 |
| 2 | 1 | 2 | 0.00e+00 | 8.12e−16 |
| 3 | 2 | 3 | 0.00e+00 | 1.27e−15 |
| 1 | 4 | 2 | 0.00e+00 | 0.00e+00 |

So in this class `Θ̃(L)` is invertible **iff** its `r × r` block is, and by
Corollary 3.1 the rank condition of Theorem 3 then holds automatically. The
consequence is the one that matters for the program: the point of `𝒫 \ 𝒞` that
Theorem 4 shows the likelihood reaches and rewards, and that **Theorem 5 shows no
root check of the engine can see, is not reachable here** — `chekma` on `Θ̃` is
exactly `chekma` on the `r × r` block. With `M = 2, r = 1` the admissibility
condition stops being the rank of a 2×2 matrix the engine cannot see and becomes
`|θ_w| < 1`, a scalar it already checks.

### And it recovers its truth, where the free version does not

```sh
python3 tools/sim/ma_identification.py structural 15
```

The WARMA DGP, whose `w` block is `(1 − 0.6B)wₜ = (1 − θB)aₜ` with `φ = 0.6`
fixed. `|φ − θ|` is the distance from an exact common factor: small means that
ARMA(1,1) is **not estimable by any method**, so a failure there says nothing
about the class. Truth in VEC coordinates: `Θ₁ = [[θ, −0.5θ],[0,0]]`.

| `θ` | \|`φ−θ`\| | spec | `n` | bias | IQR | \|`B̂₂`+0.5\| | MAmin | `G` |
|---|---|---|---|---|---|---|---|---|
| −0.5 | 1.10 | `-mawarma` | 120 | +0.034 | 0.402 | 0.0100 | 2.15 | 0.98 |
| −0.5 | 1.10 | `-mawarma` | 250 | **+0.006** | **0.261** | 0.0048 | 2.03 | 0.98 |
| −0.5 | 1.10 | free | 250 | +0.087 | 0.554 | 0.0049 | 1.89 | 0.71 |
| **+0.9** | 0.30 | `-mawarma` | 120 | +0.003 | 1.626 | 0.0016 | 1.11 | 0.93 |
| **+0.9** | 0.30 | `-mawarma` | 250 | **+0.050** | **0.272** | 0.0009 | 1.05 | 0.96 |
| **+0.9** | 0.30 | free | 250 | −0.006 | 0.584 | 0.0009 | **1.000** | 0.45 |
| +0.1 | 0.50 | `-mawarma` | 250 | −0.099 | 0.976 | 0.0032 | 1.55 | 0.97 |
| +0.1 | 0.50 | free | 250 | +0.314 | 1.125 | 0.0031 | 1.20 | 0.44 |
| +0.5 | 0.10 | `-mawarma` | 250 | −0.220 | 0.830 | 0.0021 | 2.22 | 0.94 |
| +0.5 | 0.10 | free | 250 | −0.478 | 2.940 | 0.0026 | 1.11 | 0.16 |

**`θ = +0.9` is the case that matters.** A positive coefficient — the hard regime
of §4q — with its zero at 1.11, hard against the unit circle. The free fit parks
**on the gate** (smallest root 1.000, `G = 0.45`) with twice the dispersion.
`-mawarma` recovers it with bias 0.050 and IQR 0.272, interior, `G = 0.96`.

**`G` does not fall below 0.93 in any cell of the structured class**, against
0.16–0.79 for the free one, and the smallest MA root never reaches the gate.
That is Corollary 6.3 appearing as a measurement rather than as algebra.

Where `-mawarma` is poor — `θ = +0.5` with `|φ−θ| = 0.10` — the `w` block is a
near-cancelling ARMA(1,1) and no parameterisation can help. It is a property of
the data-generating process, not of the class. `θ = +0.1` is weak-MA and also
poor, and costs correspondingly little.

### What this reverses

[SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md) §8 and §4o of this register
decided the default by arguing that making `-rankadm` the default would replace
one known error with another, because the constrained fit binds on `L(1)` and
leaves `L(0)` alone and is therefore a **downward-biased bound**. That argument
is correct about `-rankadm` and **does not apply** to the class of Theorem 6:
there is no bound there to bind, because `𝒞` is the whole parameter space. The
free model and the structured one are not a model and a restriction on it that
must be tested; they are two parameterisations, one of which contains points the
model class does not admit.

It also changes how §4i should be read. That section rejected the inherited
structure in eight of eleven cases — measured against a **free alternative whose
optimum sits on the boundary**, where by Theorem 10 and Corollary 5.1 the
statistic has no distribution. §4i says so itself. What it establishes is that
the alternative is outside the class, not that the class is wrong. Read beside
§4p, which does not reject the autoregressive half in seven of eight, the whole
class of Theorem 6 stands with its moving-average half admissible by construction
and its autoregressive half not rejected — at `q·r²` moving-average parameters
instead of `q·M²`, which for `M = 2, r = 1` is **one instead of four**.

### What is still not measured, and it is the one that decides

None of this shows the model **forecasts** better than a univariate one, which is
the only criterion that makes a multivariate specification worth its parameters.
`drvec` cannot forecast: there is no horizon option among its thirty-three, no
out-of-sample evaluation, and no comparison against `fue`. The register has
seventeen hundred lines about estimation and nothing about the question the
estimates exist to answer. The precedent is in the suite — the transfer-function
program's `forecast.c`, and its port's rolling-origin evaluation, whose own help
calls it *"the only way to decide EMPIRICALLY whether one model forecasts better
than another"*.

## 4s. The forecast against an oracle, at every horizon

*`tools/sim/forecast_oracle.py`. The other programs of the suite each have an
oracle — an independent computation to check against — and `drvec` did not. Its
two forecast certificates ([FORECAST.md](FORECAST.md) §3, §4) are internal
consistency checks: the recursion against the engine's residuals, and the
one-step band against the parameter vector. Neither says anything about
horizons beyond one, which is where the accumulation `C_m` and the cumulated
`Y₂` error live — exactly the part no source documents.*

### What the literature does and does not say

Searched: of the fifteen works in `literature/`, the ones that mention
forecasting at all mention it as a **motivation**. Ahn and Reinsel (1990) note
that imposing unit roots improves long-horizon forecasts and cite others for it;
Mauricio (2006) lists "forecast such process" as one of three purposes;
Yap and Reinsel (1995) speak of "efficient prediction". **None writes the
recursion for this class**, and the reference the last two point to for it —
*Journal of Time Series Analysis* 13, 353–375 — is not in the bank. What
licenses the procedure is therefore stated and proved here rather than cited:
Proposition 2 of `DEMOSTRACIONES.md` §6b, that the transformation loses no
information, so forecasting on `Ȳ` and inverting **is** the conditional
expectation of `Y`.

### The oracle

No formula of `drvec`'s is involved. The data-generating process is simulated,
`drvec` is handed the sample, and then the **same process** is continued 20 000
times from its true final state with fresh shocks. The average of the
continuations is the conditional expectation and their spread is the forecast
error dispersion, by construction.

```sh
python3 tools/sim/forecast_oracle.py 20000 6
```

`gap/sd` is the largest discrepancy between the point forecast and the oracle's
mean, in units of the oracle's own standard deviation; `se ratio` is the
program's band over the oracle's dispersion. At `n = 20 000`:

| `q` | `θ` | class fitted | DGP in the class? | `gap/sd` h=1, 3, 6 | `se ratio` h=1, 3, 6 |
|---|---|---|---|---|---|
| 0 | — | (vacuous) | **yes** | 0.010, 0.016, 0.010 | 1.000, 1.006, 1.007 |
| 1 | +0.5 | `-mafree` | **yes** | **0.022, 0.025, 0.016** | **1.000, 1.001, 0.995** |
| 1 | −0.5 | `-mafree` | **yes** | **0.019, 0.020, 0.018** | **1.000, 1.005, 1.008** |
| 1 | +0.5 | default `-marow` | no | 0.097, 0.104, 0.091 | 1.008, 1.058, 1.092 |
| 1 | −0.5 | default `-marow` | no | 0.119, 0.115, 0.116 | 1.035, 1.003, 1.039 |

**The algorithm is right.** Where the model contains the truth, the point
forecast matches the true conditional expectation to about **2 % of a forecast
standard deviation at every horizon**, and the bands to within **0.5 %**. That
covers the cumulation, the accumulated weights and the level reconstruction —
everything §3 and §4 of [FORECAST.md](FORECAST.md) could not reach.

At `n = 2 000` the same figures are 0.06–0.13 and the bands 1.5–6 % wide, and
they shrink with the sample: the residue at `n = 20 000` is estimation error,
which is the caveat Proposition 2 leaves open, measured rather than assumed.

### And it measured the cost of the P4 default, which was not the plan

The last two rows are a **misspecified** fit, and they were not put there on
purpose: the DGP of `sim_vec.py` has `Θ₁ = θI`, so its **differenced block
carries a moving average of its own** — `Θ₂₂ = θ ≠ 0` — and the default class of
§4r sets that entry to zero by construction. The fit absorbs it elsewhere:

| | truth | default fit, `n = 20 000` |
|---|---|---|
| `Θ₂₂` | −0.500 | **0** (imposed) |
| `F₁[2][2]` | 0.200 | **0.567** |
| `Λ₂` | 0.100 | **0.037** |
| `B₂` | −0.500 | −0.499986 |

and the forecast pays **about 10 % of a standard deviation at every horizon**,
with bands up to **9 % too wide**. `B̂₂` is untouched, as everywhere else in this
register.

**This does not overturn §4r, and it is not an argument for going back.** The
bank's data are where the free class degenerates onto the invertibility boundary
at `n = 61`–`120`, and the default is what makes those fits estimable at all.
What this measures is the other side of that trade: on a process whose
differenced block genuinely carries a moving average, the default is
misspecified and the forecast pays for it. Which of the two costs dominates on
real data is not a question this section can answer — it is exactly what the
rolling-origin comparison against a univariate model exists to settle, and that
is still not built.

## 4t. Does it forecast better than a univariate model? Measured, and mostly not

*`tools/forecast_vs_univariate.py`. The criterion the whole specification
question exists to serve, and the one this register did not have. A likelihood
cannot answer it: the VEC nests the alternative, so its likelihood is higher by
construction. Only an out-of-sample comparison can.*

### The set-up

Both sides are scored the same way, by `-estwin E -f H`: estimated **once** on
observations `1..E`, parameters held **fixed**, the origin rolled forward one
datum at a time, each forecast compared with what actually happened. The
parameters never see the data they are scored against.

The counterfactual is `drvec`'s **own diagonal rung**: with `r = 0` and
`-diagar -diagma -diagcov` the exact likelihood factorises (Theorem 9), so that
fit **is** an ARIMA(`p−1`,1,`q`) on each series separately. Using it instead of
an outside program means both sides share the sample, the estimator, the
forecast recursion and the scoring, so what is left between them is the
cointegrated structure and nothing else.

`E` is 75 % of each sample; RMSE is averaged over the two series; a ratio below
1 means the VEC forecasts better.

### The three tables

```sh
python3 tools/forecast_vs_univariate.py 2 1 4 0.75
python3 tools/forecast_vs_univariate.py 2 0 4 0.75
python3 tools/forecast_vs_univariate.py 2 1 4 0.75 -mafree
```

| case | `n` | `q=1`, default | `q=0` | `q=1`, `-mafree` |
|---|---|---|---|---|
| | | h=1 … h=4 | h=1 … h=4 | h=1 … h=4 |
| mink–muskrat | 61 | 1.453 1.124 1.076 1.038 | 1.053 1.012 **0.971** **0.985** | 1.077 1.027 **0.965** **0.947** |
| Milan | 111 | 1.129 1.159 1.190 1.218 | **0.998** 1.002 1.041 1.111 | 1.135 1.109 1.058 1.021 |
| Vienna | 112 | **0.916 0.864 0.781 0.749** | **0.990 0.985 0.995** 1.028 | **0.924 0.904 0.872 0.839** |
| Penn | 92 | **0.976 0.943 0.972 0.931** | **0.990 0.955 0.979 0.953** | **0.961 0.899 0.950** 1.098 |
| Utrecht | 112 | 1.117 1.352 1.521 1.623 | **0.943 0.947 0.970 0.999** | **0.832 0.778 0.739 0.708** |
| Aix | 89 | 1.753 2.009 2.127 2.111 | **0.906 0.877 0.905 0.897** | 1.609 1.757 1.724 1.575 |
| Arévalo | 112 | 1.076 1.161 1.269 1.358 | 1.012 1.023 1.011 1.008 | **0.914 0.870 0.847 0.835** |
| Angers | 89 | 2.472 2.929 2.781 2.130 | 1.112 1.166 1.208 1.118 | 1.675 1.836 1.870 1.598 |
| Strasbourg | 112 | 1.047 1.074 1.109 1.054 | **0.989 0.981** 1.027 1.107 | **0.976 0.910 0.898 0.902** |
| **VEC wins** | | **2/9 2/9 2/9 2/9** | **6/9 5/9 5/9 4/9** | **5/9 5/9 6/9 5/9** |

### What it says

**The moving average is what destroys the forecast.** With `q = 1` under the
default the VEC loses in seven of nine cases at every horizon, by up to a factor
of **2.9** (Angers). Drop the moving average and the same cases collapse to
within a few per cent of the univariate benchmark. That is §4q arriving where it
matters: a parameter that cannot be identified at these sample sizes is not free
to carry — it is paid for out of sample.

**At `q = 0` the cointegrated structure is roughly a wash.** The VEC wins about
half the comparisons and the margins are small — best 0.88 (Aix), worst 1.21
(Milan at `h = 4`). This is the received empirical result about multivariate
forecasting, reproduced here on this bank: beating a univariate model is hard,
and the structure that is worth something for **interpretation** is worth little
for **prediction**.

**And a result that runs against §4r's decision.** At `q = 1` the **free** class
forecasts *better* than the default in eight of the nine cases — Utrecht 1.117 →
0.832, Angers 2.472 → 1.675, Arévalo 1.076 → 0.914. The default sets `Θ₂₂ = 0`,
and on this bank the data want that entry: it is the same trade the oracle
measured in §4s from the other side, where imposing it on a process that has it
cost 10 % of a forecast standard deviation.

### The tension, stated rather than resolved

The three tables do not point the same way, and the honest reading is that **the
criterion decides the specification**:

* For **interpretation** — `B₂`, weak exogeneity, the long-run matrix — the
  default of §4r is right, and the argument has not moved: on this bank the free
  fits sit on the invertibility boundary with `G = 0.02`–`0.18`, deny the rank
  they were estimated at, and their `B̂₂` disagrees with Johansen's by 0.1 to 0.4
  (§4g). A better forecast from a model that denies its own rank is not a model
  to publish a cointegrating vector from.
* For **forecasting**, on this bank, the free class does better — while being
  inadmissible — and `q = 0` does better than both.

So the moving average in this model class is, on this kind of data, a liability
at both ends: unidentifiable when free (§4q), and costly when restricted (here).
The protocol in [GETTING_STARTED.md](GETTING_STARTED.md) already says to select
the rank at `q = 0`; these measurements say something stronger, and it is a
recommendation this register can now make on evidence: **for forecasting, `q = 0`
is not just where the rank test means what it says — it is where to stop.**

### What this does not establish

Nine bivariate cases, one estimation window, one `p`, and no test of whether any
difference is significant — the number of origins runs from 13 to 24, which is
too few for a Diebold–Mariano to say much. It is enough to answer the question
that had no answer at all, and not enough to rank specifications on a single
case.

## 4u. An applied case: three euro-area CPIs, and what four instruments say about it

*Monthly `IPC_ES`, `IPC_DE`, `IPC_FR` in logs, 1/2002–11/2023, from a price-level
and energy study. Univariate models from that study: `λ = 0`, `d = 1`, `D = 0`
and **deterministic seasonality at all frequencies** — eleven harmonics — so the
VEC is fitted on the deterministic-adjusted logs with `-interv`. In `drvec`'s
coordinates the univariate AR(1) on `∇log` is `p = 2, q = 0`. Training window
1/2002–12/2019, evaluation on what follows.*

### It began with a defect, and the defect is the first result

The `.pre` files of that study carry `refactor = 100`, which is the suite's
**norm** and not an oddity: the file that joins the univariate program to the
multivariate one is rescaled so the optimiser converges — in the C it hangs for
over two minutes at `refactor = 1` and converges in 23 iterations at 100. The
model in a `.pre` is therefore defined on `w = refactor · BoxCox(z)`, and its
`ω` are in the units of `w`.

**`-interv` subtracted them in the units of the data**, which is only right when
`refactor = 1`. Every case tested here until now had exactly that, so nothing
had ever caught it. On these three series the "adjusted" data came out with an
innovation variance **10⁴ times** its own — `σ² = 0.052` against `6·10⁻⁶` — and
the program said nothing.

It is not a new failure mode. The suite's own written discipline is *«never
hardcode the rescaling factor; read `model.refactor` — the suite has three
logged bugs from getting this wrong»*. This was the fourth, and `drvec` was the
one not following a rule the family had already paid for. Fixed, with the
invariance in the suite (§[8f]) and its negative control.

**Everything below is measured after the fix.** The first rank test run on this
case, before it, reported `r = 2` with `LR = 253.8`. That number was an artefact
and is recorded here only so the correction is visible.

### The rank: `r = 0`

Case 3, `p = 2, q = 0`, training sample, free short-run dynamics:

| `r` | npar | logL | AIC | BIC |
|---|---|---|---|---|
| 0 | 17 | 3127.93 | **−28.8049** | **−28.5393** |
| 1 | 22 | 3133.29 | −28.8083 | −28.4645 |
| 2 | 25 | 3133.47 | −28.7822 | −28.3915 |

Case 3 has no tabulated critical values, so the program's parametric bootstrap
(400 replications) supplies them:

| step | LR | 10 % | 5 % | 1 % | **p** | |
|---|---|---|---|---|---|---|
| 0 → 1 | 10.72 | 15.08 | 17.34 | 26.86 | **0.313** | not rejected |
| 1 → 2 | 0.36 | 9.29 | 10.99 | 15.49 | **0.956** | not rejected |

AIC prefers `r = 1` in the fourth decimal; BIC prefers `r = 0` clearly.

### Against Johansen, on the same adjusted series and sample

`k_ar_diff = 1`, which is this `p = 2` on `Ȳ`:

| specification | test | statistic | 5 % | verdict on `r = 0` |
|---|---|---|---|---|
| case 3 (`det_order = 1`) | trace | 33.92 | 35.01 | not rejected (rejected at 10 %: 32.06) |
| case 3 | λ-max | 22.09 | 24.25 | not rejected |
| case 2 (`det_order = 0`) | trace | 30.47 | 29.80 | **rejected** |
| case 2 | λ-max | 17.72 | 21.13 | not rejected |

**The same conclusion, with a different margin**, and the difference runs the way
§2.3 of this register already measured: the asymptotic test over-rejects and the
bootstrap is more conservative. Here the asymptotic sits on the 5–10 % edge and
the bootstrap gives 0.31.

**A note on how the sequence is read.** In the case-3 table the `r ≤ 2` row has
trace 4.11 against 3.84, so counting *how many rows reject* returns 1, while the
sequential procedure stops at the first non-rejection and returns 0.
`tools/compare_johansen.py` counts the first way (`sum(lr1 > cvt[:,1])`) and on
this case would give the right answer for the wrong reason.

### Imposing `r = 1`: the two programs disagree, and that is what `r = 0` means

Normalised on `IPC_ES`, as `drvec` must:

```
drvec:     W = log ES + 1.4012 log DE − 2.7734 log FR
Johansen:  W = log ES − 7.8930 log DE + 2.7744 log FR
```

On `Π = ΛB′`, which is invariant to the normalisation, the largest entry-wise
difference is **0.150** against norms of 0.209 and 0.166; the angle between the
two cointegrating directions is **49.8°** and between the adjustment directions
**45.9°**.

**Neither is wrong.** With rank zero the cointegrating vector is not identified —
there is nothing to converge to — and two estimators maximising different
criteria land in different places. The disagreement measures the
non-identification; it is not evidence against either program. Part of it is
normalisation: Johansen's `β` normalised on `IPC_DE` is (1, −0.352, −0.127), a
balanced vector, and forcing `drvec`'s normalisation on `IPC_ES` divides by
0.127 and inflates it. That is the `Y₁`-block choice this register lists as
open and as the user's responsibility.

`drvec`'s adjustment vector at the imposed `r = 1`, and weak exogeneity:

| | `Λ` (s.e.) | `t` | `-weakex` LR | `p` |
|---|---|---|---|---|
| IPC_ES | 0.0467 (0.0170) | 2.74 | 7.797 | **0.0052** — adjusts |
| IPC_DE | 0.0198 (0.0165) | 1.20 | 1.592 | 0.207 — weakly exogenous |
| IPC_FR | 0.0057 (0.0096) | 0.59 | 0.313 | 0.576 — weakly exogenous |

If a relation existed, only Spain would adjust to it and the core would drive
it, which is the expected direction for a small open economy.

### In sample: the error-correction term gains almost nothing

Diagonal short-run dynamics on both sides, so the only difference is the
error-correction term:

| | var(∇Ỹ) | `σ²` at `r=0` | `σ²` at `r=2` | `R²` at `r=0` | `R²` at `r=2` | gain |
|---|---|---|---|---|---|---|
| IPC_DE | 5.604e−6 | 5.541e−6 | 5.385e−6 | 0.0113 | 0.0392 | **+0.028** |
| IPC_FR | 3.685e−6 | 3.642e−6 | 3.518e−6 | 0.0117 | 0.0453 | **+0.034** |
| IPC_ES | 7.527e−6 | 6.240e−6 | 6.119e−6 | 0.1710 | 0.1871 | **+0.016** |

A 2–3 % reduction in residual variance per equation. The corresponding
`LR = 8.72` on 5 degrees of freedom is below `χ²(5)`'s 11.07 and far below the
bootstrap's critical value. **There is a gain, and it is not distinguishable
from noise.**

### Out of sample: imposing a rank the data reject costs

`-estwin 216 -f 12`, 35 origins from 12/2019, RMSE in percentage points,
Diebold–Mariano with HAC to `h−1` and the Harvey–Leybourne–Newbold correction —
the routine of the study this data comes from, applied to the per-origin errors
`-C` writes:

| cell | h=1 | h=3 | h=6 | h=12 |
|---|---|---|---|---|
| `r=2` diagonal | 1.048 (p .038) | 1.160 (p .005) | 1.239 (p .014) | 1.267 (p .003) |
| `r=2` diagonal, `Σ` free | 1.045 | 1.108 (p .012) | 1.169 (p .020) | 1.196 (p .005) |
| `r=2` free | 1.055 (p .047) | 1.160 (p .010) | 1.261 (p .012) | 1.286 (p .010) |
| `r=0` free (a VARMA) | 1.012 | 0.977 | 0.983 | 0.988 (p .050) |

Univariate RMSE: 0.62, 1.29, 2.12, 3.85 percentage points.

### What the four instruments say together

The rank test said `r = 0`; Johansen agreed under the matching specification;
the in-sample gain is 2–3 % of residual variance and insignificant; and the
out-of-sample evaluation, by an independent route, penalises imposing a rank by
5 % to 27 % of RMSE with `p < 0.05`. **The rank test was right, and the forecast
evaluation confirmed it.** That is the program working, not failing: the
instrument that decides the rank and the one that measures prediction agree, and
they were not built to agree.

Two limits on the reading. The evaluation period is **exceptional** — COVID and
the energy shock, with Spain decoupling — so this measures forecasting through a
break rather than the model class in general; and `r = 0` means *not rejected*,
not *shown to be absent*: Johansen's trace rejects at 10 % under case 3.

## 4v. A defect under §4t, and the table re-measured against it

*Found on 2026-08-22 by running `valgrind` over the paths P6 added. The
interesting part is not the defect — it is what re-measuring said about the
result that rested on it.*

### The defect

`rolling_eval` builds its own `struct Tvarma` on the stack and fills it with
`vec_shootx`, which sets every field **except `xitol`**: that tolerance is set by
each site that uses the structure, and there are a dozen sites that do it. This
one did not. `elf` was therefore called with whatever happened to be in that
word of the stack as the truncation tolerance of the `ξ` vector.

```
==12872== Conditional jump or move depends on uninitialised value(s)
==12872==    at 0x12C0C4: cxi (elfvarma.c:773)
==12872==    by 0x12D757: elf (elfvarma.c:264)
==12872==    by 0x12377B: rolling_eval (drvec.c:4422)
==12872==  Uninitialised value was created by a stack allocation
==12872==    at 0x122BB0: rolling_eval (drvec.c:4408)
```

Sixty such jumps in one run. Nothing else would have found it: the program did
not crash, did not warn, and did not produce a `nan`. It is `BUG-14`.

### Why it mattered more than a usual leak

`rolling_eval` **is** the out-of-sample route. Everything in §4t — the table that
decides that this program's version is 0.9 and not 1.0 — went through it.

### The re-measurement

The three columns of §4t, nine cases each, re-run with the fix in:

```sh
python3 tools/forecast_vs_univariate.py 2 1 4 0.75
python3 tools/forecast_vs_univariate.py 2 0 4 0.75
python3 tools/forecast_vs_univariate.py 2 1 4 0.75 -mafree
```

**All twenty-seven rows reproduce the published table digit for digit**, to the
three decimals it carries, and the win counts are unchanged: 2/9 at every horizon
for the default, 6/9 5/9 5/9 4/9 at `q = 0`, 5/9 5/9 6/9 5/9 for `-mafree`.

So the measured cost of the defect is **zero**: the value that happened to sit in
that stack word fell in the range where the truncation does not bite. That is not
the same as harmless — it was undefined behaviour, and a different compiler, a
different call depth or a different day could have moved it without a word.

The conclusion of §4t stands, and now it stands on defined behaviour.

### What actually closes it

The structure has a field that no filling function fills. While that is true,
every new site that uses it is another chance at the same bug. What closes it is
`vec_shootx` setting `xitol` itself — deliberately **not** done now, because
touching it would move every figure in this register through the back door. It
is written down for the refactor (P8), which is where a change with no measurable
effect belongs.

## 5. What is not in the register, and why

* **The Census Housing example** of the AddOn (Hillmer & Tiao 1979): the data
  have not been located. Without them that comparison cannot be attempted.
* **Finite-sample critical values.** Everything above uses asymptotic values. The
  paper's own recommendation for this class of model is a parametric bootstrap,
  which is not implemented.
* **Standard errors against a published table.** `drvec` reports them from the
  Hessian; they have not been compared with an external source.

---

*Re-measuring this register is `make test` plus the commands quoted above — the
four estimation runs, and the four modes of `tools/sim/ma_identification.py` for
§4q. If a row moves, either the program changed or the input did — and the second
is the more frequent cause, so the input should be checked first.*

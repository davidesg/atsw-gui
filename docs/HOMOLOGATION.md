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
| factorisation: joint `r=0` all-diagonal = sum of the univariate fits | **1e−9** | ✔ |
| `Σ = P D P′` reconstructs `Σ` (M=3) | worst entry **< 1e−5** | ✔ |
| a `Θ = 0` seed reproduces the cold start | **bit for bit** | ✔ |
| the two layouts of the same model agree on \|Σ̂\| | within **0.00005** | ✔ |
| `-weakex i` equals the equivalent `-alpha` file | **exactly** | ✔ |

```sh
make test          # all of the above, plus 56 more checks
```

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

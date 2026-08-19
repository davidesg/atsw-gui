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

## 3b. A limitation that bounded the specification searches

Until 2026-08-19 no model with `q ≥ 2` could be estimated: an allocation defect
in the engine's supporting library corrupted the heap and aborted the run
whenever the likelihood routine requested its cross-covariance array with a
negative lower bound, which it does for two or more moving-average lags. The
defect was not particular to `drvec` and was not new — it had been diagnosed and
fixed elsewhere in the suite on 2026-06-15, and `drvec` was carrying the pre-fix
copy ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5).

The consequence for the register is limited but must be stated. Every
specification search reported above — including the comparison against Johansen
at each procedure's own optimum — ranged over `q ≤ 1`, and did so because of a
defect rather than a modelling decision. The comparisons remain valid as
comparisons, since the criterion was applied identically on both sides and
Johansen's procedure admits no moving-average term at all; what cannot be
claimed from them is that `q ≤ 1` was selected against the `q = 2` alternative.

On mink–muskrat the alternative is now measurable, and it does not settle the
question either way:

| `p = 2, r = 1`, case 2 | log-likelihood | `npar` | AIC | BIC |
|---|---|---|---|---|
| `q = 1` | 6.4786 | 14 | 0.2466 | **0.7311** |
| `q = 2` | 11.8573 | 18 | **0.2014** | 0.8243 |

The Akaike criterion prefers the larger model and the Schwarz criterion the
smaller, so the two disagree and the question stays open. The
moving-average operator remains on the invertibility boundary — two of its four
roots at 0.99995 — so the enlargement does not relieve the condition that §3
identifies.

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

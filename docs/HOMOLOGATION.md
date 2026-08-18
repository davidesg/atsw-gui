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
| the transformation against the AddOn's closed form for M=2, r=1 | difference **0.000e+00**, term by term | ✔ |
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
`urca` 1.3.4 / Osterwald-Lenum (1992). MA terms do not affect the asymptotic
distribution (Yap and Reinsel 1995, Thm. 3, cited in Mauricio's Remark 5). **Case
3 is not tabulated here**, so a case-3 rank test reports the statistic without
critical values.

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

Over **20 replications** of a process with a true rank of 1, at n = 120, with the
sequential test at the 5 % asymptotic level:

| rank selected | replications |
|---|---|
| r = 0 | 1 (5 %) |
| **r = 1 — correct** | **16 (80 %)** |
| r = 2 | 3 (15 %) |

So the test recovers the truth four times in five, and **over-rejects about
three times as often as its nominal level**. `badnorm.inp` is one of those three,
which is why it appears above as a failure: it is a draw, not a defect. Checked
against lag order too — the over-rejection there is the same at `p = 1, 2, 3`
(statistics 18.61, 20.27, 22.39), so it is not a lag-choice artefact.

**This is the measurement that justifies F4.** Asymptotic critical values are not
good enough at these sample sizes, and the paper's own recommendation for this
model class is a parametric bootstrap. Until that exists, read a rank decision
that sits near a critical value as undecided.

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

The single-start row is what the program gives by default and is the honest
figure for a casual run. The multi-start row is what it gives when asked to look
properly, and it is the one to quote: the spread across four mathematically
equivalent set-ups collapses to 0.000014 and the level lands within 1.6 % of the
global-search reference.

Not closed, and the residue is stated: 0.002346 against 0.002311 with the
invertibility gate, or 0.002294 without it. Both `drvec` and the reference stop
*at* that gate — measured, `max|λ(Θ₁)| = 1.000050` in both — so what separates
them is which point of the boundary is reached, not the boundary itself.

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
the one that has caught people out here, so check the input first.*

# Benchmark — cointegration reference cases

Reference datasets and **published** results for validating `drvec` (VEC model
EML, Mauricio 2006). Every case below cites the article/book that reports the
result, and states the reported value (cointegration rank, and where published,
the cointegrating vector and adjustment matrix).

Full provenance (variables, sample, SHA-256 hashes) is in
`datasets/MANIFEST.md`. The per-case trace/eigen statistics, cointegrating
vectors (β) and loading matrices (α) are in `benchmark/<case>.results.txt`.
Bibliographic references with verified DOIs are in `benchmark/REFERENCES.md`.

Three tiers:

- **Verified** — the result is quoted directly from the paper/book, and the data
  is present in `datasets/`. *(No case currently qualifies: the two Mauricio
  cases were downgraded on 2026-08-16, see the next section.)*
- **Reproduced** — the result is recomputed here with the R package `urca`
  (`ca.jo`, the standard Johansen reference implementation), matching the cited
  paper's specification. The data is present in `datasets/`.
- **Downgraded / pending** — the published result is on record but cannot be
  used as a numerical target, either because the data is missing or because the
  reported value could not be reproduced. **Never tune `drvec` against these.**

A case is useful for `drvec` validation only when the reported result is
explicitly documented; therefore cases whose published rank is not yet
confirmed are listed under "pending verification" rather than asserted.

## Mauricio (2006) — the paper this project replicates

**⚠ Neither case is usable as a numerical homologation target today.**

| Case | Data | Published result | Status |
|------|------|------------------|--------|
| `mauricio/mink_muskrat` | Hudson's Bay mink & muskrat fur sales, annual 1850–1911 (62 obs) | **r = 1**. VARMA(2,1) on logs. Eigenvalues of Π̂ = I − Φ̂₁ − Φ̂₂ = **0.0413, 1.0602** (single unit root). Log-likelihood **15.6116**. ĉ = [10.7976, 13.0080]. | **Log-likelihood not reproducible** — see below |
| `mauricio/census_housing` | US single-family housing starts & houses sold, monthly Jan 1965 – May 1975 (125 raw obs; **N = 112 effective** after (1−L¹²) and ∇) | **r = 1**. VEC(1) with Θ₁ seasonal, model (A.14). Eigenvalues of Π̂ = **0, 0.7212**. B̂ = [1, −1.8625], Λ̂ = [0.5191, −0.1085], Θ̂₁ = diag(0.9600, 1.1318), Σ̂ = [29.4526; 5.7112, 9.9830]. Log-likelihood **−664.2429**, AIC/BIC 12.0043/12.1985. | **Data not located** |

Quoted from Mauricio (2006), Tables 2 and A3 (in `literature/`).

### Why `mink_muskrat` was downgraded (2026-08-16)

The published log-likelihoods of Tables 2 (15.6116) and 4 (15.1257) **could not
be reproduced on the canonical data**, and the discrepancy was traced *away
from* `drvec`. Full evidence in `docs/ANALISIS_PRELIMINAR.md` §5; in short:

- The **data is confirmed correct.** Byte-identical to the Hipel & McLeod
  `mhsets` collection (StatLib) for both series; `fma::mink` agrees too.
- The **likelihood code is confirmed correct.** Mauricio's AS 311 `elf` matches
  a brute-force multivariate-normal density to 2·10⁻⁸ (white noise and VAR(1)),
  and `drvmlest.c`'s concentrated formula checks out algebraically.
- **The published points are non-invertible as printed.** Θ̂₁ has eigenvalues
  0.8461/**−1.0063** (Table 2) and 0.9904/**−1.0078** (Table 4); the engine
  rejects both with `ifault = 4`.
- **The gap is entirely in Σ̂.** At the paper's own (μ̂, Φ̂, Θ̂) the exact AS 311
  residual covariance has |Σ| = 0.002414 against the |Σ̂| = 0.001788 published
  beside them (ratio 1.35, i.e. −½·n·log 1.35 = −9.16 of an 11.7 gap). The
  paper's **CML** column (|Σ̂| = 0.002312) *is* consistent with this data; the
  **EML** column is not. For scale, |Σ̂| = 0.001788 is what an unrestricted
  **VAR(7)** achieves on these series by OLS.
- A 40-start global search of the exact likelihood tops out at **8.1440**
  (n = 62) / **7.1701** (n = 61), not 15.61 — and its |Σ| = 0.002311 lands
  essentially **on the paper's own CML value** (0.002312).

**Independent calibration — Chan & Wallis (1978)**, the classic study of this
data (`literature/Chan-MultipleTimeSeries-1978.pdf`), estimates their eq. (13)
on **(Δ log muskrat, log mink)** — the very vector our `.inp` carries — with an
order-4 AR and *exact* ML for the vector MA (Osborn 1977), and report the
generalized variance |Σ̂| explicitly:

| source | model | \|Σ̂\| |
|---|---|---|
| Jenkins (1975), diagonal form (12) | AR(4)+MA(1) diag. | 0.00299 |
| Chan & Wallis (1978) eq. (13) | AR(4)+MA(1), exact ML | **0.00246** |
| Mauricio (2006) Table 2, **CML** column | VARMA(2,1) | 0.002312 |
| **global exact-ML optimum measured here** (n=61) | VARMA(2,1) | **0.002311** |
| Mauricio (2006) Table 2, **EML** column | VARMA(2,1) | **0.001788** |

A *richer* published model (AR order 4) reaches only 0.00246; our VARMA(2,1)
reaches 0.002311, slightly better and entirely plausible. The paper's EML column
sits **27 % below** the order-4 published fit. Our side is the one in line with
the literature.

### What this case can and cannot be used for

- ✔ **The rank result (r = 1) is a valid quoted reference — and `drvec` now
  reproduces it.** With `p=2 q=1 -case 2 -lrtest` (which since 2026-08-17
  includes the r = 0 null): LR(0→1) = 25.5981 against critical values
  13.75 / 15.67 / 20.20 (M−r = 2, case 2) → reject H₀ at 1%, so **r = 1**.
  AIC and BIC agree. See `docs/ANALISIS_PRELIMINAR.md` §4.4.
- ✔ **Usable acceptance target — on |Σ̂|, not on the log-likelihood.** On this
  data a VARMA(2,1) should reach **|Σ̂| ≈ 0.0023**, and never below ~0.0022.
  Backed by three independent sources (Chan & Wallis 0.00246 at AR order 4, the
  paper's own CML column 0.002312, the global search 0.002311).
  **`drvec` is close but not yet reliably inside.** Removing the redundant
  covariance-scale parameter (`docs/ANALISIS_PRELIMINAR.md` §3.6) moved it from
  0.002496 to between 0.00237 and 0.00257 depending on the run:

  | configuration | \|Σ̂\| | |
  |---|---|---|
  | levels (default), case 2 | 0.002461 | just outside |
  | levels (default), case 3 | 0.002569 | outside |
  | legacy layout, case 2 | 0.002373 | inside |
  | legacy layout, case 3 | 0.002483 | just outside |

  The spread across mathematically equivalent set-ups is itself the diagnosis:
  the optimizer still stops on termcode 3 at slightly different points. Closing
  that is the open item (§8).

  *Why |Σ̂| and not logL:* `drvec` evaluates the likelihood of **Ȳ_t**
  (61 obs, 1851–1911) while model (22) and the global search evaluate it on
  **Y_t**. The map Y → Ȳ has unit Jacobian but **conditions on Y₁₈₅₀**, so the
  two levels are not directly comparable. |Σ̂| is the same innovation covariance
  in both representations. As a within-representation reference only: exact ML
  of model (22) on Y_t tops out at 7.1701 (n = 61) / 8.1440 (n = 62).
- ✘ **Not usable as targets:** the published log-likelihoods (15.6116 / 15.1257)
  and the Λ̂, B̂, Σ̂ values that go with them. **Do not tune `drvec` against them.**

### Caveat on the Π̂ eigenvalues quoted above

Mauricio reports the eigenvalues of Π̂ as evidence of the number of unit roots
(0.0413/1.0602 in Table 2; 0/0.7212 in Table A3). **Read them as an indication,
not as a rank criterion.** Mélard, Roy and Saidi (2004, §3) point out that the
assumption rank[Φ(1)] = k−d does *not* imply that ΣΦⱼ = I + C has d unit
eigenvalues, and that there are counterexamples where the procedure fails (Pham,
Roy and Cédras, 2003). Since Π = Φ(1), "Π̂ has a single zero eigenvalue" is
exactly the criterion they warn about. The likelihood-ratio test (`-lrtest`) is
the instrument; the eigenvalues are a sanity check.

### Provenance caveats

- Chan & Wallis state that Jones (1914) covers **1848–1909**; Mauricio states
  1850–1911, and the data here is 1850–1911 (while `fma::mink` is 1848–1911).
  The declared window is not consistent across the literature.
- Chan & Wallis use **y₁ = muskrat, y₂ = mink** — the reverse of Mauricio's
  labelling. Easy trap when comparing matrices.

## Reproduced cases (ca.jo, matching the cited specification)

### Johansen & Juselius (1990) — Oxford Bulletin of Economics and Statistics 52(2), 169–210

| Case | Series | n obs | Result |
|------|--------|-------|--------|
| `urca_denmark` | Danish money demand: LRM, LRY, LPY, IBO, IDE | 55 | r = 2 at 5% (trace). |
| `urca_finland` | Finnish money demand: lrm1, lny, lnmr, difp | 106 | r = 2 at 5% (trace). |

### Johansen & Juselius (1992) — Journal of Econometrics 53, 211–244

| Case | Series | n obs | Result |
|------|--------|-------|--------|
| `urca_UKpppuip` | p1, p2, e12, i1, i2, dpoil0, dpoil1 | 62 | r = 2 (paper). ca.jo fails here because of the two I(0) dummies; see note. |

### Hylleberg, Engle, Granger & Yoo (HEGY) — UK consumption–income

| Case | Series | n obs | Result |
|------|--------|-------|--------|
| `urca_UKconinc` | conl, incl (logs) | 120 | bivariate M=2, r=1. |

### Pokorny (1987) — An Introduction to Econometrics, p. 408

| Case | Series | n obs | Result |
|------|--------|-------|--------|
| `urca_UKconsumption` | cons, inc, price | 76 | r = 2 at 5% (trace). |

**✔ Reproduced by `drvec` (2026-08-17).** With the three series in logs and
levels, `drvec p=2 q=0 -case 2 -lrtest` estimates r = 0, 1 and 2 — **all three
converge properly** — and returns

```
  r  M-r      LR     10%     5%      1%
  0    3   70.1280  19.77  22.00  26.81   reject H0 at 1%
  1    2   25.5141  13.75  15.67  20.20   reject H0 at 1%   ->  r = 2
```

matching the `ca.jo` rank, with AIC and BIC agreeing. See
`docs/ANALISIS_PRELIMINAR.md` §4.4.

*(The same test on `urca_denmark` — M = 5, n = 54, 49–67 parameters — does not
converge for the lower ranks and its rank should not be trusted. The model is
too rich for that sample; `drvec` prints the optimizer status per rank so this
is visible.)*

### Lütkepohl (2005) — "New Introduction to Multiple Time Series Analysis", Springer

The `e1` (West German investment–income–consumption) and `e3` (US money)
systems are the cointegration examples of Chapter 7. Their published rank is
**r = 1**; the trace statistics below are reproduced with `ca.jo` (K = 2,
`ecdet = "const"`). The remaining `e2`, `e4`, `e5`, `e6` are distributed with
the book but their published rank is not asserted here; they are retained as
data.

| Case | Series | n obs | Result |
|------|--------|-------|--------|
| `lutkepohl/e1.dat` | West German invest, income, cons (logs), 1960Q1–1982Q4 | 92 | **r = 1** (book Ch.7). Trace = 75.00, 15.45, 6.08. |
| `lutkepohl/e3.dat` | US M1, GNP, rd, rb, 1954Q1–1987Q4 | 136 | **r = 1** (book Ch.7). |
| `lutkepohl/e2.dat` | US investment & inventories, 1947Q1–1972Q4 | 104 | data only. |
| `lutkepohl/e4.dat` | W. German income & consumption, 1960Q1–1987Q4 | 112 | data only. |
| `lutkepohl/e5.dat` | W. German short/long interest, monthly 1960–1987 | 336 | data only. |
| `lutkepohl/e6.dat` | German Dp, R, 1972Q2–1998Q4 | 107 | data only. |

### Lütkepohl (2005) / Pfaff (2008) — "Analysis of Integrated and Cointegrated Time Series with R"

| Case | Series | n obs | Result |
|------|--------|-------|--------|
| `vars_Canada` | prod, e, U, rw (quarterly 1980–2000) | 84 | r = 1 (trace = 84.92, 36.42, 18.72, 3.85 with ecdet="trend", K=3). |

### Rao (1994) — "Cointegration for the Applied Economist", ed. B. Bhaskara Rao

The Raotbl datasets are the worked examples of the Rao volume (Data Appendix
Tables D.1–D.6). The ranks below are reproduced with `ca.jo` (K = 2,
`ecdet = "const"`, 5%) and are provided as an independent reference; the exact
reported rank of each table should be re-confirmed against the printed chapter
before use as a hard target.

| Case | Chapter author | n obs | vars | Result (ca.jo, 5%) |
|------|----------------|-------|------|--------|
| `urca_Raotbl1` | Dickey, Jansen & Thornton (1994) | 144 | 5 | r = 2 |
| `urca_Raotbl2` | Dickey, Jansen & Thornton (1994) | 144 | 4 | r = 3 |
| `urca_Raotbl3` | Holden & Perman (1994) | 99 | 6 | r = 3 |
| `urca_Raotbl4` | Perron (1994) | 117 | 6 | r = 2 |
| `urca_Raotbl5` | Perron (1994) | 117 | 5 | r = 2 |
| `urca_Raotbl6` | Mehra (1994) | 123 | 8 | r = 5 |
| `urca_Raotbl7` | Otto (1994) | 132 | 4 | r = 2 |

## Unit-root cases (not cointegration rank references)

### Nelson & Plosser (1982) — Journal of Monetary Economics 10, 139–162

| Case | Series | n obs |
|------|--------|-------|
| `urca_nporg` | 14 US macro series (levels) | 111 |
| `urca_npext` | 14 US macro series (logs, extended to 1988) | 129 |

These are the canonical unit-root test datasets, not cointegration cases; they
are retained for unit-root testing only.

## Pending verification

- `urca_ecb` (26 obs, 4 vars) — ECB data without a single canonical published
  rank; retained in `datasets/` but not asserted as a benchmark case.
- Census Housing raw data — not yet located (source sites offline).
- `mauricio/mink_muskrat` — data confirmed correct, but the published EML
  log-likelihood and Σ̂ are not reproducible; see the Mauricio section above and
  `docs/ANALISIS_PRELIMINAR.md` §5.

## How the reproduced values were computed

```r
library(urca)
ca.jo(data, type = "trace", K = 2, ecdet = "const", spec = "longrun")
ca.jo(data, type = "eigen", K = 2, ecdet = "const", spec = "longrun")
```

`K = 2` (one lag in the differenced VAR) is the default used across these
papers. `ecdet = "const"` restricts the constant to the cointegrating relation.
Software: R 4.3.3, `urca` 1.3.4. The full trace/eigen statistics, cointegrating
vectors and loading matrices are stored in `benchmark/<case>.results.txt`.

## Caveats

- `urca_nporg` and `urca_npext` have 15 variables; the default `ca.jo` critical
  values only tabulate up to 12, and the reduced-rank regression is near
  singular, so no clean rank is reported. They are kept as unit-root data only.
- `urca_UKpppuip` includes two I(0) deterministic dummies, which makes the
  default `ca.jo` fit singular; the published result (r = 2) is from the paper.
- For `drvec` (Mauricio's transformation) a case maps cleanly when **M = 2,
  r = 1** (bivariate, one cointegrating relation). The multi-variable cases
  (e1, e3, denmark, …) exercise the general-B₂ (s×r) path.

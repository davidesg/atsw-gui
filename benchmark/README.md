# Benchmark — cointegration reference cases

Reference datasets and **published** results for validating `drvec` (VEC model
EML, Mauricio 2006). Every case below cites the article/book that reports the
result, and states the reported value (cointegration rank, and where published,
the cointegrating vector and adjustment matrix).

Full provenance (variables, sample, SHA-256 hashes) is in
`datasets/MANIFEST.md`. The per-case trace/eigen statistics, cointegrating
vectors (β) and loading matrices (α) are in `benchmark/<case>.results.txt`.
Bibliographic references with verified DOIs are in `benchmark/REFERENCES.md`.

Two tiers:

- **Verified** — the result is quoted directly from the paper/book, and the data
  is present in `datasets/`.
- **Reproduced** — the result is recomputed here with the R package `urca`
  (`ca.jo`, the standard Johansen reference implementation), matching the cited
  paper's specification. The data is present in `datasets/`.

A case is useful for `drvec` validation only when the reported result is
explicitly documented; therefore cases whose published rank is not yet
confirmed are listed under "pending verification" rather than asserted.

## Verified cases (result quoted from the paper)

### Mauricio (2006) — the paper this project replicates

| Case | Data | Published result |
|------|------|------------------|
| `mauricio/mink_muskrat` | Hudson's Bay mink & muskrat fur sales, annual 1850–1911 (62 obs) | **r = 1**. VARMA(2,1) on logs. Eigenvalues of Π̂ = I − Φ̂₁ − Φ̂₂ = **0.0413, 1.0602** (single unit root). Log-likelihood **15.6116**. ĉ = [10.7976, 13.0080]. |
| `mauricio/census_housing` | US housing starts & houses sold, monthly 1965–1975 (seasonally differenced, 125 obs) | **r = 1**. VEC(1) with Θ₁ seasonal. Eigenvalues of Π̂ = **0, 0.7212**. B̂ = [1, −1.8625], Λ̂ = [0.5191, −0.1085]. Log-likelihood **−664.2429**. *Data not yet located.* |

Quoted from Mauricio (2006), Tables 2 and A3 (in `literature/`).

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

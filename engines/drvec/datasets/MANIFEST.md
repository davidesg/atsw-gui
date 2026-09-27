# Dataset manifest — provenance and integrity

Every dataset in `datasets/` is listed here with:

- the **exact bibliographic source** it was taken from (paper/book + table);
- its **variables, frequency and sample period**;
- a **SHA-256 hash** so any copy can be verified byte-for-byte.

Reference software used to extract the R-package datasets and to compute the
benchmark statistics:

- R 4.3.3 (2024-02-29)
- `urca` 1.3.4
- `vars` 1.6.1
- `tseries` 0.10.58

The `urca` datasets were exported with the bundled `tools/extract_benchmarks.R`
script, which calls `write.csv` on the `data.frame` returned by `data(<name>)`.
The Lütkepohl files are byte-copies of the JMulTi book datasets. The gretl files
are byte-copies from `/usr/share/gretl/data/misc/`.

---

## 1. Mauricio (2006) — the paper this project replicates

### `mauricio/mink_muskrat.csv`
- **Source**: Jones, J.W. (1914), "Fur-farming in Canada", Commission of
  Conservation, Canada, pp. 209–214; as used by Reinsel (1997) and by
  Mauricio, J.A. (2006), "Exact maximum likelihood estimation of partially
  nonstationary vector ARMA models", CSDA 50, 3644–3662, Section 4.
- **Retrieved from**: Hyndman's Time Series Data Library
  (`robjhyndman.com/tsdldata/ecology1/mink.dat` and `muskrat.dat`).
- **Variables**: `mink`, `muskrat` (annual fur sales, Hudson's Bay Co.).
- **Sample**: annual, 1850–1911, 62 observations.
- **SHA-256**: `d7fe4eb3d2c107cd5cb52045998d9bc4cf09540c1569db291e9e8bfcbbdc5285`

### `mauricio/mink_muskrat.inp`
- **Derived from**: `mauricio/mink_muskrat.csv` (same provenance), taking
  natural logs of both series. No other transformation.
- **Layout**: drvec's default (levels). Column order is [Y₂ ; Y₁]: col 1 =
  `log muskrat` (Y₂, nonstationary — drvec forms ∇Y₂ internally), col 2 =
  `log mink` (Y₁, the cointegrating block, kept in levels). M = 2, r = 1.
- **Regenerated 2026-08-17.** It previously held ∇`log muskrat` pre-differenced
  (61 rows) and its series-name line was in the reverse order to its columns.
  Without the true levels of Y₂ drvec has to cumulate them from an arbitrary
  origin, which breaks `-case 1` and makes `Ê[W]` incomparable with the
  published value; see `docs/ANALISIS_PRELIMINAR.md` §4.3.
- **Sample**: annual, 1850–1911, 62 observations (61 after differencing).
- **SHA-256**: `15438b4dc29a1ff73c1a0b191bd36231efd31f4542e6d511371f11ae176bde17`

### `mauricio/census_housing` (data NOT yet located)
- **Source**: Mauricio (2006), AddOn A2; the data are the US Census Housing
  data (housing starts, houses sold) used by Reinsel (1997) Ex. 6.4/6.6 and
  Mélard et al. (2004).
- **Status**: the redistribution sites (Tsay MTS book, Mélard's ULB page) are
  offline; the R packages that used to bundle it no longer do. Not yet located.

---

## 2. Johansen & Juselius (1990) — OBES 52(2), 169–210

"Maximum likelihood estimation and inference on cointegration — with
applications to the demand for money."

### `urca_denmark.csv`
- **Variables**: `LRM` (log real money M2), `LRY` (log real income), `LPY`
  (log price deflator), `IBO` (bond rate), `IDE` (bank deposit rate); plus
  `ENTRY` (period label).
- **Sample**: quarterly 1974:Q1–1987:Q3, 55 observations.
- **SHA-256**: `8af2d2844aff4fc1719eea651b44292fe0abec24c17c0ff42579a03289bb99c0`

### `urca_finland.csv`
- **Variables**: `lrm1` (log real money M1), `lny` (log real income), `lnmr`
  (marginal rate of interest), `difp` (inflation rate).
- **Sample**: quarterly 1958:Q2–1984:Q3, 106 observations.
- **SHA-256**: `f34c5a8354518fdc73a737279cca4a8c540c5362e2c2b12c51d0c0bc2a2d2fea`

---

## 3. Johansen & Juselius (1992) — Journal of Econometrics 53, 211–244

"Testing structural hypotheses in a multivariate cointegration analysis of the
PPP and the UIP for UK."

### `urca_UKpppuip.csv`
- **Variables**: `p1` (UK wholesale price index), `p2` (trade-weighted foreign
  wholesale price index), `e12` (UK effective exchange rate), `i1` (3-month
  treasury bill rate), `i2` (3-month Eurodollar rate), `dpoil0` (world oil
  price at t), `dpoil1` (world oil price at t−1). All in logs.
- **Sample**: quarterly 1971:Q1–1987:Q2, 62 observations.
- **SHA-256**: `03a2a3d2147818f977ea242c2f36dff9187ae5237fac33d5126893d8eb3b4ad5`

---

## 4. Nelson & Plosser (1982) — Journal of Monetary Economics 10, 139–162

"Trends and Random Walks in Macroeconomic Time Series."

### `urca_nporg.csv`
- **Variables** (14): `year`, `gnp.r`, `gnp.n`, `gnp.pc`, `ip`, `emp`, `ur`,
  `gnp.p`, `cpi`, `wg.n`, `wg.r`, `M`, `vel`, `bnd`, `sp`.
- **Sample**: annual, 1860–1970 (series start/end as in the original table).
- **SHA-256**: `1eef35bf0a5238561bcc1c560325e7c4111472f5d6709c6dc205b2be2078fab7`

### `urca_npext.csv` (extended, from Schotman & van Dijk 1991, JAE 6, 387–401)
- **Variables** (14): `year`, `realgnp`, `nomgnp`, `gnpperca`, `indprod`,
  `employmt`, `unemploy`, `gnpdefl`, `cpi`, `wages`, `realwag`, `M`,
  `velocity`, `interest`, `sp500` (all logs except the bond yield).
- **Sample**: annual, 1860–1988.
- **SHA-256**: `5487f7f3fa5b8be9a13f31cf0e40b04ddd531c493849e47b07a94958cf1ec13f`

Note: these two are **unit-root** (not cointegration) datasets; they are kept
for the unit-root test benchmark and are not rank-reference cases.

---

## 5. Hendry & Doornik / UK data (urca package)

### `urca_UKconinc.csv`
- **Variables**: `conl` (log total real consumption), `incl` (log real
  disposable income).
- **Sample**: quarterly, ending 1984:Q4, 120 observations.
- **SHA-256**: `d08e22d4a217e2545e497ed5a72653fcc4251de8f7e8cd8da90b570978911db3`

### `urca_UKconsumption.csv`
- **Variables**: `cons` (non-durable expenditure), `inc` (personal disposable
  income), `price` (consumers' expenditure deflator, 1970=100).
- **Sample**: quarterly 1957:Q1–1975:Q4, 76 observations.
- **Source**: Pokorny, M. (1987), *An Introduction to Econometrics*, p. 408.
- **SHA-256**: `4daf62d8aaf9e4cca689ecb3a74aca79fe34b3c26c89bf6d87d9db382b36df3f`

---

## 6. Rao (1994) — "Cointegration for the Applied Economist"

Datasets from the Data Appendix (Tables D.1–D.6) of the Rao volume. Each entry
cites the chapter author who used the data.

| File | Chapter author | Table | Variables | Sample | SHA-256 |
|------|----------------|-------|-----------|--------|---------|
| `urca_Raotbl1.csv` | Dickey, Jansen & Thornton (1994) | D.1 | k, ksa, r3m, r10y, rgnp (logs) | 1953:1–1988:4 | `05519b627bda3fc0902721f16797aa2a5278e598d1d5e1a77fc442c0574a6d53` |
| `urca_Raotbl2.csv` | Dickey, Jansen & Thornton (1994) | D.2 | m1p, m2p, mbp, nm1m2p | 1953:1–1988:4 | `f7243c0313bf49ed13772cc08972a78197058c16d64028bba02e15ebfa08ac44` |
| `urca_Raotbl3.csv` | Holden & Perman (1994) | D.3 | lc, li, lw + dummies dd682/dd792/dd883 | 1966:4–1991:2 | `872da9102d14c2f98e54fcde78d1a7c277abb0732722dc5f620500afc9fb3eac` |
| `urca_Raotbl4.csv` | Perron (1994) | D.4 | aus, can, den, fin, fra, ger | 1870–1986 | `8929f5f837067e48e58abd1d6eee49ec10d5f9e2b2b72203ecc0ec6d45672f67` |
| `urca_Raotbl5.csv` | Perron (1994) | D.5 | ita, nor, swe, ukg, usa | 1870–1986 | `21be4a577d54adf9d2179a08f99fb212aeaadf77a673d01f8936a573f6d9272e` |
| `urca_Raotbl6.csv` | Mehra (1994) | D.6 | rgnp, pgnp, ulc, gdfco, gdf, gdfim, gdfcf, gdfce | 1959:1–1989:3 | `0bc6be5fd19338d63611679d678c59731369db7c3e0f55835a7e6a3937d2bccd` |
| `urca_Raotbl7.csv` | Otto (1994) | D.6 | m1, p, gdp, r | 1956:1–1988:4 | `693a8a3deb70f520aa96d78342858c0fdd663ab22d25da107ca332132818c3ba` |

---

## 7. Lütkepohl (2005) — "New Introduction to Multiple Time Series Analysis"

Files downloaded from the JMulTi book-data page
(`jmulti.de/data_imtsa.html`); the descriptions below are quoted from that page.

| File | Description | Sample | SHA-256 |
|------|-------------|--------|---------|
| `lutkepohl/e1.dat` | West German fixed investment, disposable income, consumption (bn DM) | 1960Q1–1982Q4 | `c4fdef149249cea596657c42713ce2f41df160d23c29ed1b9b59e2ff10fc0d97` |
| `lutkepohl/e2.dat` | US fixed investment, change in business inventories | 1947Q1–1972Q4 | `6229c8ed16cd35825d4795b1b6ba65b897d5a8932b5e1cf242e94bf0ca6569c8` |
| `lutkepohl/e3.dat` | US real M1, real GNP, rd, rb | 1954Q1–1987Q4 | `888585cbd316245b61ec4905dcf9066279f31ed8b83e83732b7506be824704a1` |
| `lutkepohl/e4.dat` | W. German real per-capita income, consumption | 1960Q1–1987Q4 | `3ccefd9ab3fbe36803ea422a80def89541d98e4044fb3a1fc4b483119a0ebed2` |
| `lutkepohl/e5.dat` | W. German short & long interest rates (monthly) | 1960M1–1987M12 | `13f2aef8e4297519fe6077e1269b60b82e416641b1484000c02b9fdccb89ff39` |
| `lutkepohl/e6.dat` | German Dp (Δlog gdp deflator), R (nominal long rate) | 1972Q2–1998Q4 | `f5c5256b68c002359a8de596f65be0992e0fefe6b117ab6b229473a1a0b7cd55` |

---

## 8. Lütkepohl (2005) / Pfaff (2008) — `vars` package

### `vars_Canada.csv`
- **Variables**: `e` (employment), `prod` (productivity), `rw` (real wage),
  `U` (unemployment).
- **Sample**: quarterly 1980:Q1–2000:Q4, 84 observations.
- **Source**: the `Canada` dataset of R package `vars`; the VECM example of
  Lütkepohl (2005) Ch.7 and Pfaff (2008) *Analysis of Integrated and
  Cointegrated Time Series with R*, Springer.
- **SHA-256**: `7fa03363a8feed2188ffd08dfae6e4f207d5e35b98a1d94647d890b11f8cee94`

---

## 9. Gretl bundled datasets (byte-copies of local install)

| File | Dataset | SHA-256 |
|------|---------|---------|
| `gretl/denmark.gdt` | Danish money demand (LRM, LRY, IBO, IDE; 55 obs) — Johansen (1995) Ch.7 | `69ebffb580702fc5cc56961b84577a3207181a7a956440a80c03218a7ec931d4` |
| `gretl/ukppp.gdt` | UK PPP | `6276c1f836ed5d21604da50e88987492a4424e4397448227fbcce886a1eb0138` |
| `gretl/hendry_jae.gdt` | Hendry JAE consumption–income | `656dd115c10f30e0fce4122050727870ad8c270168b9428323281e6f6da7797c` |

---

## 10. ECB (urca package)

### `urca_ecb.csv`
- **Source**: European Central Bank (`www.ecb.europa.eu`), bundled in `urca`.
- **Status**: no single canonical published rank; retained as data only.
- **SHA-256**: `d7f04f283d20fe943618f761401f5d40cbeb32ac1f515f255bfd9086ad2f3223`

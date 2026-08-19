# `drvec` against Johansen

*An external check on the estimator, and a measurement of what the MA component
buys. Run with `tools/compare_johansen.py`; the data are eight annual wheat-price
pairs (European markets against London, 1700–1813, n = 90–113), which are real,
short and awkward — the conditions this program is meant for.*

Johansen's reduced-rank regression is the reference implementation of
cointegration, and `statsmodels`' version shares no ancestry with this code. So
it answers two different questions, and keeping them apart is the whole point:

1. **On the same specification, do they agree?** That is a check on `drvec`.
2. **At each one's own optimum, do they say the same thing?** That is a
   measurement of the model class, not of the code.

---

## 1. The same specification: they agree to 0.017

With `q = 0` the two fit **the same model**, by routes with nothing in common — a
closed-form eigenvalue problem against numerical optimisation of the exact
likelihood.

The order correspondence is **`p = k + 1`**: `drvec` carries `F₁…F_{p−1}` on `∇Y`,
i.e. `p−1` lags, and Johansen carries `k`. It is swept rather than checked at one
point, because a wrong correspondence would agree at one order by coincidence and
nowhere else.

Absolute difference in the cointegrating coefficient, renormalised on the same
variable:

| pair | k=1 (p=2) | k=2 (p=3) | k=3 (p=4) | mean |
|---|---|---|---|---|
| Milan | 0.0284 | 0.0130 | 0.0165 | 0.019 |
| Strasbourg | 0.0166 | 0.0250 | 0.0270 | 0.023 |
| Utrecht | 0.0077 | 0.0063 | 0.0062 | 0.007 |
| Vienna | 0.0236 | 0.0008 | 0.0061 | 0.010 |
| Aix | 0.0519 | 0.0340 | 0.0344 | 0.040 |
| Arévalo | 0.0016 | 0.0063 | 0.0234 | 0.010 |
| Angers | 0.0239 | 0.0378 | 0.0045 | 0.022 |
| Penn | 0.0003 | 0.0018 | 0.0030 | 0.002 |

**24 comparisons, every difference between 0.0003 and 0.052.** Two independent
implementations agreeing to that on real short samples is the strongest external
check `drvec` has, and it is on the quantity the program exists to produce. It
also confirms the `p = k+1` convention empirically.

---

## 2. At each one's own optimum: they diverge, and the reason is structural

Johansen at the order its own AIC selects; `drvec` at `q = 0` and at `q = 1`.
Residual diagnosis on both sides — which is the **only** comparable statistic,
because AIC and BIC are not: Johansen's likelihood is conditional and `drvec`'s is
exact and unconditional, so their information criteria measure different things.

| pair | Johansen AIC: k, rank, B̂₂, portm. | `drvec` q=0: B̂₂, portm. | `drvec` q=1: B̂₂, portm. |
|---|---|---|---|
| Milan | k=3, **r=0**, −0.535, 0.89 | −0.527, **0.021** | −0.447, 0.66 |
| Strasbourg | k=3, **r=0**, −0.648, 0.54 | −0.637, **0.036** | −0.695, 0.93 |
| Utrecht | k=3, **r=0**, −0.776, 0.67 | −0.802, 0.54 | −0.625, 0.95 |
| Vienna | k=1, **r=2**, −0.893, 0.35 | −0.869, 0.51 | −0.570, 0.97 |
| Aix | k=1, **r=2**, −1.083, 0.31 | −1.031, 0.60 | −0.759, 0.78 |
| Arévalo | k=3, r=1, −0.855, 0.63 | −0.995, 0.60 | −0.617, 0.88 |
| Angers | k=1, **r=2**, −1.038, 0.27 | −1.014, 0.59 | −0.635, 0.64 |
| Penn | k=2, r=1, −1.086, 0.23 | −1.079, 0.24 | −1.101, 0.76 |

**At its own optimum Johansen reaches a usable rank (`r = 1`) in 2 of 8 pairs.**
In three it finds no cointegration at all, and in three it finds full rank
(`r = M`, which in a bivariate system says the series are stationary and there is
nothing to cointegrate). `drvec` with `q = 1` gives `r = 1` with clean residuals
in all eight.

### The bind, and why it is not a coding difference

The AIC picks `k = 3` for the three pairs where it needs the lags **to whiten the
residuals** — and at `k = 3` the rank test collapses to `r = 0`. Measured
separately by sweeping the order: on these samples Johansen loses the
cointegration it finds at `k = 1` somewhere around `k = 3–4`, because each extra
lag costs `m² = 4` parameters out of 90–113 observations.

So there is a genuine bind: **the lags needed to whiten destroy the rank test.**
The VARMA escapes it by whitening with 4 MA parameters instead of 3 extra `Γ`
matrices, and it is more parsimonious *where it matters*:

| | parameters |
|---|---|
| Johansen, `k=3` (needed to whiten Milan, Strasbourg, Utrecht) | 16 |
| `drvec`, `p=2, q=1` | **14** |

### And `drvec` with `q = 0` fails the diagnosis too

Worth stating, because it is the same finding from the other side: on Milan
(0.021) and Strasbourg (0.036) the `q = 0` fit — the *same model* Johansen fits —
**does not pass its residual test**. Neither program can whiten these two pairs
without either an MA term or enough lags to lose the rank. That is a property of
the data, and it is the case for a VARMA rather than an argument about software.

---

## 2b. And the theory says which inference is the valid one

The measurements above have a theoretical counterpart, and it is worth stating
because it changes what the comparison means.

**A standard error can be computed for anything; valid inference is a different
claim.** For the cointegrating coefficient the two are not the same question,
because `β̂` is superconsistent and its limit distribution is mixed normal rather
than normal.

* **`drvec`'s route is licensed explicitly.** The transformed system it estimates
  is Phillips' (1991) triangular representation — Mauricio's equation (19) — and
  the paper concludes that *«asymptotic optimal inference applies to full-system
  EML estimation»* and that tests on `Λ` and `B` *«can be conducted using standard
  (e.g., Wald or likelihood ratio) asymptotic χ² tests»*. See
  [MODEL.md §2b](MODEL.md).
* **Johansen's is licensed only when the VECM is correctly specified.** Cappuccio
  (1996) shows there is **no reparameterisation** of an ARMA cointegrated system
  that yields a VECM with independent errors, so with MA dynamics present
  Johansen's ML is misspecified and *«estimation and inference on the
  cointegrating vectors and on the short-run parameters will be somehow affected»*.
  He names the obvious escape — approximate the MA with more autoregressive
  terms — and §1 above is that escape, measured: on these samples the lags cost
  the rank test first.

So the divergence in §2 is not two implementations disagreeing. It is one of them
estimating the model the data appear to have, with inference the theory backs, and
the other estimating a model that cannot represent that data's error structure.

**Confirmed for this code, not just asserted.** Wald and LR are asymptotically
equivalent under the theory, so their agreement is a check on the implementation:
on simulated data with a known coefficient they agree to four decimals from
n = 200 (table in [MODEL.md §2b](MODEL.md)). That is also why the comparison of
standard errors in §2 should not be read as one program being "tighter" than the
other — they are answering under different specifications.

## 3. What this does *not* establish

* **Not a claim that Johansen is wrong.** On the same specification the two
  agree; the divergence appears only when each is allowed its own model, and then
  they are answering with different model classes.
* **The rank Johansen selects at its optimum is unstable** (0, 1 or 2 depending
  on the pair and the order), which is consistent with the finite-sample
  over-rejection already measured for `drvec`'s own sequential test — about three
  times its nominal size at n = 120. Neither program's rank decision should be
  read as settled on samples of this length.
* **The integration order is not clean either.** Where Johansen picks `r = 2` the
  evidence on whether the series have a unit root is mixed: for Aix and Angers,
  ADF rejects the unit root and KPSS rejects stationarity, on the same series.
  For the same London series, ADF says I(1) on the 1700–1813 window and rejects
  the unit root on 1700–1789. Any conclusion resting on the integration order of
  these samples is fragile.
* **Not a parsimony claim in general** — only at the orders each method needs to
  produce an adequate model on these data.
* **Not a claim that Johansen's standard errors are wrong arithmetic.** They are
  computed correctly for the model Johansen fits; the issue is whether that model
  can represent the data's error structure (§2b).

---

## 4. Reproducing it

```sh
make
python3 tools/compare_johansen.py <pair.inp> 3
```

The `.inp` holds the two log price levels in `[Y₂ ; Y₁]` column order; the tool
renormalises Johansen's `β` on the same variable `drvec` normalises on, so the
two numbers are the same object. `det_order=0` / `deterministic="ci"` is the
restricted constant, i.e. `drvec`'s case 2.

Needs `statsmodels`. The pair files for the run above are built from `fue` `.inp`
files by `~/Dropbox/Cycles/Analysis/drvec_par.py`.

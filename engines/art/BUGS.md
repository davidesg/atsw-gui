# Known bugs — art (C identifier)

The reference is art-python (`docs/ESTUDIO-identificador.md` §7): whatever the C
does differently from art's identifier is a defect, unless the analyst decides
otherwise. These come from art-python's bugs of 2026-09-29 to 2026-10-09,
checked against `src/model_detection.c` and against `bin/art` on simulated
series (2026-10-09).

## BUG (HIGH) — the candidates come from the MLP, not from the significance gates

**Found:** 2026-10-08 (art-python BUG-0240, `ART_18/tests/benchmark_arima.py`).
**File:** `src/model_detection.c`, `build_mlp_shortlist` (~995).

### Symptom

ARMA(1,1) φ = 0.95, θ = 0.7 (persistent signal plus measurement noise, the
house-price shape), d = 1, n = 200, 50 replications, exact order:

| | C (`--mlp-direct`) | art-python |
|---|---|---|
| first / in the top three | **18 % / 38 %** | 72 % / 100 % |

On one series (n = 216, `-d 1 --harmonics off`), the C puts the AR(2) first and
the ARMA(1,1) third. art-python puts the ARMA(1,1) first. On an AR(1) φ = 0.6
the C lists the AR(2), although the PACF at 2 (−0.09) is inside the band;
art's AR gate (`_validate_ar`) keeps it out.

### Fix

Enumerate the candidates as art does: every (p,q) that passes `_validate_ar` and
`_validate_ma` within the effective orders, plus white noise when Ljung-Box
admits it, plus the mixed cells (next bug). The MLP stays out of the list.
art-python does not use it (decided 2026-10-09: on the batteries it adds
nothing that the correlogram does not give).

## BUG (MEDIUM) — the mixed ARMA cells enter by AICc

**Found:** 2026-10-09 (art-python, `suggest_orders`, `ART_MIXTOS_POR`).
**File:** `src/model_detection.c`, `add_arma_grid_candidates` (~2465).

### Symptom

The grid ARMA(p ≤ 3, q ≤ 2) is estimated by Hannan-Rissanen. Only the four
mixed cells with the lowest AICc enter the shortlist. Since 2026-10-09
art-python lets all six cells in and places them by the similarity of their
searched template, like every other candidate. There is no AICc before the
estimation, except to break the first-place tie between pure candidates of equal
size.

Measured in art-python (`ART_18/tests/results_*_gate_{aicc,similitud}.txt`):
the mean accuracy is the same, and the ARMA(1,1) .95/.7 rises from 64/90 % to
72/100 % (first / top three).

### Fix

Drop the sort and the `added < 4` limit, and add every cell that Hannan-Rissanen
estimates. `rank_shortlist_by_fit` already places them by similarity.

## BUG (MEDIUM) — the effective AR order reads a block of PACF lags as a high-order AR

**Found:** 2026-10-09, comparing with art-python BUG-0194 (2026-09-29).
**File:** `src/model_detection.c`, `determine_effective_orders` (~200-263).

### Symptom

The C takes the last significant PACF lag before three quiet ones, capped at 5.
art-python searches the regular AR up to 3. It admits an AR of order s/k ≥ 4
only on its own pattern: a significant and ISOLATED PACF bar, with every lag
from 4 to s/k − 1 inside the band. Decided 2026-09-29: a block of significant
lags is persistence, not a high-order AR. On Chile's ∇ln CPI (an I(2) series,
PACF significant at 1, 2, 3, 5 and 6), letting an AR of high order compete made
the AR(6) win and absorb the unit root that the DCD has to find.

Note: this rule is why the C finds the AR(4) of Salamanca (20/54 % against art's
0/0 %, art-python BUG-0238, wontfix). In the monthly series the AR(4) enters
art's list too. It loses first place to Box-Jenkins parsimony, by design. To
follow art is to give that up.

Checked 2026-10-09, the other way round: art-python with the C's rule (last
significant lag up to 5, p_max = 5 when s = 1). Chile and Colombia keep their
first candidate; the AR(5) only enters lower in the list. The thesis tests
pass. But the AR(4) of Salamanca stays at 0 / 0 % (first / top three, ARIMA
battery), and the means drop by one point (75/81 → 74/80 %, three engines
86 → 85 % in the top three). So the C's rule does not explain why the C finds
the AR(4): that comes from its own ranking. art keeps the cap at 3.

### Fix

The same rule as art-python's `_effective_orders`: `_last_sig(pacf, min(p_max, 3))`,
plus the isolated-bar rule for the submultiples s/k ≥ 4.

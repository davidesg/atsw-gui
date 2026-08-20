# The specification, the admissible set, and how `drvec` should be built

*Written after the measurements of [HOMOLOGATION.md](HOMOLOGATION.md) §4g–§4l
and before the work they imply, so the route is decided once. Same genre as
[VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md): what fails, the precedent, what
the sources say, what has to change, what must not be the fix, the bank, and the
order of work.*

---

## 1. What fails

A user who runs

```sh
bin/drvec data/pairs/milan 2 1 1 -case 2 -mean
```

gets today the widest specification the program can fit, and it is the one the
measurements say is least trustworthy. On all eight wheat pairs that fit

* stops on `steptol` or on a failed line search, **never on the gradient**;
* has its smallest moving-average root at **1.0000**, the invertibility
  boundary, from every seed tried — good seeds included;
* has `Θ̂(1)` singular to working precision, with the null direction aligned with
  `Λ⊥` at 0.946–0.9996, so `G = σ_min(Λ⊥′Θ(1)B⊥)` sits at 0.016–0.133 against
  ≈ 1 for a correctly specified model. **The fitted model denies the rank it was
  estimated at**;
* and returns a `B̂₂` that disagrees by 0.1 to 0.4 with every admissible route,
  while those routes agree with each other.

None of that is a coding defect. The transformation is verified to 4e−15 and the
estimator recovers known truths. **It is the specification**: a free `M×M` `Θ`
with a free `F` is wider than any theory that justifies it, and at the sample
sizes of applied work (n = 90–113) it is close to unidentified.

## 2. The precedent in the suite

The ladder. `-rungs` already estimates rungs 0–2 at `r = 0`, certifies the base
against the univariate fits, and reports the LRs with their degrees of freedom,
so the sequence is something the program produces rather than something a user
assembles. The specification of `Θ` and `Φ` needs exactly the same treatment,
and the rungs already exist as options: `-warma`, `-mawarma`, `-marow`,
`-matri`, free.

## 2b. The theory this rests on

Written out in [THEORY.md](THEORY.md), because the theorems this program had
been leaning on were all proved for neighbouring models. What that note
establishes, and what each item of §4 below depends on:

* **Theorem 3.** `{Y_t}` from (1) has cointegrating rank exactly `r` **iff**
  `rank(Λ⊥′Θ(1)) = M − r`. Nothing in Mauricio (2006) states this in these
  parameters, and nothing in the estimation enforces it.
* **Corollary 3.1.** `det Θ(1) ≠ 0` suffices. So the entire difficulty is the
  surface `det Θ(1) = 0` — a unit moving-average root at frequency zero.
* **Theorem 4.** That surface is inside the set the optimiser searches, the
  likelihood is finite on it, and points of it can fail the rank condition. The
  maximiser over the search set need not be in the model class.
* **Theorem 5.** The failure is **invisible** to the engine's checks: the
  transformed system stays stationary and only its moving average touches the
  unit circle, which the engine permits. Hence the condition must be computed on
  `(Λ, B, Θ)` directly.
* **Corollary 5.1.** Phillips optimality, the mixed-normal limit of `B̂₂` and the
  `χ²` limits are theorems **about the model class**. At a fit on that boundary
  they are not licensed — which is why the comparisons of §4i had to be
  bootstrapped.
* **Corollary 6.2.** The restriction `Θ = [T₁₁ T₁₂ ; 0 0]` makes the degeneracy
  impossible by construction, with no constant to choose. That is the reason a
  specification is preferred to a floor.

## 3. What the sources actually say

* **Mauricio (2006)** assumes partial nonstationarity of the *true* process and
  refers to Yap and Reinsel (1995) for identifiability, without translating
  either into the parameters it proposes estimating. The admissible set is never
  written down, and the pathological region sits on the boundary the engine
  permits (§4h).
* **The BVECM theorems** are proved for Phillips' triangular model in the WARMA
  parameterisation. Their VEC image carries two restrictions the general VEC
  does not have — `Γ_i = M_lα′` and the moving-average structure — and the
  converse ("any VEC can be written in WARMA form") is **asserted**. Mauricio's
  transformation cannot supply it: an identity does not turn a rank-`M` `Γ_i`
  into a rank-`r` one (§4k).
* In `Ȳ = [∇Y₂ ; W]` coordinates that class is a **pattern of zeros**, and `B₂`
  enters only by forming `W` — by subtraction. Estimating it there is measured
  to converge on the gradient in eight of eight, with multi-start spreads of
  0.000 to 4.0 against 0.4 to 18.7 (§4l).

## 4. What has to change

1. **The inverse map for `-warma`.** It reports in `Ȳ` coordinates because that
   is what it estimates. To be usable as a primary route it must also report
   `Λ`, `F_i`, `Π = ΛB′` and the rank condition, by inverting the transformation
   once at the end — which is what the legacy's `analisis_BEC` does and what
   [ESTUDIO_BVECM_vs_DRVEC.md](ESTUDIO_BVECM_vs_DRVEC.md) §1 calls the dual
   direction.
2. **The specification ladder as one command.** One run, five rows: `-warma`,
   `-mawarma`, `-marow`, `-matri`, free. Per row: `npar`, logL, termination,
   `G`, the smallest moving-average root, `B̂₂`, and a mark for whether the fit is
   **admissible**. Plus the nested comparisons that are valid, and a refusal to
   print a `χ²` p-value for the ones that are not.
3. **Admissibility carried into the rank test.** `-lrtest` fits a free `Θ` at
   every rank, and those fits are exactly the ones that land inadmissibly. A
   rank test built on inadmissible fits is not a rank test, and the applied
   conclusions — what rank the wheat pairs have — may move. This is the
   measurement with the most at stake.
4. **The default, decided with the register in hand.** Not before.

## 5. What must not be the fix

* **Not relaxing the invertibility or stationarity checks.** The pathology is a
  property of the specification, not of the guard.
* **Not making a restriction the silent default.** Every recorded result in the
  register rests on the current default; it moves only with the measurement in
  front of it, and the golden values are re-measured when it does.
* **Not preferring a tolerance to a specification.** `-rankadm` works and is the
  right diagnostic, but a floor is a number someone chose; `-marow` and `-warma`
  are classes that cannot degenerate — Corollary 6.2 of [THEORY.md](THEORY.md),
  not a preference. Use the floor to measure, the class to estimate.
* **Not reporting parameters that were never estimated.** Twice now a new branch
  through a parameter walk has published a `Θ` and a `B₂` nobody fitted
  ([DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) §8d, §8h). Every new route
  reports what it estimated, in the coordinates it estimated it in, and the
  structural walk check covers it at `M = 5, r = 2` where a wrong dimension is
  visible.

## 6. The bank

Fixed and already in place: the eight wheat pairs (`data/pairs/`),
`mink_muskrat` in the three deterministic cases, the synthetic series of known
rank, and `badnorm`. Plus `tools/sim`, which is what made any of this decidable:
a VEC simulator, a WARMA simulator, and the two identity checks.

Per route, the same five quantities as §4b, taken with
`tools/measure_seeding_bank.sh`, plus `G` and the moving-average root.

**And the split matters more than the average.** The triangular class carries
the eight pairs and fails visibly on `mink_muskrat` (−179, −14, −36 against 3.7,
6.5, 6.5). A specification that fails where it does not apply, loudly, is worth
more than one that degrades quietly everywhere; any candidate default has to be
judged on both halves.

## 7. The order of work

1. ~~**The inverse map**~~ **Done**: `-warma` reports the fit in both coordinate
   systems, with `Λ`, `F_i`, `Θ_j`, `Π = ΛB′` and the rank condition recovered by
   inverting the transformation once, and the residual of that inversion printed
   beside them — `10⁻¹⁷` on every case of the bank
   ([HOMOLOGATION.md](HOMOLOGATION.md) §4l). Theorem 6's structure comes out of
   the inversion by itself, which is the check that it was done right.
2. **The specification ladder in one command**, with the admissibility verdict
   and no invalid p-values.
3. **Re-measure `-lrtest`** under `-warma` and `-marow` on the bank, against the
   free version and against the bootstrap critical values of §2.3. This is where
   an applied conclusion can actually change.
4. **Decide the default**, publish the comparison, and re-measure the golden
   values if it moves.
5. **Test the autoregressive half** of the theorems' class (`Γ_i = M_lα′`) as a
   restriction rather than as the indication §4k leaves it at — with a
   bootstrap, for the same boundary reason as §4i.

Steps 1 and 2 are plumbing with a measured purpose. Step 3 is the one that can
change what the program says about data. Step 4 is the only one that touches a
recorded result, and it comes last on purpose.

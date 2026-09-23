# The specification, the admissible set, and how `drvec` should be built

> **Status: the five steps are done.** What each one produced is marked in §7,
> the decision it led to is §8, and what it left open is §9. The measurements
> live in [HOMOLOGATION.md](HOMOLOGATION.md) §4g–§4p and the theory in
> [THEORY.md](THEORY.md).

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
2. ~~**The specification ladder in one command**~~ **Done**: `-specs`, with the
   admissibility verdict read off the rank condition, no `χ²` p-value for a
   comparison involving an inadmissible rung, and a negative LR flagged as the
   non-convergence it is ([HOMOLOGATION.md](HOMOLOGATION.md) §4m).
3. ~~**Re-measure `-lrtest`**~~ **Done, and the applied conclusion did change**
   ([HOMOLOGATION.md](HOMOLOGATION.md) §4n). At `q = 0` all eight pairs reject
   `r = 0` at the bootstrap resolution floor; with a free `q = 1` only two do,
   and with admissibility enforced only one. The reason is Theorem 3: the free
   moving average degenerates in the `Λ⊥` direction, which is the one carrying
   the common trend, so it absorbs what the test measures. **The protocol that
   follows is to select the rank at `q = 0` and fit the moving average at the
   selected rank.** `-warma` and `-marow` cannot supply an alternative for a
   rank test because they collapse at `r = 0` — a structural obstacle, recorded
   as open.
4. ~~**Decide the default**~~ **Decided, and it does not move.** The reasoning
   is §8 below; the short form is that the free fit is measured to be wrong on
   this data *and* the constrained one is measured to be a downward-biased bound
   rather than an answer, so substituting one for the other would be replacing a
   known error with a different one. What changed instead is the
   **presentation**: an inadmissible fit now says so on the terminal, and
   [GETTING_STARTED.md](GETTING_STARTED.md) carries the order to do things in.
   No recorded result moves and no golden value is re-measured, which was
   verified rather than assumed — the suite is green on all of them.
5. ~~**Test the autoregressive half**~~ **Done**: `-artest`
   ([HOMOLOGATION.md](HOMOLOGATION.md) §4p). **Not rejected in seven of the
   eight pairs**, which reverses the indication §4k had left — the row space of
   a freely estimated `F̂₁` is a weakly determined direction, and a distance
   between two such is not evidence. The class therefore splits: its
   moving-average half is rejected (§4i, eight of eleven) and its autoregressive
   half is not. Bootstrapped for a different reason than §4i: here the
   unrestricted model is admissible, and what is non-standard is that
   `Γ_i = M_iα′` is a reduced-rank restriction.

Steps 1 and 2 are plumbing with a measured purpose. Step 3 is the one that can
change what the program says about data. Step 4 is the only one that touches a
recorded result, and it comes last on purpose.

## 8. The default, decided

**The default computation does not move. The default presentation does.**

Three candidates were on the table and two are refused by their own
measurements.

* **Make a restricted class the default.** Refused by §5 of this plan, and
  independently by the measurement: `-mawarma` is rejected at 5 % in eight of
  eleven cases by a bootstrap that is valid (§4i), and `-warma` fits
  `mink_muskrat` twenty units worse than the free model (§4l). A default has to
  work where it does not apply, and neither does.
* **Make `-rankadm` the default.** Tempting, because by Theorem 4 the admissible
  set *is* the model class and searching outside it is the defect, not a
  preference. Refused because of what §4n measures: the constrained fit binds on
  `L(1)` and leaves `L(0)` alone, so it is a **downward-biased bound** and not an
  answer. Substituting it for the free fit would replace one known error with a
  different one, and it would move every recorded number in the register to a
  value nobody is prepared to defend.
* **Keep the free model and say what it is.** Taken. The free model is the widest
  in the class and the one every recorded result rests on; what was wrong was
  not that the program fits it but that it handed the result over without saying
  that, on this kind of data, the fit denies the rank it was estimated at.

So: every fit reports `σ_min(Λ⊥′Θ(1)B⊥)`, an inadmissible one now says so **on
the terminal** with the one command that shows the alternatives, and
[GETTING_STARTED.md](GETTING_STARTED.md) carries the protocol — rank at `q = 0`,
ladder at the selected rank, then estimate. The suite checks the notice in both
directions, because a warning that fires on everything is not a warning.

*What this leaves undone, stated so the decision is not read as a closure:* there
is no default that is both admissible and unbiased, because building one needs
a rank test whose alternative is admissible, and §4n records why that cannot be
written in the restricted classes. Until then the honest default is the widest
model with its verdict attached.

## 9. What the plan produced, and what it left open

**Produced.** Five options that did not exist and one that did not work:
`-warma` with its inverse map, `-specs`, `-rankadm` with a floor set from the
measurement rather than from taste, `-artest`, and a rank condition reported at
every fit and shouted on the terminal when it fails. Two theorems written down
that the literature states for neighbouring models and not for this one
(Theorems 3 and 4 of [THEORY.md](THEORY.md)), and one converse shown to be
asserted rather than proved (Theorem 6).

**Changed about what the program says.** One applied conclusion moved: the
cointegration evidence in the eight wheat pairs is unanimous at `q = 0` and
mostly gone at `q = 1`, for a reason that is now a theorem rather than a
shrug — the free moving average degenerates in the direction carrying the common
trend. The protocol that follows is in
[GETTING_STARTED.md](GETTING_STARTED.md).

**Changed about what the program computes.** Nothing by default. No golden value
moved, and that was verified rather than assumed.

**Left open, and stated so it is not read as closure.**

* There is **no default that is both admissible and unbiased**, because building
  one needs a rank test whose alternative is admissible, and §4n records why
  that cannot be written in the restricted classes: they are defined relative to
  the `r/s` split and collapse at `r = 0`.
* The restricted classes carry the wheat pairs and **fail on `mink_muskrat`**
  (§4l). Nothing here explains why that dataset is different, beyond the record
  already noting that Johansen and `drvec` disagree about its `B₂` and that its
  surface has been hard since the beginning.
* The moving-average half of the triangular class is **rejected** (§4i) and its
  autoregressive half is **not** (§4p). The specification that fits both
  findings — a free `T₂₂` that cannot reach one — is `-marow`, but no test of
  `-marow` against the free model with a valid distribution has been built.
* Four of the bootstrap p-values in §4n and §4i sit at the floor `1/(B+1)`.
  Resolving 1 % needs `B ≥ 999`, which nothing here has run.
## 10. The default, decided again — and this time it moves

*Written after the measurements of [HOMOLOGATION.md](HOMOLOGATION.md) §4q and
§4r, and after Corollary 6.3 of `DEMOSTRACIONES.md`. §8 is superseded; the
reasoning that produced it is left standing above, because seeing why a correct
argument was applied to the wrong object is worth more than a clean page.*

**What §8 argued.** Keep the free model as the default, because the two
candidates for replacing it both fail: `-rankadm` gives a **downward-biased
bound** rather than an answer (§4n), and the inherited class is **rejected** by
the data in eight of eleven cases (§4i). So the honest default is the widest
model with its verdict attached.

**Why that was the wrong object.** Both legs are statements about imposing a
**restriction on `𝒫`**. The class of Theorem 6 is not one. By Corollary 6.3,
with the bottom `s` rows of every `Θ̃ₖ` zero, `det Θ̃(1) = det(I_r − ΣT_k)` and
the non-zero eigenvalues of the companion are exactly those of the `r × r` block
— so `𝒞` **is the whole parameter space of that class**, and the engine's own
invertibility gate enforces the rank condition of Theorem 3. There is no bound
to bind, hence no downward bias. And §4i's rejection was measured against a free
alternative sitting on the invertibility boundary, where by Theorem 10 the
statistic has no distribution — §4i says so itself. It establishes that the
alternative is outside the model class, not that the class is wrong.

**What the measurements say.** §4q: in the regime the bank lives in — moving
averages of the `(1 − θB)` kind, which is what differenced price and population
series give — the free `Θ` is not recoverable at `n = 120` or `n = 250`, at any
magnitude, and `-multistart` makes it worse rather than better. §4r: on a WARMA
truth with an identified `w` block, `-mawarma` recovers `θ = +0.9` — the hard,
near-unit-circle case — with bias 0.050 and IQR 0.272, interior, `G = 0.96`,
where the free fit parks on the gate with `G = 0.45` and twice the dispersion.
And §4p: the autoregressive half of the same class is **not** rejected in seven
of eight pairs.

**The decision.** The class of Theorem 6 becomes the model `drvec` fits by
default; the free `Θ` becomes what it actually is — the widest parameterisation,
useful as a diagnostic and as the thing to compare against, and not a
specification anyone should publish from on this kind of data. For `M = 2,
r = 1` that is **one** moving-average parameter instead of four, which is also
the only version of this model with a chance of earning its parameters against a
univariate alternative.

**What that costs, stated because a default that moves moves every number.**
Every figure in the register measured on the free default was measured on a
different model from the one the program will fit. They are not wrong — they are
measurements of the free parameterisation, and §4q and §4r are what they now
mean — but the register has to say which default each of them belongs to, and
the golden values of the suite have to be re-measured or re-labelled. That is
the work, and it is why this is a decision and not a patch.

**What is still not decided.** Whether the resulting model forecasts better than
a univariate one. Nothing in this plan, or in the register, measures that; it is
the acceptance criterion the whole specification question exists to serve, and
it is in `PLAN_PRODUCCION.md` as the phase that closes the program rather than
as an option.

---

## 11. The default, decided a third time — the free class, and a search to carry it

*2026-09-23. §10 is superseded; it is left standing for the same reason §8 was.*

**What §10 rested on, and what is left of it.** Two legs. The theoretical one —
Corollary 6.3: with the lower `s` rows of `Θ` zero, "the admissible region is
the whole space and the engine's gate imposes it" — is **false in both
directions**: a `-marow` point with `Θ₁ = [[1, .7], [0, 0]]` passes the gate and
is inadmissible, and `Θ₁ = [[3, .4], [0, 0]]` is refused and admissible
([ESTUDIO_MAURICIO_2026-09-23.md](ESTUDIO_MAURICIO_2026-09-23.md) §4). The source
it was attributed to, the BVECM article, uses its restricted class only to frame
its proofs; what it and its program estimate is the full model — the program with
an MA on exactly the entry `-marow` zeroes ([ESTUDIO_MAROW_2026-09-23.md](ESTUDIO_MAROW_2026-09-23.md)).
The empirical leg — §4q, the free `Θ` is not recoverable at these sample sizes —
stands, but it is a statement about **one start**.

**What changed the balance.** The search (P12, `CHANGELOG.md`): the free fit
starts from the optima of every class it contains, embedded, so it can never end
below them; on the wheat pairs `marow ≤ matri ≤ free` now holds where one cold
start gave them out of order. Against the restriction, the data of the suite
itself: `-matri` beats `-marow` by LR ≥ 5.96 in 37 of 40 configurations, and the
free class forecast better in 8 of 9 cases (§4t).

**The decision** (the user's, 2026-09-23): the default is the free `Θ`, Mauricio
(2006)'s model. `-marow`, `-matri`, `-mawarma` and `-warma` remain as
restrictions to test against it. That also keeps `-lrtest` nested (BUG-27).

**What it costs.** Every figure measured between 2026-08-20 and today without a
class flag belongs to `-marow`; the note at the top of `HOMOLOGATION.md` says so.
The battery's golden values without a class flag now read the free fit, and the
`-marow` ones are kept, with the flag, so the class stays covered.

**Still open.** The bootstrap test of one class against another, which is what
would make choosing a restriction a measurement rather than an assumption.


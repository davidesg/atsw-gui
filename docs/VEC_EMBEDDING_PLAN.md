# Embedding the VEC matrix: the specification, the obstacle, and the plan

*How `Π = ΛB′` is added on top of the certified entry gate, why the ladder's
usual bridge does not reach this rung, and what has to be built and measured
before it does.*

*Companion documents: [MODEL.md](MODEL.md) for the model and the parameter
vector; [SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §3–§4 for the ladder's two
contracts and for how far univariate information travels.*

---

## 1. What this is for

The suite's ladder is a construction of **optima from optima**. Each rung is
estimated, certified, and handed upward: the certificate of one rung is the
starting point of the next, and a rung whose base has not been certified cannot
be read, because whatever is wrong below is already inside it.

`drvec` now holds the base. At the diagonal rung — `r = 0` with diagonal `F`,
`Θ` and `Σ` — the exact likelihood factorises, and every run there verifies both
contracts and reports them: the crossing identity, and the optimality
certificate that says whether what arrived was an optimum or a specification.

What comes next is the structure this program exists to estimate. Where the
suite's transfer-function program adds a filter, `drvec` adds the
**error-correction matrix**

```
  Π = Λ B′,     rank r,     B = [ I_r ; B₂ ],     B₂ of size s × r.
```

This document specifies how that matrix is embedded, establishes why the
embedding the ladder uses everywhere else does not work here, and sets out the
order of work. It is written before the work rather than after it, so that the
route is decided once.

## 2. The model, and where the matrix enters

The estimated model is the vector error-correction form

```
  F(L) ∇Yₜ = −Λ (B′Y_{t−1} − E[W])  +  Θ(L) Aₜ,     Aₜ ~ iid N(0, Σ)
```

and estimation is carried out not on `Yₜ` but on Mauricio's (2006)
transformation

```
  Ȳₜ = ( ∇Y_{2t}′ , Wₜ′ )′,      Wₜ = Y_{1t} + B₂′ Y_{2t},
```

which is a **stationary** VARMA whose exact unconditional likelihood the engine
computes. The transformed autoregressive matrices are, with `C̄`, `C̄⁻¹`, `H̄`
and `Λ̄ = [ 0_{M×s} , Λ ]` as in [MODEL.md](MODEL.md) §2,

```
  Φ̄₀ = C̄⁻¹
  Φ̄₁ = C̄⁻¹H̄ − Λ̄ + F₁C̄⁻¹
  Φ̄ᵢ = FᵢC̄⁻¹ − F_{i−1}C̄⁻¹H̄        i = 2 … p−1
  Φ̄_p = −F_{p−1}C̄⁻¹H̄
```

Two properties of this decide everything below.

**The VEC matrix enters only through `Φ̄₁`, and only as `−Λ̄`.** Every other
appearance of `Λ` in the model is through that one term. `B₂`, by contrast,
enters *everywhere*: it defines `C̄`, so it reaches every `Φ̄ᵢ`, the moving-average
matrices `Θ*ᵢ = C̄ΘᵢC̄⁻¹`, the covariance `Σ* = C̄ΣC̄′`, and the data itself through
`Wₜ`. `Λ` is a coefficient; `B₂` is a change of coordinates.

**At `r = 0` the transformation is the identity.** With `s = M` we have `C̄ = I`
and `H̄ = 0`, so `Φ̄ᵢ = Fᵢ`, `Θ*ᵢ = Θᵢ` and `Σ* = Σ`. The gate therefore estimates
`F`, `Θ` and `Σ` **in the same coordinates** the error-correction form uses. This
is what makes the gate a usable base at all: its estimates are not in a
representation that has to be translated before they can be carried upward.

## 3. The obstacle, and it is structural

The ladder's bridge everywhere else is: take the certified optimum, set the new
block to zero, and start there. The new block at zero reproduces the previous
rung exactly, so the starting log-likelihood equals the certified one and the
optimiser can only improve on it. **That bridge does not reach this rung.**

Setting `Λ = 0` does reproduce `Π = 0`, which is the `r = 0` model. But it does
so at a point where the transformed system has a **unit autoregressive root**,
and the likelihood is not defined there: the engine rejects it. Computed
directly from the expressions above, for `M = 2`, `r = 1`, `p = 2`, `B₂ = −0.9`
and `F₁ = diag(0.3, 0.2)`, the smallest root modulus of `Φ̄(z)` is

| ‖Λ‖ | adjusting direction | opposite direction |
|---|---|---|
| 0 | **1.000000** | **1.000000** |
| 1e−6 | 1.000000 | 1.000000 |
| 1e−3 | 1.00177 | 0.99824 |
| 0.05 | 1.10130 | 0.92111 |
| 0.40 | 1.88424 | 0.61937 |

Three things follow, and they are the substance of the problem.

**The base sits exactly on the boundary of the space above it.** Not near it:
at `Λ = 0` the modulus is one to every digit computed. The `r = 0` model is not
an interior point of the `r = 1` parameter space, so the certified optimum
cannot be evaluated there, let alone started from.

**Only one direction leaves the boundary admissibly.** The sign of `Λ` decides
whether the root moves outside the unit circle or inside it, and the second is
non-stationary and equally unusable. A seed must therefore enter the interior
along the adjusting direction, which is a statement about *where* to start, not
merely how far.

**And `B₂` is unidentified at the boundary.** `Π = ΛB′ = 0` for every `B₂` when
`Λ = 0`, so the point the base occupies is also a point of non-identification.
This is the same feature that makes the rank test's distribution non-standard
(Johansen, 1988), and it is why the parametric bootstrap exists in this program
at all; here it reappears as an obstacle to seeding.

### This explains a measurement already on record

[SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §4 records that seeding the full
univariate block at `r = 1` moves the **starting** log-likelihood *down* by 17
units, and attributes it to three structural reasons: marginalising a VARMA
mixes autoregressive and moving-average structure, `C̄` and `Λ` couple the
equations densely, and `Φ̄_p` is determined rather than free. All three stand.
What the table above adds is the reason underneath them: the natural embedding
is not merely inaccurate, it is **inadmissible**, so any seed has to be pushed
off the boundary by an amount nobody has specified — and an unspecified amount
is what a 17-unit drop looks like from outside.

## 4. What the gate will not tell you

Stated so that the certificate is not over-read. The gate certifies the base:
that the transformation, the differencing, the parameter walk, the deterministic
terms and the scaling all crossed intact, and that the values brought in were
optima. It says nothing whatever about

* whether the rank is one — the gate is fitted **at** `r = 0` by construction;
* whether a cointegrating relation exists;
* whether `B₂` is identified, or well conditioned, at the fit above;
* whether the moving-average operator will rest on its own boundary once the
  rank is raised — measured on eight real pairs, it usually does
  ([HOMOLOGATION.md](HOMOLOGATION.md) §3b).

A certified gate is a necessary condition for reading what is built on it, and
nothing more.

## 5. The rungs, specified

The path from the certified gate to the full model is a sequence of nested
models, each of which is a valid null for the next. Writing them out is the
point of this document, because the sequence — not any single fit — is what
makes the construction reproducible.

| rung | model | added | df of the LR against the rung below |
|---|---|---|---|
| **0** | `r = 0`, `F`, `Θ`, `Σ` all diagonal | — | *the certified base* |
| **1** | `r = 0`, `Σ` free | contemporaneous correlation | `M(M−1)/2` |
| **2** | `r = 0`, `F` and `Θ` free | cross dynamics | `(p−1)M(M−1) + qM(M−1)` |
| **3** | `r = 1` | `Λ`, `B₂` — **the VEC matrix** | non-standard |

Rungs 0 → 1 → 2 are ordinary nested comparisons: the restricted model is an
interior point of the unrestricted one, the log-likelihood cannot fall, and the
statistic is `χ²` with the stated degrees of freedom. Each is a legitimate
optimum-from-optimum bridge, and each is a check that can be run.

Rung 2 → 3 is the one this document is about, and it is not ordinary: by §3 the
null lies on the boundary of the alternative and `B₂` is unidentified under it.
The log-likelihood still cannot fall — `r = 0` is nested in `r = 1` as a model —
but neither the seeding bridge nor the `χ²` reference distribution survives the
crossing. The rank test already handles the second half of that with a
parametric bootstrap; the first half is what has to be built.

## 6. Three ways across, and they are not equally good

**(A) Enter the interior along the adjusting direction.** Carry the gate's
`F̂`, `Θ̂`, `Σ̂` unchanged — they are already in the right coordinates, by §2 —
take `B₂` from the static regression, and set `Λ` to a small step in the
direction that moves the root outward, calibrated so that the transformed system
is admissible with margin. Cheap, uses the certificate, and its only free choice
is the step length, which can be **measured** rather than assumed.

**(B) Profile `Λ` and `B₂` on the certified base.** Hold `F̂`, `Θ̂`, `Σ̂` at the
gate's values and estimate only `Λ` and `B₂`, then release everything. The
conditional problem is small and much better behaved than the joint one, and its
solution is by construction admissible, so it lands in the interior without any
step-length choice. It costs one extra optimisation.

**(C) What is done now.** `init_guess` ignores the gate and runs a conditional
regression for every block from scratch. It works, and it is what every recorded
result rests on, but it makes no use of the certificate and cannot state where
its starting point came from.

**The plan is (B), with (A) as the fallback** if the conditional problem proves
ill-conditioned on the bank. (B) is preferred because it needs no arbitrary
constant: the step off the boundary is chosen by the likelihood rather than by
the implementer. (C) is retained unchanged as the default until (B) is measured
to be at least as good, so that no recorded result moves without evidence.

### What must not be the fix

**Not relaxing the engine's stationarity check.** The unit root at `Λ = 0` is a
property of the model, not of the check. A tolerance that admits it would let
the optimiser evaluate points where the likelihood does not exist, and every
number downstream would be undefined rather than merely wrong.

**Not starting `Λ` at an arbitrary constant.** The table in §3 shows the root's
distance from the circle is a function of `Λ`, `B₂` and `F`; a fixed constant is
a different distance in every dataset, which is precisely the kind of hidden
choice that makes results irreproducible.

**Not abandoning the gate because the bridge is harder here.** The certificate
is what makes rungs 1 and 2 meaningful and what localises any failure below rung
3. That it does not extend to rung 3 unchanged is a fact to be documented, which
is what this document does, not a reason to stop certifying.

## 7. The bank this needs

Measurement, not opinion, decides between (B) and (C), so the bank is specified
before either is built.

* **The eight wheat-price pairs** already used for the Johansen comparison,
  where the answer at rung 3 is known from the recorded runs, so a change in the
  seeding must reproduce them or explain itself.
* **`mink_muskrat`**, the published case, in all three deterministic cases.
* **The synthetic series of known rank**, where rung 3's answer is known by
  construction and a seeding change must not alter it.
* **A case where the gate fails its own contract**, constructed on purpose, to
  confirm that the failure is reported at rung 0 and not first noticed at rung 3.

For each: the starting log-likelihood, the final log-likelihood, the optimiser's
termination, the number of restarts needed to reach the best point, and the
smallest moving-average root at the optimum.

## 8. The order of work

1. ~~**Report the rung ladder.**~~ **Done**: `-rungs` emits rungs 0–2 and their
   LRs from a single run, so the sequence is a thing the program produces rather
   than a thing a user assembles; rung 0 certifies itself in place. Rung 3
   already exists as `-lrtest`. Measured on `mink_muskrat`, `p = 2, q = 1`,
   case 1 — and the check that matters is the last line, not the statistics:

   | rung | npar | logL | LR | df | p |
   |---|---|---|---|---|---|
   | 0 `F`, `Θ`, `Σ` diagonal | 5 | −34.6278 | | | |
   | 1 `Σ` free | 6 | −30.5844 | 8.087 | 1 | 0.0045 |
   | 2 `F`, `Θ` free | 10 | −6.3311 | 48.507 | 4 | 0.0000 |

   Rung 2 reproduces the plain `r = 0` fit to the digit (−6.3311226118), which
   is what says the ladder re-estimates the same models and not a family
   configured differently. See [DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) §8.
2. ~~**Measure (C) as it stands**~~ **Done**: the baseline is
   [HOMOLOGATION.md](HOMOLOGATION.md) §4b, taken with
   `tools/measure_seeding_bank.sh` — the same instrument (B) will be measured
   with. The headline is that the cold start gives away **11.5–16.8** units of
   log-likelihood on the eight pairs and **287** on `mink_muskrat` case 1, that
   the best of 20 restarts beats it in 10 of the 15 cases, and that it reaches
   the best point first in only 3. Nothing in the seeding was altered. One
   thing outside it was: the bank turned up the case §7 asked to be
   constructed — a gate failing its own contract, on Milan — and the failure
   was the check's, not the gate's (§1b).
3. ~~**Build (B)** behind an option~~ **Done**: `-seedgate`. The default and the
   recorded results do not move.
4. ~~**Measure (B) against (C)**~~ **Done**: [HOMOLOGATION.md](HOMOLOGATION.md)
   §4c, same bank and same instrument.
5. **Decided, and it falls against (B).** It wins three of eleven cases and
   loses seven, by up to 37.6 log-likelihood units (Angers). The route stays in
   the program behind its flag because the measurement is worth keeping, not
   because it should be used. Two findings outlast it: the entry off the
   boundary is **one-sided** — the conditional regression's `Λ` has no reason to
   point the admissible way, and `elf` refuses the other — and it is
   **expensive**, 60 to 100 units; and the `r = 0` dynamics **do not transfer**
   to `r = 1`, which is what the whole route assumed.
6. ~~If (B) wins, make it the default~~ — it does not, so nothing moves.

### 8b. What the negative result points at next

(B) failed on its premise, not on its arithmetic: it assumed the rung below is a
good place to start the rung above. What it did *not* try is starting from an
estimate of `Λ` and `B₂` that is already close to the answer — and one exists,
in closed form.

**(D) The canonical reduced-rank solution.** Johansen's estimator solves an
eigenvalue problem for `β`, and `α` follows from it; both are closed-form and
cost no optimisation. The register already measures how good they are as a
starting point without having asked the question: §2.1b puts Johansen's `β`
within **0.0003 to 0.052** of `drvec`'s own optimum across 24 comparisons on the
eight pairs. That is a pre-estimate one to two orders of magnitude closer than
anything (B) or (C) produces.

Two things have to be handled and both are known. `Λ = −α` in this
parameterisation, since the model carries `−Λ(B′Y−E[W])` — the sign that
(B) had to discover by scanning. And the canonical `β` is normalised on a
different variable, so it needs renormalising onto `B = [I_r ; B₂]` before it can
be written into the parameter vector. Neither is a choice; both are conversions.

The measurement to take is the same one: same bank, same five quantities, and
`-eval` at the seed to see how far the start is before the optimiser touches
it.

Steps 2 and 4 are the reason for writing this first: without a baseline taken
before the change, the change cannot be evaluated, and the work is repeated.

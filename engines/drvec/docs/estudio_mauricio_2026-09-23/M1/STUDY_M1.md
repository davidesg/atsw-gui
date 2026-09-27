# STUDY M1 — Does `drvec` implement the model and the transformation of Mauricio (2006)?

*Scope: the paper's model (Sections 2–3, Remarks 1–6) and its transformation to a stationary
VARMA; the code that realises it in `/home/david/Dropbox/SRC/drvec/src/drvec.c` (repo HEAD
`5aebd28`, drvec 0.10). The engine (`elf`, `est`) is treated as a black box whose interface
was read from its headers and from Mauricio (2002, JTSA 23, "An algorithm for the exact
likelihood of a stationary VARMA model"). Everything here was done read-only; the scripts and
logs are next to this file.*

Legend: **VERIFIED** = proved here and/or checked numerically; **CONFIRMED DISCREPANCY** =
paper and code differ, with evidence; **OPEN** = not settled, with what would settle it.

Files in this directory:

| file | what it is |
|---|---|
| `paper.py` | independent numpy implementation of the paper (eqs. 5–18, 20–21), exact Gaussian likelihood from the block-Toeplitz covariance, simulator of (5) |
| `check_code.py`, `check_code.log` | paper vs `vec_shootx` + `elf`, 7 configurations |
| `check_theory.py`, `check_theory.log` | checks of the paper's own claims (unit roots, stationarity, means, the `-warma` class) |
| `drvec_copy/src/harness.c` | `#include "drvec.c"` with `main` renamed. It calls `build_y2_levels`, `vec_shootx` and `elf` on a given `x[]`. drvec's sources are copied, not modified |
| `runs/` | `drvec -lrtest` on a copy of `mink_muskrat.inp`, default vs `-mafree` |

---

## 0. Verdict

**The core is faithful.** `vec_shootx` builds exactly the paper's C̄, C̄⁻¹, H̄, Λ̄, Φ̄₀…Φ̄_p
(eq. 16), Φ\*_k = C̄Φ̄_k, Θ\*_k = C̄Θ_kC̄⁻¹ and Σ\* = C̄ΣC̄′ (eq. 18). It also builds the paper's
data vector Ȳ_t = (∇Y₂ₜ′, Wₜ′)′ (eq. 17) and the paper's mean E[Ȳ] under its three cases
(Remark 6, eqs. 20–21). The exact log-likelihood that `elf` returns on that input equals an
independent block-Toeplitz evaluation of the paper's stationary VARMA to 2–4·10⁻⁸. That
residual is entirely the truncated constant `LOG2PI = 1.837877066`. The difference in Φ\*,
Θ\*, Σ\*, μ is 0.0 and in Ȳ ≤ 1.4·10⁻¹⁴, over 7 configurations (M = 2, 3, 4; r = 1, 2;
p = 1, 2, 3; q = 0, 1, 2; cases 1, 2, 3; both data layouts).

**However, with q ≥ 1 and r ≥ 1 the default fit does not estimate the paper's model.** Since
P4 the default MA class is `-marow`: the s rows of every Θ_k belonging to the ∇Y₂ equations
are forced to zero. That is a proper subclass of the paper's model. It excludes the paper's
own estimates for its own example: the muskrat (Y₂) row of Θ̂₁ is (−0.8953, −0.0174) in
Table 4 and (−0.6039, −0.1837) in Table 5. Under the default the rank test also compares
non-nested models, because r = 0 keeps a free Θ. `-mafree` restores the paper's model
exactly.

There is a second, smaller discrepancy: the paper's conditional ML (Remark 4) is not
implemented. `-m 2` switches off the ξ-truncation of the exact likelihood, which makes it
*more* exact than the default, yet the `.out` labels it "Conditional (Approximate) Maximum
Likelihood".

**The paper's mathematics is correct.** Every derivation in Section 3 was redone and holds.
The paper's "rigorous argument" for stationarity has gaps, and a one-line determinant proof
replaces it (Theorem 2 below).

---

## 1. The paper's model, restated

**Data and model (eq. 1).** {Yₜ} is M-dimensional (M ≥ 2) and

  Φ(L)Yₜ = Θ(L)Aₜ, Φ(L) = I − Σᵢ₌₁ᵖ ΦᵢLⁱ, Θ(L) = I − Σⱼ₌₁^q ΘⱼLʲ, Aₜ ~ IID N(0, Σ).

Assumptions (p. 3646): |Φ(x)| = 0 and |Θ(x)| = 0 have no roots *inside* the unit circle.
Roots on the circle are allowed for Θ. Identifiability conditions of the Yap–Reinsel (1995)
type are assumed.

**Partial nonstationarity (p. 3646–3647).** |Φ(x)| = 0 has D roots equal to one, 0 < D < M,
and all other roots outside the circle. P = rank Φ(1) = M − D. The paper's P is drvec's r,
and D is drvec's s = M − r.

**VEC form (eqs. 2–3, 5).**

  ∇Yₜ = −ΠYₜ₋₁ + Σᵢ₌₁ᵖ⁻¹ Fᵢ∇Yₜ₋ᵢ + Θ(L)Aₜ, Π = Φ(1) = I − ΣΦᵢ, Fᵢ = −Σⱼ₌ᵢ₊₁ᵖ Φⱼ,

  Π = ΛB′ (eq. 4), Λ and B are M × P of full column rank, and Wₜ = B′Yₜ is stationary.

**Normalisation and partition (eqs. 6–7).** B = [I_P ; B₂], with B₂ of size (M−P) × P.
Yₜ = (Yₜ₁′, Yₜ₂′)′, with Yₜ₁ of size P and Yₜ₂ of size M − P. There is no cointegration within
Yₜ₂.

**Parameters.** B₂, Λ, F₁…F_{p−1}, Θ₁…Θ_q, Σ, and in Remark 6 the mean m = E[Ȳ].

*Check of (3) (VERIFIED).* Write (I − ΣFᵢLⁱ)(1 − L) + ΠL. The coefficient of L is
−F₁ − I + Π. The coefficient of Lⁱ for 2 ≤ i ≤ p−1 is Fᵢ₋₁ − Fᵢ. The coefficient of Lᵖ is
F_{p−1}. With Fᵢ = −Σ_{j>i}Φⱼ these are −Φ₁, −Φᵢ and −Φ_p respectively, so the expression
equals Φ(L). Hence (1) ⇔ (2) as polynomial identities, with no stationarity needed. That is
exactly what the paper says.

---

## 2. The transformation, redone

Notation: r = P and s = M − P. B⊥ := [−B₂′ ; I_s] is M × s, and B′B⊥ = −B₂′ + B₂′ = 0.
E₁ := [I_r ; 0_{s×r}].

### 2.1 The matrices

C̄ = [0_{s×r} I_s ; I_r B₂′] (eq. 11) and C̄⁻¹ = [−B₂′ I_r ; I_s 0] (eq. 14).

**Lemma 1 (VERIFIED).** C̄C̄⁻¹ = I_M, and |det C̄| = 1.

*Proof.* C̄C̄⁻¹ = [0·(−B₂′) + I_s·I_s, 0·I_r + I_s·0 ; I_r(−B₂′) + B₂′I_s, I_r·I_r + B₂′·0]
= [I_s 0 ; 0 I_r]. Permuting the block columns of C̄ gives the block-triangular matrix
[I_s 0 ; B₂′ I_r], whose determinant is 1. Hence det C̄ = (−1)^{rs}. ∎

**Lemma 2 (eq. 12, VERIFIED).** C̄∇Yₜ = Ȳₜ − H̄Ȳₜ₋₁, with H̄ = diag(0_s, I_r).

*Proof.* C̄∇Yₜ = [∇Yₜ₂ ; ∇Yₜ₁ + B₂′∇Yₜ₂] = [∇Yₜ₂ ; Wₜ − Wₜ₋₁] = Ȳₜ − [0 ; Wₜ₋₁], and
H̄Ȳₜ₋₁ = [0 ; Wₜ₋₁]. ∎

**Lemma 3 (eq. 9, VERIFIED).** ΠYₜ₋₁ = ΛWₜ₋₁ = Λ̄Ȳₜ₋₁, with Λ̄ = [0_{M×s}, Λ].

*Proof.* ΠYₜ₋₁ = ΛB′Yₜ₋₁ = Λ(Yₜ₋₁,₁ + B₂′Yₜ₋₁,₂) = ΛWₜ₋₁ = [0, Λ][∇Yₜ₋₁,₂ ; Wₜ₋₁]. ∎

### 2.2 The operator identity (the cleanest form of the derivation)

Define G(z) := [0 (1−z)I_s ; I_r B₂′]. Then Ȳₜ = G(L)Yₜ.

**Theorem 1 (VERIFIED, analytically and numerically to 2·10⁻¹⁶).** Let

  Φ̄(z) := (I − Σᵢ₌₁ᵖ⁻¹ Fᵢzⁱ) C̄⁻¹ (I − H̄z) + zΛ̄.

Then Φ̄(z)G(z) = Φ(z) identically in z. Expanding Φ̄(z) = Φ̄₀ − Σᵢ₌₁ᵖ Φ̄ᵢzⁱ gives exactly
eq. (16):

  Φ̄₀ = C̄⁻¹, Φ̄₁ = C̄⁻¹H̄ − Λ̄ + F₁C̄⁻¹, Φ̄ᵢ = FᵢC̄⁻¹ − Fᵢ₋₁C̄⁻¹H̄ (2 ≤ i ≤ p−1),
  Φ̄_p = −F_{p−1}C̄⁻¹H̄.

For p = 1, Φ̄₁ = C̄⁻¹H̄ − Λ̄.

*Proof.*

1. (I − H̄z)G(z) = [I_s 0 ; 0 (1−z)I_r][0 (1−z)I_s ; I_r B₂′] = (1−z)[0 I_s ; I_r B₂′]
   = (1−z)C̄. Hence (1−z)I = C̄⁻¹(I − H̄z)G(z).
2. Λ̄G(z) = [0, Λ]G(z) = [Λ, ΛB₂′] = ΛB′ = Π, with no z in it.
3. Φ(z) = (I − ΣFᵢzⁱ)(1−z) + zΠ = [(I − ΣFᵢzⁱ)C̄⁻¹(I − H̄z) + zΛ̄] G(z).
4. Expanding:
   C̄⁻¹ − z C̄⁻¹H̄ − Σᵢ zⁱFᵢC̄⁻¹ + Σᵢ zⁱ⁺¹FᵢC̄⁻¹H̄ + zΛ̄.
   Collecting powers of z gives (16). ∎

This is the same computation as the paper's (premultiply (8) by C̄, substitute (9) and (12),
premultiply by C̄⁻¹). The operator form makes the equivalence of (15) with (5) an identity
between polynomials. The paper's (15) is Φ̄(L)Ȳₜ = Θ(L)Aₜ, since Φ̄(L)Ȳₜ = Φ̄(L)G(L)Yₜ =
Φ(L)Yₜ.

**The normalised form (eq. 18, VERIFIED).** Premultiply (15) by Φ̄₀⁻¹ = C̄ and write
Aₜ = C̄⁻¹A\*ₜ:

  Ȳₜ = Σ Φ\*ᵢȲₜ₋ᵢ + (I − Σ Θ\*ⱼLʲ)A\*ₜ, with Φ\*ᵢ = C̄Φ̄ᵢ, Θ\*ⱼ = C̄ΘⱼC̄⁻¹, A\*ₜ = C̄Aₜ,
  and Var A\*ₜ = Σ\* = C̄ΣC̄′.

The paper states Θ\*ᵢ = Φ̄₀⁻¹ΘᵢΦ̄₀, which is C̄ΘᵢC̄⁻¹ — the same thing. The paper does not
state Σ\*; it is immediate. The sign convention is I − ΣΦ\*ᵢLⁱ and I − ΣΘ\*ⱼLʲ, which is the
engine's convention: residual line 301 of `elfvarma.c` is
aₜ = (wₜ − μ) − Σφⱼ(wₜ₋ⱼ − μ) + Σθⱼaₜ₋ⱼ, and Mauricio (2002) eq. (1) agrees.

Useful closed forms, used below:

* C̄⁻¹H̄ = [0_{r×s} I_r ; 0 0] = [0, E₁].
* Φ̄(1) = F(1)C̄⁻¹(I − H̄) + Λ̄. Since C̄⁻¹(I − H̄) = [B⊥, 0],

  **Φ̄(1) = [F(1)B⊥ , Λ]** (VERIFIED to 1·10⁻¹⁶).

### 2.3 Stationarity of the transformed model — the paper's gap, and a proof

The paper (pp. 3650–3651) replaces a proof by a three-step argument:

1. both components of Ȳ are stationary, so Ȳ is stationary;
2. Ȳ follows a uniquely identified VARMA;
3. hence that VARMA is stationary.

**The gaps:**

* **Step 1.** Marginal stationarity of two blocks does not imply joint stationarity of the
  stacked vector. It holds here because both blocks are linear filters of the same {Aₜ} under
  the Granger representation of an I(1) process, but the paper does not say so.
* **Step 2.** The derivation shows that the *map* from VEC parameters to (Φ̄, Θ) is unique.
  It does not show that the pair (Φ\*, Θ\*) is irreducible (left-coprime). Step 3 is true
  only for irreducible pairs: a stationary process can satisfy a VARMA whose AR operator has
  a unit root cancelled by the MA, e.g. yₜ = aₜ satisfies (1−L)yₜ = (1−L)aₜ. The paper allows
  Θ roots *on* the circle, so this is not excluded by its assumptions on Θ.
* **Supplying the missing step.** If U(z) is a common left factor of Φ̄ and Θ, then
  Φ = Φ̄G = U(Φ̄₁G) and Θ = UΘ₁, so U is a common left factor of (Φ, Θ). Hence left-coprimeness
  of (Φ, Θ), which is part of the paper's identifiability assumption, transfers to (Φ̄, Θ) and
  so to (Φ\*, Θ\*). With that, and the standard fact that the poles of an irreducible
  Φ\*⁻¹Θ\* are the zeros of |Φ\*|, the argument can be completed.

The following proof is shorter and needs none of this.

**Theorem 2 (VERIFIED; numerically, the determinant relation holds to 1.5·10⁻¹⁵).**

  det Φ(z) = (−1)^{rs}(1 − z)^s det Φ̄(z), and det Φ\*(z) = (−1)^{rs} det Φ̄(z).

*Proof.*
* det G(z) = (−1)^{rs}(1−z)^s, by the same block-column permutation as in Lemma 1.
* Apply det to Theorem 1.
* det C̄ = (−1)^{rs}. ∎

**Corollary 2.1 (the exact condition for (18) to be stationary).** The following are
equivalent:

* (a) Φ\*(z) has all roots outside the unit circle;
* (b) |Φ(z)| = 0 has exactly s roots at z = 1, counted with multiplicity, and all other roots
  outside the circle (the paper's "partially nonstationary" assumption);
* (c) Φ(z) has no roots inside the circle, no roots on it except at 1, and
  det Φ̄(1) = det[F(1)B⊥, Λ] ≠ 0 ⇔ det(Λ⊥′F(1)B⊥) ≠ 0, given that Λ has full rank.

*Proof.*
* By Theorem 2 the roots of det Φ\* are the roots of det Φ with s copies of z = 1 removed.
  So (a) ⇔ (b).
* [F(1)B⊥, Λ] is square. Premultiply by the nonsingular [Λ⊥′ ; (Λ′Λ)⁻¹Λ′]. This gives the
  block matrix [Λ⊥′F(1)B⊥ 0 ; ∗ I_r], whose determinant is det(Λ⊥′F(1)B⊥). ∎

So the paper's assumption — D roots at one, together with rank Φ(1) = M − D — is exactly
Johansen's I(1) condition, and it is exactly what makes Φ\* stationary. The engine's AR
check, `ifault = 2` in `cgamma`, is therefore a check of the I(1) condition. Two remarks:

* **Counterexample (VERIFIED).** It shows that the rank condition alone is not enough. Take
  M = 2, r = 1, Λ = (0.4, 0)′, B₂ = 0.5, and F₁ chosen so that Λ⊥′(I − F₁)B⊥ = 0. Then
  rank Π = 1, but |Φ| has 2 unit roots and Φ\* has a root of modulus exactly 1.000000
  (`check_theory.log`). The transformation still holds algebraically; the transformed model
  is not stationary.
* **The Λ = 0 boundary.** At Λ = 0, Φ̄(1) = [F(1)B⊥, 0] is singular. That is THEORY.md
  Thm 7(ii), re-derived here in one line.

**Invertibility (VERIFIED).** det Θ\*(z) = det(C̄Θ(z)C̄⁻¹) = det Θ(z). The MA roots are
unchanged, so invertibility in Ȳ coordinates is invertibility of Θ.

### 2.4 Identification and parameter count

**Proposition 3 (the map is injective, VERIFIED).** The map
(B₂, Λ, F, Θ, Σ) ↦ (B₂; Φ\*₁..Φ\*_p, Θ\*₁..Θ\*_q, Σ\*) is injective. B₂ belongs in the image
because it defines the data Ȳ through W; the likelihood is a function of (B₂, Φ\*, Θ\*, Σ\*).

*Proof.*
* Given B₂, C̄ and G are known.
* Φ(z) = C̄⁻¹Φ\*(z)G(z) (Theorem 1 with Φ\* = C̄Φ̄), which is a polynomial determined by Φ\*.
* From Φ(z), Π = Φ(1) and Fᵢ follow by (3).
* Λ = the first r columns of Π, because B′ = [I_r, B₂′].
* Θ = C̄⁻¹Θ\*C̄ and Σ = C̄⁻¹Σ\*C̄⁻¹′. ∎

This is identification of the *reparameterisation*. Identification of the VARMA on Ȳ from
its spectral density still needs the paper's unstated Yap–Reinsel-type conditions. Neither
the paper nor drvec imposes them in the free class (**OPEN**, as in the paper).

**Count.** The dimension of the rank-r matrices is r(2M − r) = Mr + sr (Λ and B₂). The free
model has Mr + sr + (p−1)M² + qM² + M(M+1)/2 parameters, plus the means:
0 / r / M in cases 1 / 2 / 3. This matches drvec's `par_blocks` (lines 616–655). drvec
carries M(M+1)/2 − 1 parameters for Σ plus a concentrated σ²; see D5 in §6.

### 2.5 The triangular form (Remark 5, eq. 19) — VERIFIED

Ȳₜ = Uₜ gives ∇Yₜ₂ = Uₜ₁ and Wₜ = Uₜ₂. Since Yₜ₁ = Wₜ − B₂′Yₜ₂, this is Yₜ₁ = −B₂′Yₜ₂ + Uₜ₂.
That is eq. (19), with U stationary by Theorem 2. That it is Phillips' triangular form, and
the inference claims that follow, are the other agent's subject.

### 2.6 The normalisation

(6) requires the upper r × r block of the true cointegrating matrix to be nonsingular. The
paper acknowledges this and cites Luukkonen et al. and Kurozumi. If the block is singular,
B₂ has no finite value and the transformation does not exist. Π = ΛB′ is invariant to the
choice. drvec puts the choice in the `.inp` column order; see MODEL.md §5.4. There is no
discrepancy with the paper.

---

## 3. The deterministic terms (Remark 6)

**Theorem 4 (VERIFIED).** Model (20) is Φ̄(L)(Ȳₜ − m) = Θ(L)Aₜ with m = E[Ȳ] = (δ′, m_W′)′,
where δ = E∇Y₂ and m_W = E W. It is equivalent to the VEC with an intercept,

  ∇Yₜ = μ − ΛB′Yₜ₋₁ + ΣFᵢ∇Yₜ₋ᵢ + Θ(L)Aₜ, with μ = Φ̄(1)m = F(1)B⊥δ + Λm_W.

The map m ↦ μ is a bijection ℝᴹ → ℝᴹ, because Φ̄(1) is nonsingular (Corollary 2.1).
Equivalently:

  ∇Yₜ − γ = −Λ(B′Yₜ₋₁ − m_W) + ΣFᵢ(∇Yₜ₋ᵢ − γ) + Θ(L)Aₜ, with γ = E∇Y = B⊥δ = (−B₂′δ ; δ).

*Proof.* Φ̄(L)Ȳ = Φ̄(1)m + ΘA, and Φ̄(L)Ȳ = Φ(L)Y. Take Φ̄(1) from §2.2. Then B⊥δ = γ,
because ∇Y₁ = ∇W − B₂′∇Y₂ and E∇W = 0. ∎

Hence the paper's three cases are these (all VERIFIED; the case-3 simulation gives a sample
mean of Ȳ of (0.0526, −0.0212, 1.5076) against m = (0.05, −0.02, 1.5), with n = 2·10⁵):

| case (paper = drvec) | E∇Y₂ | E W | equivalent VEC intercept μ | Johansen (1995) |
|---|---|---|---|---|
| 1 | 0 | 0 | μ = 0 | H₂ — no deterministic terms |
| 2 | 0 | free (r) | μ = Λm_W (restricted constant, "αρ₀′") | H₁\* |
| 3 | free (s) | free (r) | μ free (M); drift γ with B′γ = 0, i.e. E∇Y₁ = −B₂′E∇Y₂ | H₁ |

The case-2 row follows from μ = Λm_W when δ = 0. The case-3 constraint E∇Y₁ = −B₂′E∇Y₂ is
the paper's own statement of case 3, re-derived.

Johansen's H\* (a trend restricted to the cointegrating space) and H (an unrestricted trend,
which gives quadratic trends in levels) are **not covered** by the paper or by drvec. drvec
only has `-interv`, which subtracts univariate deterministic terms estimated beforehand
(see D8).

**Code.** The mean is `vec_shootx` lines 3803–3811 (`mu = [E∇Y₂ ; E W]`, zero blocks by
case), passed as `armax->mu` (line 4068). The engine subtracts it from wₜ, as in (20).
`-mean` without `-case` promotes case 1 to case 2 (line 8167). **drvec's case numbering is
the paper's**; the default is case 1, while the paper's own example uses case 2 (eq. 25).
The critical values for `-lrtest` are case 1 → "none" and case 2 → "restricted constant";
there is no table for case 3 (lines 7344–7353). That is consistent with the table above.

*Reporting defect (LOW).* The `.out` prints the model as
"∇Yₜ = α(β′Yₜ₋₁ − E[W]) + Γ₁∇Yₜ₋₁ + … + Θ(L)Aₜ" for every case (line ~6378). In case 3 the
correct display, by Theorem 4, carries the drift: ∇Yₜ − γ = α(β′Yₜ₋₁ − E W) + ΣΓᵢ(∇Yₜ₋ᵢ − γ)
+ … The estimated model is right, because the engine demeans Ȳ; only the printed equation
omits γ.

---

## 4. The likelihood and the sample

**What the paper maximises.** The exact Gaussian likelihood of the stationary VARMA (18) with
mean (20), evaluated on the observed Ȳₜ. Ȳ needs ∇Yₜ₂, so Ȳ exists for t = 2…N. In the
example, 1850–1911 gives 62 years and the paper's "N = 61 effective observations" (Fig. 2).

**Proposition 5 (unit Jacobian — omitted by the paper, VERIFIED).**

* The map (Y₁,₂, W₁, Ȳ₂, …, Ȳ_N) ↔ (Y₁, …, Y_N) is linear and triangular.
* Its diagonal blocks are C̄ (for each t ≥ 2) and the r + s block [I_r B₂′ ; 0 I_s]. All
  have |det| = 1. Hence dY = dȲ and there is no Jacobian term.
* This matters for ML over B₂. The transformation of the data depends on B₂, and a
  B₂-dependent Jacobian would bias the estimator. It does not arise.

**Exactly which likelihood (VERIFIED).**

  L(θ) = f(Ȳ₂, …, Ȳ_N; θ), the stationary marginal density of N − 1 vectors.

Under the model, f(Y₁..Y_N) = f(Y₁,₂, W₁, Ȳ₂..Ȳ_N). L is obtained from it by integrating out
W₁ and Y₁,₂. With the usual assumption that the stochastic-trend origin Y₁,₂ is independent
of the stationary path (fixed or diffuse), L = f(Y₂..Y_N | Y₁,₂) with W₁ marginalised.

It is **not** f(Y₂..Y_N | Y₁): the observable, stationary W₁ carries information about Ȳ₂
that is discarded. A likelihood using (W₁, Ȳ₂..Ȳ_N) would use r more numbers. Neither the
paper nor drvec does this, so there is no discrepancy. drvec's docs say the fit "conditions
on Y₁₈₅₀". That is loose language: the first observation is only used to form ∇Y₂,₁₈₅₁.

**drvec.**

* `build_y2_levels` (lines 535–600), default layout: datamat[t] = (Y₂,ₜ₊₁ − Y₂,ₜ, Y₁,ₜ₊₁) and
  Y2_levels[t] = Y₂,ₜ₊₁ for t = 1..n−1. `vec_shootx` [5] (lines 4082–4099) sets
  w = (∇Y₂, Y₁ + B₂′Y₂). That is Ȳ₂..Ȳ_n, the paper's sample. It matches the independent
  construction to ≤ 1.4·10⁻¹⁴.
* `est`/`elf` maximise the likelihood of Ȳ with Σ\* = σ²·C̄QC̄′, Q₁₁ = 1, and σ² concentrated.
  The objective is (f₁/f₁₀)^m (f₂/f₂₀), where f₁ is the quadratic form and f₂ = |Ω|^{1/n}.
  The reported value is logL = −½mn(log2π − log mn + 1) − ½n(m log f₁ + log f₂). This is the
  exact log-likelihood maximised over σ²; see D5.
* **Numerically (VERIFIED).** The concentrated log-likelihood from the direct Toeplitz
  evaluation equals `elf`'s with `xitol < 0` to 2.4–4.0·10⁻⁸ in all 7 configurations. The
  difference is exactly ½·mn·(log 2π − 1.837877066) — the truncated constant — e.g.
  ½·177·4.09·10⁻¹⁰ = 3.62·10⁻⁸. The un-concentrated value at Σ\* = qq agrees to the same
  digits.
* With the default `xitol = +1e-3` (`-m 1`) the engine truncates the ξ sequence. The error
  against the exact value is up to 4.6·10⁻³ in logL at Σ\* = qq, and 1.3·10⁻³ concentrated,
  in these random configurations. It is exactly 0 when q = 0 (config F). This is the engine's
  only approximation, and it bears on D2.
* The `.out` header "Sample : 61 observations from 1850" prints the raw start year. The first
  Ȳ is 1851 in the levels layout (`ybar_start_date`, line 2654, gets this right and is used
  elsewhere). Cosmetic, **LOW**.

---

## 5. Equation ↔ code map (`src/drvec.c`)

Line numbers are at HEAD `5aebd28`. The "definition at ~509" in the file map is only the
prototype; the only definition of `vec_shootx` is at 3666.

| quantity | paper | code | status |
|---|---|---|---|
| parameter vector layout: mean, Λ, F, Θ, Σ(−1), B₂ | Remarks 1, 6 | `par_blocks` 616–655, `calc_nparametrs` 657, unpack in `vec_shootx` 3802–3946 | VERIFIED (harness consumes `npar` exactly, 7 configs) |
| m = E[Ȳ] by case | (21), cases 1–3 | 3803–3811, 4068 | VERIFIED (Δ = 0) |
| Λ (M × r), sign −Λ(B′Y − E W) | (5), (25) | 3813–3836; seed Λ = −coef on W_{t−1} (init_guess) | VERIFIED; reported as α = −Λ (report ~6430) |
| Fᵢ | (5) | 3838–3854 | VERIFIED |
| Θⱼ, free class | (5) | 3856–3895 (`else` branch) | VERIFIED (Δ = 0) |
| Θⱼ, default `-marow` | — (not in paper) | 3879–3885, default set at 8169–8185 | **CONFIRMED DISCREPANCY D1** |
| Σ, Q₁₁ = 1 | Σ = Var Aₜ | 3897–3939 | VERIFIED equivalent (D5) |
| B₂ (s × r) | (6) | 3942–3946 | VERIFIED |
| C̄ | (11) | 3977–3980 | VERIFIED (Δ = 0) |
| C̄⁻¹ | (14) | 3981–3984 | VERIFIED |
| H̄ | (13) | 3985–3986 | VERIFIED |
| Λ̄ | (10) | 3987–3988 | VERIFIED |
| Φ̄₀…Φ̄_p | (16) | 3990–4035 (p = 1 handled) | VERIFIED |
| Φ\*ᵢ = C̄Φ̄ᵢ | (18) | 4037–4045 | VERIFIED (Δ = 0) |
| Θ\*ⱼ = C̄ΘⱼC̄⁻¹ | (18) | 4046–4056 | VERIFIED (Δ = 0) |
| Σ\* = C̄ΣC̄′ | implied by (18) | 4057–4067 | VERIFIED (Δ = 0) |
| Ȳₜ = (∇Y₂ₜ′, Wₜ′)′ | (17) | `build_y2_levels` 535–600, `vec_shootx` 4082–4099, `build_ybar` 1115 (same construction) | VERIFIED (≤ 1.4e−14) |
| exact likelihood of (18) | Remark 1 | engine `est`/`elf`, xitol 3693 | VERIFIED (2–4e−8, the LOG2PI constant); CML: D2 |
| `-warma` Φ\*, Θ\*, Σ\* written directly | — | 3708–3780; inverse `warma_inverse` 741 | VERIFIED equivalent to a paper subclass (D6) |
| levels to forecast | inverse of (17) | `forecast_core` 4561, `level_error_map` 4536 | VERIFIED analytically (below) |
| B₂ seed (Johansen canonical) | Remark 1 step 1 | `canonical_b2` 877, `prelim_b2` 1070 | starting values only; normalisation on the upper r × r block is correct |
| rank condition σ_min(Λ⊥′Θ(1)B⊥) | not in paper | `granger_smin` 390 | reporting only unless `-rankadm`; outside the paper |

**Forecasting (VERIFIED analytically).**

* `forecast_core` runs the recursion of (18) on Ȳ with mean m. It uses the engine's residuals
  â = E[A\*ₜ | Ȳ₁..Ȳₙ], which `cres` returns rescaled by the Cholesky factor of qq. That is
  the correct finite-sample predictor E[Ȳₙ₊ₕ | data].
* It returns to levels through Y₂ = cumulated ∇Y₂ from the observed Y₂,ₙ and
  Y₁ = W − B₂′Y₂, which is the inverse of G.
* `level_error_map` gives the level error for horizon h as
  Σₘ Gₘ A\*ₙ₊ₕ₋ₘ, with Gₘ = [Cₘ(1:s, ·) ; Ψₘ(s+1:M, ·) − B₂′Cₘ(1:s, ·)] and Cₘ = Σ_{k≤m}Ψₖ.
  This is exactly the MA representation of (Y₂ₙ₊ₕ, Y₁ₙ₊ₕ) implied by the inverse of G.
* The bands ignore the finite-sample uncertainty of the pre-sample shocks, which is standard.

---

## 6. Discrepancies

### (a) Deliberate departures

**D1 — the default MA class `-marow`: Θₖ = [T₁₁ T₁₂ ; 0 0]. CHANGES THE MODEL. Severity: HIGH.**

*What the code does.* `parse_cli`, lines 8169–8185: if q > 0 and no class flag is given, it
sets `global_marow = 1`. `vec_shootx` (3879–3885) then fills only the first r rows of each
Θₖ, the Y₁ equations in the internal order. The s rows of the ∇Y₂ equations are zero. This
is active for r ≥ 1 (`marow_on`, line 316).

*What that is.* A proper subclass of the paper's model: q·s·M fewer MA parameters. In Ȳ
coordinates Θ\*ₖ = [0 0 ; T₁₂ − T₁₁B₂′  T₁₁], so the ∇Y₂ block of the transformed system has
no MA at all (checked algebraically; config D numerically).

*Not w.l.o.g. — counterexample.* M = 2, r = 1, with Y₂ an IMA(1,1) (∇Y₂ₜ = a₂ₜ − θa₂ₜ₋₁)
and W stationary. The ∇Y₂ row of Θ₁ is (0, θ) ≠ 0 in the VEC form. No finite-order `-marow`
model reproduces it exactly.

*The paper's own example is outside the default class.* In Mauricio's Tables 4 and 5 the
muskrat equation — Y₂, the nonstationary series, in both the normalisation B = [1, β₂]′ and
drvec's `.inp` — has Θ̂₁ row (−0.8953, −0.0174) (Table 4, s.e. 0.2678) and (−0.6039, −0.1837)
(Table 5). The default drvec fit forces that row to zero, so it cannot estimate the model
the paper reports. The repository's own golden tests reproduce the paper's example with
`-mafree` (`tests/run_tests.sh` 498–504), which confirms that `-mafree` is the paper's model.

*Consequence for the rank test (CONFIRMED by running it).* At r = 0 the structured classes
are switched off (line 316), so Θ is free with M² parameters per lag. At r ≥ 1 the default
gives rM per lag. The r = 0 model is therefore not the Λ → 0 limit of the r = 1 model, and
the hypotheses are **not nested**. The code's comment at 7330ff ("rank r is nested in r+1")
is false under the default.

On `mink_muskrat.inp`, `-case 2 -lrtest` gives:

| MA class | npar r = 0 | npar r = 1 | logL r = 0 | logL r = 1 | LR |
|---|---|---|---|---|---|
| default | 10 | 12 | −6.3311 | 2.3040 | 17.27 |
| `-mafree` | 10 | 14 | −6.3311 | 6.4786 | 25.62 |

The conclusion happens to be the same here; the statistic is not. The paper's Table 3
procedure uses the same MA specification at every rank.

*Justification in the docs.* THEORY.md Cor. 6.2 and DEMOSTRACIONES Cor. 6.3 say that in this
class Θ(1) is invertible iff the r × r block is, so the rank condition holds automatically.
That is a reason to prefer the class. It does not make the class equal to the paper's model.
To fit the paper's model: `-mafree`.

**D2 — `-m 1|2` and the paper's CML. CHANGES WHAT IS ESTIMATED under `-m 2`, and the report
is WRONG. Severity: MEDIUM.**

* `met` only sets `xitol = ±1e-3`. Line 3693 and 13 other sites; there is no other use.
* In `elf`, xitol (`delta`) is used only by `cxi`, which truncates the ξ recursion when
  Σ|ξ| < xitol for q + 1 consecutive lags. A negative xitol never truncates.
* So `-m 1` is exact ML with a 10⁻³ truncation, which is the default and is labelled "Exact".
  `-m 2` is exact ML with no truncation, which is *more* exact.
* The `.out` labels `-m 2` "Conditional (Approximate) Maximum Likelihood" (line 8419) and the
  README "approximate ML".
* The paper's Remark 4 CML is not implemented anywhere. Every "CML" column in the paper is
  therefore unreachable with drvec.
* Measured: the default's truncation moves logL by up to 4.6·10⁻³ at a fixed point (§4).
  external_review.md reports a stop point that depends on it: B̂₂ = −0.248 against −0.205.

This does not change the *model*. It changes the *objective* by an approximation, and the
label is wrong. It is already recorded as H1/H2 in `docs/external_review.md`; it is confirmed
here from the code.

**D3 — levels layout (default) vs `-differenced` (legacy). Equivalent in cases 2 and 3,
different in case 1.**

* *Default.* The paper's Ȳ₂..Ȳₙ exactly (§4).
* *`-differenced`.* The s columns arrive as ∇Y₂. `Y2_levels[1] = 0` and
  `Y2_levels[t] = Σ_{u=2..t} ∇Y₂,ᵤ` (lines 580–586). Hence W^leg_t = Wₜ − B₂′c with c the
  (unknown) level of Y₂ at the first row. Numerically (config G), W − W_true is constant at
  2.174 in the W column to 9·10⁻¹⁶.
* **Proposition (VERIFIED).** In cases 2 and 3, L(θ, m_W; Ȳ^leg) = L(θ, m_W − B₂′c; Ȳ). So
  the maximised likelihood and B̂₂, Λ̂, F̂, Θ̂, Σ̂ are identical; only Ê[W] shifts by −B₂′c.
  *Proof.* The engine uses Ȳ − m, and Ȳ^leg − m = Ȳ − (m + (0, B₂′c)). The map
  m_W ↦ m_W + B₂′c is a bijection of the free parameter m_W at each B₂. ∎
* In case 1, m_W = 0 is fixed, so the shift −B₂′c enters as a B₂-dependent non-zero mean.
  That is a different model, and B₂ is contaminated. The code warns (line 8328); it does not
  refuse.

**D4 — sign conventions. EQUIVALENT.** Internally the parameters are the paper's
(−Λ(B′Y − E W), Λ as in Tables 4 and 5). The report converts to α = −Λ, β = B, Γᵢ = Fᵢ,
Π_J = −ΛB′ (lines ~6365–6470). The row order is mapped by `inp2lam` (2299), which is correct.
Since ΛB′ = (−Λ)B′ with B's top block +I, (α, β) = (−Λ, B) is the only consistent
assignment.

*Documentation inconsistency (LOW).*
* MODEL.md §1 says "Λ is Johansen's α and B is his β with the opposite sign", and §5.3 says
  B₂ = −β.
* THEORY.md §1 and the code say α = −Λ, β = B.
* MODEL.md's statement is right only if "β" means the coefficient of the regression
  Y₁ = β′Y₂ + u.

**D5 — Σ = σ²Q with Q₁₁ = 1 and σ² concentrated. EQUIVALENT.**
* Σ ↦ (σ² = Σ₁₁, Q = Σ/Σ₁₁) is a bijection of the PD cone onto ℝ₊ × {Q PD, Q₁₁ = 1}.
* Σ\* = σ²C̄QC̄′.
* The engine maximises over σ² analytically (Mauricio 2002, §1: "any decomposition of
  E[AA′] is valid").
* So the maximum and the maximiser of Σ coincide with the paper's. Verified: the
  concentrated values agree with the direct evaluation (§4).
* Standard errors are for Q's entries, not Σ's; that is inference, outside this study.

**D6 — `-warma` (not default). EQUIVALENT to a paper subclass (VERIFIED).**
* `-warma` writes Φ\*ₖ = [0 cₖ] (first s columns zero), Θ\*ₖ = diag(0, Tₖ) and Σ\* free,
  with B₂ entering only through W.
* **Proposition.** This set is exactly the image of the paper's model restricted to
  {Fᵢ = MᵢB′ for all i, Θₖ = [T₁₁ T₁₁B₂′ ; 0 0]}, under a bijection with closed-form inverse:
  - Dₖ = C̄⁻¹cₖ, Λ = E₁ − ΣDₖ;
  - M₁ = Λ − E₁ + D₁, Mₖ = Mₖ₋₁ + Dₖ;
  - consistency: M_{p−1} = −D_p holds automatically.
* *Proof.* Φ\*(z)G(z) = G(z) − Σzᵏcₖ[I_r, B₂′]. Hence
  Φ(z) = (1−z)I + z E₁B′ − Σzᵏ DₖB′, and z E₁B′ is the H̄ part. Match with
  (1−z)I + zΛB′ − ΣMᵢB′zⁱ(1−z) coefficient by coefficient. ∎
* Checked numerically to 1·10⁻¹⁶ (`check_theory.log` §4). `warma_inverse` solves the same
  system by least squares, which is correct, since the residual is zero on the image.
* Parameter counts agree: pMr for the AR in both.

**D7 — other restrictions** (`-mawarma`, `-matri`, `-diagar`, `-diagma`, `-diagcov`, `-alpha`,
`-fixb2`). Each is a restriction on the parameters of (5)–(6) or of (18), exactly the use
Remark 2 licenses. None is a default. They change the model only when asked for.

**D8 — `-interv`.** Deterministic terms estimated beforehand in univariate `fue` models are
subtracted from the levels before Ȳ is formed (lines 1219ff). The paper's Remark 2 suggests
estimating them *jointly*. This is a two-step approximation, declared as such in the code.
It changes the estimator, not the VEC model.

**D9 — r = 0 is allowed.** Then C̄ = I, H̄ = 0 and Φ̄_p = 0: a VARMA(p−1, q) on ∇Y. This is
the natural limit and outside the paper's 0 < P < M. Correct.

### (b) Places where the code is simply wrong (none in the transformation)

| # | what | evidence | severity |
|---|---|---|---|
| W1 | `-m 2` labelled "Conditional (Approximate) ML"; it is untruncated exact ML; CML absent | lines 8419, 3693; `elf` uses xitol only in `cxi` | MEDIUM |
| W2 | `-lrtest` under the default MA class compares non-nested models | §6 D1, run above: LR 17.27 vs 25.62 | MEDIUM |
| W3 | printed VEC equation omits the drift γ in case 3 | line ~6378 vs Theorem 4 | LOW |
| W4 | "Sample: … from 1850": the first Ȳ is 1851 in the levels layout | main, "Sample" line; `ybar_start_date` | LOW |
| W5 | `-differenced` with case 1 is estimated (with a warning) although it is a different model | D3 | LOW (warned) |
| W6 | MODEL.md §1/§5.3 sign statement contradicts THEORY.md and the code | D4 | LOW (docs) |

**OPEN.** The `.out` says "Exact Unconditional ML, Algorithm AS 311 (Mauricio 1997)". The
engine's own header (`elfvarma.c`) cites Mauricio (1995, JASA 90); AS 311 is the published
implementation of that likelihood. Whether the vendored `elfvarma.c` *is* AS 311 or the
earlier 1995 code could be settled by diffing against the AS 311 Fortran/C. It does not
affect the numbers, which match the direct evaluation.

---

## 7. Errors and gaps in the paper itself

1. **The stationarity argument** (pp. 3650–3651) is incomplete (§2.3). Step 1 asserts joint
   stationarity from marginal stationarity. Step 2 conflates uniqueness of the map with
   irreducibility of (Φ\*, Θ\*). The paper allows MA roots on the unit circle, so step 3 needs
   irreducibility.
   *Fix:* Theorem 2, det Φ\*(z) = ±det Φ(z)/(1−z)^s, which is a complete proof and gives the
   sharp condition (Corollary 2.1: exactly s unit roots ⇔ det(Λ⊥′F(1)B⊥) ≠ 0).
2. **The hypotheses are stated in two pieces** — D unit roots, and rank Φ(1) = M − D — and
   their conjunction is exactly what is needed. A reader could take the rank condition alone
   as sufficient. It is not: see the counterexample, where Φ\* gets a unit root.
3. **The Jacobian is not mentioned.** It is 1 (Proposition 5), and that is what makes ML over
   a data transformation that depends on B₂ legitimate. It should be stated.
4. **"Exact" is exact for the marginal of Ȳ₂..Ȳ_N** — that is, conditional on the
   common-trend origin, with W₁ discarded (§4). This is not an error, but it is not the full
   f(Y₁..Y_N), which does not exist for an I(1) process without an initial-condition
   assumption. The paper does not say which it is.
5. **Σ\* = C̄ΣC̄′** is not written out (trivial).
6. **"Uniquely identified" (pp. 3649–3650)** is identification of the reparameterisation
   (Proposition 3). Identification of the VARMA on Ȳ still rests on the assumed Yap–Reinsel
   conditions. It is not proved or imposed by the transformation.
7. **Remark 6** gives the means but not the equivalent VEC intercept. μ = Φ̄(1)m =
   F(1)γ + Λm_W (Theorem 4) is what places cases 1–3 among Johansen's H₂, H₁\* and H₁. The
   paper asserts the correspondence, and it is right.

No derivation in Section 3 is wrong. Eqs. (9)–(18) were redone symbolically and checked
numerically to machine precision.

---

## 8. Numerical record (from `check_code.log`)

| cfg | M | r | p | q | case | MA | layout | Φ\* | Θ\* | Σ\* | μ | Ȳ | logL: `elf`(xitol < 0) − direct | `elf`(xitol = +1e−3) − direct (concentrated) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A | 3 | 1 | 3 | 2 | 3 | free | levels | 0 | 0 | 0 | 0 | 1.4e−14 | +3.62e−8 | −1.27e−3 |
| B | 3 | 2 | 2 | 1 | 2 | free | levels | 0 | 0 | 0 | 0 | 0 | +3.62e−8 | +2.44e−4 |
| C | 2 | 1 | 1 | 1 | 1 | free | levels | 0 | 0 | 0 | 0 | 0 | +2.42e−8 | +4.33e−5 |
| D | 3 | 1 | 2 | 1 | 3 | marow | levels | 0 | 0 | 0 | 0 | 1.4e−14 | +3.62e−8 | +1.42e−3 |
| E | 4 | 2 | 3 | 1 | 3 | free | levels | 0 | 0 | 0 | 0 | 7.1e−15 | +4.01e−8 | +7.49e−5 |
| F | 2 | 1 | 2 | 0 | 2 | — | levels | 0 | 0 | 0 | 0 | 0 | +2.42e−8 | +2.42e−8 (no MA ⇒ no truncation) |
| G | 3 | 1 | 2 | 1 | 2 | free | `-differenced` | 0 | 0 | 0 | 0 | const. shift only | +3.62e−8 | −4.82e−5 |

The +2–4e−8 column is the analytically predicted ½·mn·(log 2π − 1.837877066). In the Φ\*
through μ columns, "0" means a bit-for-bit match (max abs difference 0.0).

The identities (Lemma 1, Theorem 1, Φ̄(1) = [F(1)B⊥, Λ], Theorem 2, det Θ\* = det Θ,
|det C̄| = 1) hold to ≤ 1.5·10⁻¹⁵ in 5 configurations. Using the companion count of
`check_theory.py`, the number of unit roots of |Φ| equals s in every draw.

One random draw (M = 3, r = 2, p = 2 in `check_theory.log`) has an extra root of |Φ| inside
the circle. As Corollary 2.1 predicts, its Φ\* is explosive: the largest companion modulus is
1.041. So the transformed model's stationarity tracks the *non-unit* roots of Φ exactly. The crude count
printed by `check_code.identities` for M = 4 is a numerical artefact of polynomial fitting
and is superseded.

# STUDY M3 — Line-by-line verification of `drvec/docs/DEMOSTRACIONES.md` and of the code that rests on it

Scope: drvec's own theorems (T1, C1.1, C1.2, T2, T3, C3.1, C3.2, T2′, Lemma A, T4, T5,
T6, C6.1, C6.2, C6.3, T6b, T7, T8, T9, P1, P2, T10, T11, Identification). Mauricio's paper
derivations and the AddOn are covered elsewhere. Engine internals (elfvarma.c, drvmlest.c)
are not reviewed, only the conditions they are called with.
Everything here is read-only against the repositories. Scripts, runs and a scratch rebuild
live in this directory:

| script | what it checks |
|---|---|
| `s1_algebra.py` | T1 identity, T2′ determinant identity, (c) ⇔ AR gate, T2 residue |
| `s2_granger_G.py` | T3 vs `granger_smin()`: counterexample in the default class, `-mawarma` G = Λ⊥′B⊥; writes `dgp_G0.txt` |
| `run/g0.*` | the binary on data from the s2 counterexample: prints "DENIES THE RANK" at an admissible DGP |
| `s3_gate_C63.py` | C6.3 (1)(2) algebra; gate-admitted/inadmissible and gate-rejected/admissible points; T4 covariance |
| `s4_triangular.py` | T6/T6b direct direction (simulation, exact), BVECM converse counterexample, C6.1 table (sympy), C6.2 |
| `s5_factorisation.py` + `run/fac.*` + `run_xi/` + `build_xitol/` | T9 against an independent exact Gaussian likelihood; the gate at xitol = 1e−3 and 1e−10 |
| `s6_cvals.py` | which deterministic case the `-lrtest` tables belong to |
| `s7_coprime.py` | the rank condition of T3 = left-coprimeness of (Φ, Θ) at z = 1 |

---

## 0. Summary table

| Result | Tag | Verdict |
|---|---|---|
| Model §0, class 𝒞 = (a)(b)(c) | — | **DEFINITION INCOMPLETE**: (a)–(c) do not imply rank r; the rank condition (d) is missing (inconsistent with THEORY.md Def. 2 and with T3/T4) |
| T1 | standard | PROOF CORRECT (bijection *onto its image*) |
| C1.1 | new | PROOF CORRECT |
| C1.2 | new | PROOF CORRECT (trivial) |
| T2 | standard | CORRECT WITH A GAP: simple pole asserted, not derived; C verified rather than derived (supplied §2) |
| T3 | new | PROOF CORRECT — **CODE DOES NOT IMPLEMENT IT**: `granger_smin` computes σ_min(Λ⊥′Θ(1)**B⊥**), a different (strictly stronger) condition |
| C3.1 | new | PROOF CORRECT; its code remark is false (det Θ(1) ≠ 0 does not make the code's G non-singular) |
| C3.2 | new | CORRECT WITH A GAP: item 3 "generically"/"lives in col(Λ)" imprecise (exact version supplied) |
| T2′ | standard | CONCLUSION CORRECT, PROOF HAS A GAP (appeals to identification, which fails exactly on the fatal surface); direct proof via det Φ̄(x) = ±det Φ(x)/(1−x)^s supplied |
| Lemma A | standard | Consistent with the engine; tolerance figures data-dependent (see T9) |
| T4 | new | CORRECT WITH A GAP: "not in 𝒞" contradicts the §0 definition of 𝒞; finiteness argument informal (rigorous one supplied); gate description slightly wrong |
| T5 | new | CORRECT WITH A GAP (via T2′); "invisible" means "not rejected", and it holds in **every** MA class, not only the free one |
| T6 | new | Direct direction CORRECT (verified to 1e−14); converse CORRECT algebraically; **necessity has a gap** (needs identification/uniqueness of the VEC representation); BVECM converse is indeed false (counterexample) |
| C6.1 | new | PROOF CORRECT (sympy) |
| C6.2 | new | Statement correct; proof misprints Θ(L)'s cross block (harmless) |
| **C6.3** | new | Items (1)(2) CORRECT; item (3) true only for *strict* invertibility; **"the gate IS the admissibility condition" and "𝒫∖𝒞 not reachable in this class" are FALSE** (counterexamples both ways) |
| T6b | BVECM | PROOF CORRECT; the Θ̃ reparametrisation is drvec's own (BVECM Cor. 2 stops at ε_t) |
| T7 | new | CORRECT (minor: r unit roots, not "a root"; the Λ = 0 null is non-standard because of unit roots/unidentified B₂, not a "boundary" in Chernoff's sense) |
| T8 | cited | **STATEMENT MISMATCHES SOURCE** (garbled extra term; source is trace r vs m, code is λ-max r vs r+1) and **CODE OUTSIDE THE SOURCE'S CONDITIONS** (default MA class makes r=0 vs r=1 non-nested; restricted MA changes with r; case-1 critical values belong to the unrestricted-constant model) |
| T9 | new | PROOF CORRECT (confirmed with an independent exact likelihood); code's tolerance "= xitol" is not a bound: gate says NOT VERIFIED on a correct model |
| P1 | measured + structural | Informal but sound as heuristics |
| P2 | new | CORRECT WITH A GAP (conditioning set: program conditions on Ȳ₂…Ȳₙ, not on Y₁…Yₙ; exact only with Y₁ treated as fixed); code map G_m matches |
| T10 | cited | Statement omits Phillips' explicit hypothesis Ω = 2πf(0) > 0 (Thm 1′) and overstates "exact likelihood required" (Remark m) |
| T11 | cited | Matches Johansen 1991 Thm 5.1, **but that theorem is for the VAR**; the VARMA result is Yap–Reinsel 1995 Thm 2 |
| Identification §7 | cited | Quotes correct; **item 2 is FALSE**: the rank condition of T3 *is* left-coprimeness at z = 1 (s7), so it is implied by the assumption Yap–Reinsel and Mauricio state |

---

## 1. §0 — the class 𝒞

DEMOSTRACIONES defines 𝒞 by (a) |Θ(x)| ≠ 0 for |x| < 1, (b) |Φ(x)| has exactly s unit
roots and the rest outside, (c) det(Λ⊥′F(1)B⊥) ≠ 0, and says 𝒞 is "the set for which the
model is truly I(1) of rank exactly r". That is false for the stated (a)–(c): (a) allows
MA roots **on** the circle, and T3 itself shows that under (a)–(c) the rank is r **iff**
rank(Λ⊥′Θ(1)) = s. The points of T4 (det Θ(1) = 0, null in col(Λ⊥)) satisfy (a)(b)(c).
THEORY.md Def. 2 defines 𝒞 semantically ("I(1) with rank exactly r"), which is the one
T3/T4 need.

**Fix.** 𝒞 := {(a),(b),(c),(d)} with (d) rank(Λ⊥′Θ(1)) = s, or equivalently (s7)
(d′) rank[Φ(1) Θ(1)] = M. Note also that (c) is redundant given (b) and rank Λ = rank B = r
(local Smith form: the zero of det Φ at 1 has order equal to its nullity s ⇒ all partial
multiplicities are 1 ⇒ simple pole ⇔ (c)). Numerically, s1 shows Φ̄(1) = [F(1)B⊥, Λ], so
det Φ̄(1) ≠ 0 ⇔ (c).

Also, Def. 1 of 𝒫 in THEORY.md ("roots of |Φ| on or outside") is looser than the engine:
the engine's AR gate (`cgamma` fails, elfvarma.c:109; Cholesky of Ω fails, :233–238)
requires Φ* strictly stationary, which by the identity of §5 means (b) **and** (c) hold.
So the only way the engine's admitted set exceeds 𝒞 is through Θ (the MA gate), which is
consistent with what the rest of the document says.

## 2. Theorem 1, Corollaries 1.1–1.2, Theorem 2

**T1** (standard). Checked in `s1_algebra.py` in a stronger form: with D(L) =
diag((1−L)I_s, I_r), the doc's Φ̄_k satisfy **Φ̄(L) D(L) C̄ = Φ(L)** identically (max error
9e−15 over 400 random (M, r, p) draws). That is Ȳ = D(L)C̄Y. The "bijection" is a
bijection onto its **image** (a proper subset of VARMA(p,q) on Ȳ). Recovering F₁ from Φ̄₁
alone is not possible, since Λ and F₁ both enter Φ̄₁. The last equation Φ̄_p = −F_{p−1}C̄⁻¹H̄
is what pins down Λ, as `warma_inverse` does. PROOF CORRECT.

**C1.1, C1.2.** Similarity preserves the determinant. C1.2 is a tautology. CORRECT.

**T2** (Granger with MA). Conclusion right (standard Johansen form with C Θ(1)). Gaps:
1. "Φ(x)⁻¹ has a simple pole at 1 by (b)". (b) gives the *order of the zero of det Φ* at 1,
   not the pole order. Supplied: rank Φ(1) = r ⇒ nullity s; the local Smith form at z = 1
   has s invariant factors (1−z)^{k_i}, k_i ≥ 1, with Σk_i = order of the zero = s ⇒ all
   k_i = 1 ⇒ simple pole.
2. C is *checked* (it satisfies (∗)), not *derived*. Supplied uniqueness: the residue R
   satisfies Φ(1)R = 0 and RΦ(1) = 0 ⇒ R = B⊥KΛ⊥′. The O(1) term gives F(1)R + ΛB′H₀ = I.
   Premultiplying by Λ⊥′ gives Λ⊥′F(1)B⊥K = I ⇒ K = (Λ⊥′F(1)B⊥)⁻¹.
3. (a) is not needed for T2.

`s1_algebra.py` checks the residue numerically (1−x)Φ(x)⁻¹ → C (relative error O(ε)).
Verdict: CORRECT WITH A GAP (both pieces supplied).

## 3. Theorem 3 and the code — the central finding

**Statement and proof.** Cointegrating space = {b : b′CΘ(1) = 0}. Its dimension is
M − rank(Λ⊥′Θ(1)), because C = B⊥KΛ⊥′ with B⊥K of full column rank. The proof is correct.
Both directions hold: b′CΘ(1) ≠ 0 ⇒ b′Y contains a random walk with variance ∝ t, since
Σ > 0. PROOF CORRECT.

**The code computes something else.** `granger_smin` (drvec.c:390–471) builds
B⊥ = [−B₂′; I_s] (l. 412), orthonormalises it, and returns σ_min of

  G = Λ⊥′ Θ(1) **B⊥**   (s × s, drvec.c:437–452).

The theorem's matrix is Λ⊥′Θ(1) (s × M). Since B⊥ has full column rank, G non-singular ⇒
condition (d), but **not conversely**. The code comments (l. 344–368, 6659–6662) justify G
as "the one that appears in Granger's representation, C(1) = B⊥(Λ⊥′ΓB⊥)⁻¹Λ⊥′Θ(1)". But
B⊥ does not multiply Θ(1) on the right in that formula. The long-run matrix
CΘ(1) = B⊥KΛ⊥′Θ(1) has the rank of Λ⊥′Θ(1).

**Counterexample (s2, in the DEFAULT class `-marow`).** M = 2, r = 1, p = 1, q = 1:
Λ = (0.5, −0.2)′, B₂ = −1, Θ₁ = [[0.5, 3.0],[0, 0]]. The MA is invertible (eigenvalues
0.5, 0) and det Θ(1) = 0.5. (b) holds (eigenvalues of I − ΛB′: 1, 0.3), and (c) holds
(Λ⊥′B⊥ = 1.30). rank CΘ(1) = 1, so the cointegration rank is exactly 1. Simulation: the
k-step variance ratio of B′Y → 0 while that of Y₁ → 0.041, the analytic long-run variance.
Yet **σ_min(G) = 1e−16**. The binary itself, run on 2001 observations from this DGP
(`run/g0.out`), estimates Θ₁ ≈ [[0.506, 2.949],[0,0]], Λ ≈ truth, B₂ = −0.991 and prints:

    *** WARNING: sigma_min(alpha_perp' Theta(1) beta_perp) = 7.838e-03 < 2.0e-01
        This fit DENIES THE RANK it was estimated at ...

This is a false alarm on a correct, invertible, identified model.

**Further consequences.**
- Under `-mawarma` (and `-warma`) one has Θ(1)B⊥ = B⊥ identically: under (6), the cross
  block cancels −(I−Θ_w)B₂′ − Θ_wB₂′ = −B₂′. So G = Λ⊥′B⊥ **for any Θ**: the "rank
  condition" reported for those classes does not depend on the MA at all (s2 (3): three
  draws, G equals σ_min(Λ⊥′B⊥) to 6 digits). Λ⊥′B⊥ is the VAR(1) version of condition (c).
  In a VAR(p ≥ 2) it can vanish on a perfectly valid I(1) model (s2 (4)).
- `-rankadm` rejects admissible points. The `-specs` column `adm` (l. 7015) and its
  "no χ² p-value" decisions, the `.out` block (l. 6006–6015, 6622–6672) and the terminal
  WARNING all use G.
- The 0.2 floor ("empty gap between 0.016–0.133 and 0.52–1.00", l. 371–380,
  HOMOLOGATION 4h/4j) was calibrated on G. The "admissible" values for the
  `-warma/-mawarma` rungs are just σ_min(Λ⊥′B⊥). The §4j conclusion that `-matri` "does
  not remove the pathology" was measured with G and must be re-measured. Supplied: under
  `-matri`, M = 2, r = 1, T₂₂ = 1 is fatal for (d) only if λ₂ = 0, whereas G also vanishes
  on the artefact line t₁₂ = −(1−t₁₁)b₂.
- The wheat-pair diagnosis (null direction of Θ̂(1) aligned with Λ⊥ at cos 0.946–0.9996)
  was measured directly, not through G, so it stands.

**Fix.** Report and impose σ_s(Λ⊥′Θ(1)) (the s-th singular value of the s × M matrix, with
Λ⊥ orthonormal), or equivalently σ_M([Φ(1) Θ(1)]) (§11). Re-calibrate the floor.

## 4. Corollaries 3.1, 3.2

**C3.1** proof correct. Its "relevance" sentence ("-rankadm imposes the strong condition
σ_min(Λ⊥′Θ(1)B⊥); det Θ(1) ≠ 0 would suffice") is false for G. In the s2 point
det Θ(1) = 0.5 and G = 0.

**C3.2.** (1) and (2) correct. (The symbol Θ*(L) in (1) clashes with T1's Θ*.) Item (3)
says "rank(Λ⊥′Θ(1)) = s (generically)" and "the unit MA root lives in the direction of
col(Λ)". Exact version: (d) holds iff null(Θ(1)′) ∩ col(Λ⊥) = {0}. If the left null space
is one-dimensional and spanned by w ∉ col(Λ⊥), then (d) holds. w need not lie in col(Λ).
"Harmless" is only with respect to the rank: the point is still on the invertibility
boundary, which violates Phillips' Ω > 0 and Yap–Reinsel's invertibility (T8, T10).
CORRECT WITH A GAP.

## 5. Theorem 2′ (stationarity of the transformed system)

The conclusion (|Φ*(x)| has no roots in |x| ≤ 1) is right. The proof uses Mauricio's
argument: "a stationary process following a uniquely identified VARMA has a stationary
AR". Left-coprimeness of (Φ*, Θ*) fails exactly on the fatal surface of T3 (§11), so the
argument is weakest where the doc needs it. **Direct proof (supplied, verified in s1 to
9e−15):** from Φ̄(L)D(L)C̄ = Φ(L),

  det Φ̄(x) · (1−x)^s · det C̄ = det Φ(x),   det C̄ = ±1.

Under (b), det Φ has an s-fold zero at 1 and all others outside the unit circle. So det Φ̄,
and det Φ* = det C̄ · det Φ̄, have all their zeros outside the closed disc. No identification
is involved, Θ plays no role, and the "salvo cancelación" caveat can be dropped.
Corollary: Φ̄(1) = [F(1)B⊥, Λ], so det Φ̄(1) ≠ 0 ⇔ (c). The engine's AR gate therefore
enforces (b)+(c).

## 6. Lemma A, Theorems 4 and 5

**Lemma A.** Form and code references are consistent: `chekma` rejects when an eigenvalue
modulus is ≥ 1.00005 (elfvarma.c:529), and the ξ truncation is at `s2 < xitol` (:773).
The accuracy claim is data-dependent; see T9, where the engine's logL is off from the exact
value by 4.5e−4 (joint) and 7.0e−4 (univariate) at xitol = 1e−3. The tail neglected by ξ
grows like xitol/(1−|θ|), so it is worst near the invertibility boundary, which is exactly
where T4's fits sit. (At an exact unit root ξ does not decay, and the sequence runs to
n − 1, i.e. no truncation.)

**T4.**
- "Not in 𝒞" is false under DEMOSTRACIONES' own definition of 𝒞 (§1); it is true with
  (d) added or with THEORY.md's definition.
- The finiteness argument ("measure-zero event for a finite Toeplitz form") is informal.
  **Supplied:** Ȳ_{1:n} = G a_{1:n} + (a term independent of a_{1:n}), with G block lower
  triangular with identity diagonal blocks (Ψ₀ = I). So Cov ≥ G(I⊗Σ*)G′ > 0 for **every** Θ,
  invertible or not, and log L is finite. It is continuous because the autocovariances
  are continuous in the parameters while Φ* is stationary. s3 (E): VMA(1) with
  det Θ(1) = 0 in the Λ⊥ direction, n = 150: min eigenvalue 4.2e−4 > 0. The doc's scalar
  formula (eigenvalues 2 − 2cos(kπ/(n+1))) is verified. Note that it tends to 0 like π²/n²,
  so the likelihood is finite for each n but the information degenerates.
- "The gate rejects only points strictly inside" is loose. `chekma` admits companion
  eigenvalues up to 1.00005, i.e. MA roots slightly **inside** the circle, down to 0.99995.

Verdict: CORRECT WITH A GAP.

**T5.** Φ* roots depend only on Φ (§5), so no AR check can see a Θ-side failure. Correct,
and more simply proved than in the doc. But "invisible" only means "not rejected". In
**every** class, rank failure ⇒ w′Θ(1) = 0 with w ≠ 0 ⇒ det Θ(1) = 0 ⇒ the MA companion has
an eigenvalue exactly 1. s3 (D): 2000 free-class inadmissible draws, all with an eigenvalue
|λ| = 1 to 6e−13. `chekma` sees it and admits it. The doc's claim that T5 "is a statement
about the free class" (C6.3 consequence) is wrong: nothing in the proof uses freeness.
CORRECT WITH A GAP.

## 7. The triangular class — Theorem 6, 6b, C6.1, C6.2

**T6 direct direction / T6b.** s4 (a) simulates the full WARMA with MA and with
contemporaneously correlated (a, η). It checks that the VEC with the doc's M_l, A, Γ_i and
Θ̃_j = [[Θ_{w,j}, −Θ_{w,j}β′],[0,0]], on A_t = (a_t + β′η_t; η_t), reproduces Δz_t exactly
(relative error ≤ 3e−14 for (m, r, p, k, q) = (2,1,1,0,1), (3,1,2,1,2), (4,2,3,2,1),
(5,3,2,3,3)), with rank Γ_i ≤ r. The code (drvec.c:3865–3871, 3951–3961) builds
Θ[k][i][r+jj] = Σ_ii T₁₁[k][i][ii]·B₂[jj][ii] = (T₁₁B₂′)_{i,jj}, which is (6) with
B₂ = −β. CORRECT. (Only lag 1 is written in (6); it holds lag by lag.)

**T6 converse (sufficiency).** Given (5), Γ_i = N_iα′. Since α′ has full row rank, the
M_l are recovered uniquely: M_{i+1} from Γ_{i+1} − Γ_i, then M₁ = A − Σ_{l≥2}M_l. Their
blocks give Ψ_{l−1} (bottom) and Φ_l (top − β′Ψ_{l−1}). With (6), ε = (Θ_w(B)a + β′η; η).
CORRECT.

**T6 necessity** ("admits a triangular representation *only if* (5), (6)"). The argument
("Δz would depend on Δz_{t−i} outside span(w)") compares two representations of the same
process. That is valid only if the VEC representation is unique. For the pure VAR it is:
the linear projection is unique for a non-degenerate process. With MA, uniqueness requires
the identification conditions (left coprimeness plus an order/echelon condition such as
Hannan's rank[Φ_p Θ_q] = M). Without them, multiplying by a common left factor produces a
VEC violating (5)/(6) for a process that is triangular. **Gap: add "with (F, Θ) identified"
to the "only if".** The necessity of (6) is also relative to the BVECM definition of WARMA
(w driven by a alone). Phillips' broader triangular form (u_t a general stationary process)
contains `-marow` too.

**BVECM converse is false** (doc's claim confirmed). Every WARMA-derived VEC has
rank Γ_i ≤ r (s4 (a)). A VEC(2) with M = 2, r = 1 and Γ₁ = [[0.3, 0.2],[−0.1, 0.25]] has
rank Γ₁ = 2. It is a valid I(1) rank-1 process, and its VAR representation is unique, so
it has no WARMA form. OLS on 2e5 simulated observations recovers Γ₁ with singular values
(0.368, 0.258). BVECM Step 6 skips exactly this ("one can express the right-hand side
solely in terms of lagged w and Δz₂").

**Code.** `-warma`'s zero pattern (Φ*_k = [0 Ψ_k; 0 Φ_k], Θ*_k = diag(0, T_k)) is exactly
(5)+(6). Supplied: the ∇Y₂ columns of Φ̄_k are F_kB⊥, which vanish ⇔ F_k = N_kB′ ⇔ (5).
`-mawarma`, `-marow` and `-matri` impose MA structure only, with F free, so they are not
"the triangular class". `-artest` tests (5) separately. That is consistent, but the doc's
"relevance" line for T6 should say so.

**C6.1** (sympy, s4 (c), r = 1, s = 2). Θ* = C̄ΘC̄⁻¹ patterns: matri → [[T₂₂, 0],[·, T₁₁]];
marow → [[0, 0],[·, T₁₁]]; mawarma → diag(0, T₁₁). Exactly the table. PROOF CORRECT.

**C6.2.** Θ(1) = [[I−Θ_w, −Θ_wB₂′],[0, I]] and det = det(I−Θ_w): correct. The proof
writes Θ(L) = [Θ_w(L), Θ_w(L)B₂′; 0, I]. The cross block is actually −Θ_{w,1}B₂′L
(= (Θ_w(L) − I)B₂′). This is harmless for the determinant. The claim "−matri does not
remove the pathology, −marow does" is an empirical claim measured with G (§3).

## 8. Corollary 6.3 — "the engine's gate IS the admissibility condition"

(1) det Θ̃(1) = det(I_r − ΣT_k) and (2) spectrum = r-block companion + sq zeros are
correct for every q and (r, s) (s3 (A): error 0 and 1e−14 over 1200 draws).

One wording error in (2): the text says "no coordinate of block s feeds those of block r".
The reverse is true. The r-coordinates **are** fed by the s-coordinates through the cross
blocks C_k; what makes the matrix block-triangular is that the s-coordinates are not fed
by the r-coordinates (the s rows of the first block row are zero, so the s-part is a pure
shift). The conclusion is unaffected.

(3) "Θ̃ invertible iff its r×r block is; then (d) holds" is true for **strict**
invertibility, but that is just C3.1 and holds in every class.

**The "Consequence" and the code's reliance on it are false:**
- **Gate-admitted but inadmissible, inside `-marow`** (s3 (B)): Θ₁ = [[1, 0.7],[0,0]],
  Λ = (−0.56, 0.8)′. `chekma` passes (|λ| = 1 < 1.00005), and (d) fails. Even
  t₁₁ = 1.00004 (non-invertible) passes the gate.
- **Gate-rejected but admissible**: Θ₁ = [[3, 0.4],[0,0]]. `chekma` rejects it, and (d)
  holds.
- In the free class, too, inadmissibility ⇒ an MA eigenvalue exactly at 1 (§6). The
  logical relation between the gate and admissibility is **identical in both classes**.

What is true, and is the real content: in `-marow`, a rank failure can only occur through
a unit root of the **r×r (W) block**. The T₂₂ → I route (overdifferencing of the already
differenced block, which is where the free fits went) is removed. In the free class, the
unit MA root can sit in any direction. That is a C6.2-type statement, and it is the
defensible basis for the default. The strong claims do not hold: "𝒞 is the entire space",
"no bound", and "the -rankadm bias argument does not apply".

**Code that rests on it:** the default switch to `-marow` (drvec.c:8170–8186; comment
l. 279–303: "IS NOT REACHABLE there: chekma on Theta IS chekma on the r×r block"), and the
terminal WARNING text "The default (-marow) cannot reach them" (l. 6650–6655). The default
itself remains empirically supported (HOMOLOGATION 4q/4r), but its theorem-level
justification is not valid as written.

## 9. Theorem 7 (Λ = 0)

(i) correct. (ii) det Φ̄ = ±(1−x)^r det F(x) by §5: **r** unit roots, not "a root".
(iii) The stationary exact likelihood is undefined, and the engine's `cgamma` fails
(ifault 2). The non-standard null of the rank test comes from the extra unit roots and
the unidentified B₂ (a Davies-type problem), not from a "boundary" in the Chernoff sense.
CORRECT (minor wording).

## 10. Theorem 8 (Yap–Reinsel 1995, Thm 3) and `-lrtest`

**Source (read, pp. 253, 259–260).** Model (1) with det Θ(x) = 0 having all roots
**outside** the unit circle, and Φ, Θ left coprime. Estimation is **conditional** Gaussian
ML with Θ₁…Θ_q unrestricted in both fits. Thm 3: −T log(|S|/|S₀|) for H₀: rank C = r
against H_A: rank **m** (the trace test) converges to
tr{(∫B_d dB_d′)′(∫B_dB_d′)⁻¹(∫B_d dB_d′)}. §6 covers a constant with Q₁′μ = 0 (no drift),
which is drvec case 2.

**Statement mismatch.** The doc's formula has a second line "+ a′[∫B_d dB_m′ − ½B_m(1)…] …".
That term is not in Yap–Reinsel's Theorem 3; §6's modification uses ξ = ∫B_d, a
different thing. H₁ in the source is rank m; `run_lrtest` (drvec.c:7239–7455) computes
2[L(r+1) − L(r)], the λ-max form. The λ-max limit follows from the same eigenvalue
representation (their eq. 27–28), but Yap–Reinsel do not state it.

**Code outside the source's conditions:**
1. **Non-nested at the first step (default class).** At r = 0, `ma_struct_on()` switches
   the MA structure off (l. 315), so Θ is free. At r ≥ 1 the default is `-marow`. The
   r = 0 model is therefore not contained in the r = 1 model. On the repo's own
   `data/synth.inp` (p = 1, q = 1): npar(r=0) = 6 and npar(r=1) = 7, with LR = −4.98.
   With `-mafree`: 6 vs 9, LR = −5.03, also negative. The program prints "(rank r is
   nested in r+1)" (l. 7325), which is false under the default.
2. For r ≥ 1 the `-marow` classes are nested (fewer zero rows as r grows), but H₁ then
   frees additional MA parameters. The limit is no longer Yap–Reinsel's, which has the
   same unrestricted Θ under H₀ and H₁.
3. **Case-1 critical values belong to another model.** `lr_cval_none` (l. 148–153) comes
   from urca `ca.jo(type="eigen", ecdet="none")`, and urca's "none" **adds an unrestricted
   constant** to the short-run regressors (`Z1 <- cbind(1, Z1)`, checked in the installed
   urca). Monte Carlo at M − r = 1 (s6, 20 000 reps):

   | case | MC 90/95/99% | drvec uses |
   |---|---|---|
   | no deterministic term (drvec case 1) | 3.07 / 4.21 / 7.12 | 6.50 / 8.18 / 11.65 |
   | unrestricted constant (urca "none") | 6.66 / 8.31 / 11.62 | — |
   | restricted constant (drvec case 2) | 7.57 / 9.25 / 12.76 | 7.52 / 9.24 / 12.97 ✓ |

   So in case 1 the asymptotic test is run against the wrong (larger) critical values.
   Case 2 is right.
4. Exact vs conditional likelihood: asymptotically equivalent for strictly invertible Θ.
   The boundary fits (MA root ≈ 1) violate the invertibility hypothesis anyway.

The parametric bootstrap (`-bootstrap`) calibrates whatever statistic is computed, so it
remains usable. The asymptotic table does not apply to the default `-lrtest` with q ≥ 1.

## 11. Identification (§7) — the rank condition is left-coprimeness at z = 1

The quotes are correct: Yap–Reinsel p. 253 ("it is assumed that Φ(L) and Θ(L) are left
coprime … Dunsmuir and Hannan 1976; Hannan 1975"), and Mauricio 2006 p. 3646 ("Additional
conditions … such as those considered by Yap and Reinsel (1995), are also assumed").

**Supplied equivalence (s7, 20 000/20 000 agreement):**

  rank(Λ⊥′Θ(1)) = s  ⇔  rank[Φ(1) Θ(1)] = M.

Proof: Φ(1) = ΛB′, and B′ has full row rank. So w′[ΛB′, Θ(1)] = 0 ⇔ w′Λ = 0 and
w′Θ(1) = 0 ⇔ w = Λ⊥c with c′Λ⊥′Θ(1) = 0. Left-coprimeness requires rank[Φ(z) Θ(z)] = M
for all z. The fatal points are therefore exactly those where the levels VARMA has a
common left factor vanishing at z = 1: the unit AR root in direction w cancels the unit MA
root, the classical common-factor over-parametrisation.

Consequences:
- §7 item 2 ("Granger with MA is *stronger* than coprimeness; a point can be a perfectly
  identified VARMA and inadmissible") is **false**. Every inadmissible point is
  unidentified.
- §8's closing line ("the only condition the literature does not write in the
  parameters") is misleading. The literature *assumes* it, as part of identification.
  What is true is that neither Mauricio's estimator nor drvec's engine *enforces* it.
- This also explains T4/T5: over the unrestricted 𝒫 the likelihood is flat along
  common-factor directions, which is why free fits drift there.

## 12. Theorem 9 (diagonal factorisation) and the entry gate

The mathematics is trivially correct (independence). The code compares the joint diagonal
fit with M univariate `elf` evaluations **at the same fitted point** (`gate_contract`,
drvec.c:2461–2652), which is the right use.

**Independent check** (s5; simulated independent ARMA(1,1) differences, n = 300; drvec
`fac 2 1 0 -diagar -diagma -diagcov`):

| quantity | value |
|---|---|
| my exact joint logL (Toeplitz/Cholesky) at drvec's printed point | −1038.6388278 |
| statsmodels exact (Kalman), same point | −1038.6388278 (agrees to 1e−10) |
| drvec joint `logelf` | −1038.6392778 (−4.5e−4) |
| drvec gate's univariate sum | −1038.6381298 (+7.0e−4) |
| gate: joint − sum | **−1.148e−3 > tol 1e−3 → "*** NOT VERIFIED ***"** |
| scratch rebuild with xitol = 1e−10 (`build_xitol/`, `run_xi/`) | joint −1038.63882748, sum −1038.63882746, gap 2e−8 |

The common-scale concentration is not the cause: re-concentrating each σᵢ² separately
changes the sum by 8e−10 at this point, which is second order at a converged fit. So the
identity holds, and the residue is ξ truncation, as the doc says. But the doc's
"~1.4·10⁻⁴" and the code's tolerance "= xitol" (l. 2571) are not bounds. The truncation
error scales roughly like xitol/(1−|θ|), and here (|θ| ≈ 0.55) it exceeds xitol. **The
gate declares a false failure on a correct model.** Fix: xitol·c/(1−max|θ|), or a relative
tolerance, or evaluate the gate with a tighter xitol.

The "optimality certificate" ("gap ≥ 0 always, zero iff the values were the optima")
holds only for a monotone optimiser that converged. "Iff" should read "iff the brought
values are a stationary point the optimiser does not leave".

## 13. Proposition 1, Proposition 2

**P1.** It is a heuristic argument, not a theorem, and each of its three points holds.
(2): C̄ΘC̄⁻¹ with Θ diagonal is non-diagonal unless T₁₁B₂′ = B₂′T₂₂. (3): the ∇Y₂ columns
of Φ̄_p are zero. Fine as stated ("medido + estructural").

**P2.** (1) The filtrations coincide given B₂ and the anchor Y₁. Correct. (2) The program
(`forecast_core`, drvec.c:4561–4605) forecasts from the stationary VARMA on the sample
Ȳ₂…Ȳₙ (in raw indices) with `elf`'s residuals. That is E[·| Ȳ₂…Ȳₙ]. The proposition
claims E[·| Y₁…Yₙ], which also conditions on W₁ (part of the stationary process, and
informative). The two coincide only if Y₁ is treated as a fixed constant (the usual
convention); the difference is O(ρⁿ). (3) The level-error map is correct. G_m =
[C_m^{(∇Y₂)}; Ψ_m^{(W)} − B₂′C_m^{(∇Y₂)}], and `level_error_map` (l. 4536–4548) implements
exactly that (rows s+i use B₂[k][i] = (B₂′)_{ik}). Var = Σ G_mΣ*G_m′ is the known-parameter,
infinite-past MSE. CORRECT WITH A GAP (conditioning convention; stated, not an error in the
code).

## 14. Theorems 10, 11

**T10 (Phillips 1991a).** "Provided that all unit roots in the system have been eliminated
by specification and data transformation" is quoted correctly (abstract, §3). Omitted:
Theorem 1′ (linear-process/ARMA errors) is stated **"If Ω = 2πf(0) > 0"**. In drvec terms
Ω > 0 requires det Θ(1) ≠ 0, so the LAMN licence fails on the **whole** surface
det Θ(1) = 0, including C3.2 case (3), not only on the fatal subset. Overstated: "full
system ML requires the exact likelihood of a general ARMA". Phillips uses the Whittle
likelihood (eq. 21), and Remark (m)/Thm 1″ say that only a consistent estimate of Ω is
needed. STATEMENT INCOMPLETE (missing hypothesis Ω > 0).

**T11 (Johansen 1991, Thm 5.1).** It matches the source: H₃, mixed normal, V_α independent
of G. But it is a VAR theorem, with Gaussian iid errors and the linear-trend case τ_H ≠ 0.
For drvec's VARMA, the relevant result is **Yap–Reinsel 1995, Theorem 2**:
T(B̂₀ − B₀) → [A′Θ′⁻¹Σ⁻¹Θ⁻¹A]⁻¹A′Θ′⁻¹Σ⁻¹Θ⁻¹PMP₂₁⁻¹, mixed normal, with a normal-theory
conditional limit (p. 258–259). It requires a strictly invertible Θ, i.e. Θ = Θ(1)
non-singular in their notation. Cite that instead or in addition.

## 15. Incidental (confirmed, already filed)

`run/g0.out` shows the Θ table labelled `D.Y2 <- A.Y2(-1) 0.506`,
`D.Y2 <- A.Y1(-1) 2.949`, but the fitted row is the Y1 equation (the DGP's Θ₁ row 1). Γ
and Θ are printed with `sname()` on internal indices (drvec.c:6313–6337) while Λ goes
through `inp2lam`. This is **BUG-23** in docs/BUGS.md. It is noted here because it makes a
`-marow` fit *look* as if the differenced block carries an MA.

## 16. Which design decisions rest on a false or unproven claim

1. **The rank diagnostic, `-rankadm`, the `-specs` admissibility column, the 0.2 floor and
   the "DENIES THE RANK" warning** rest on G = Λ⊥′Θ(1)B⊥. That is not Theorem 3's
   condition. It gives false alarms in the default class (demonstrated with the binary).
   In `-warma/-mawarma` it does not depend on Θ at all.
2. **Default MA class `-marow`**: justified in code and docs by C6.3's "𝒞 is the whole
   space / the gate imposes admissibility", which is false. The default is still
   defensible, but on weaker grounds: C6.2-type removal of the T₂₂ route, plus
   HOMOLOGATION 4q/4r.
3. **`-lrtest` asymptotic verdicts with q ≥ 1**: T8 does not cover the default. The first
   step is non-nested, and case 1 uses the unrestricted-constant table.
4. **Entry gate tolerance = xitol**: not a bound; gives false "NOT VERIFIED".
5. **§7's claim that the rank condition is beyond identification**: false. It is
   left-coprimeness at z = 1. This changes the narrative, not the code, and suggests the
   natural diagnostic σ_M([Φ(1) Θ(1)]).

Sound and verified: T1, T2 (after the supplied steps), T3 (the mathematics), T2′'s
conclusion, C6.1, C6.2, C6.3(1)(2), T6 direct and converse (with the identification
proviso for necessity), T6b, T9's identity, and P2's error map.

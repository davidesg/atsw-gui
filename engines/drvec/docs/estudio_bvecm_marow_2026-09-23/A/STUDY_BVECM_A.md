# STUDY BVECM-A: the article behind drvec's default MA class (`-marow`)

*2026-09-23. Read-only study. Sources: `Article_Multivariate Convergence/cointegration_convergence/Legacy/`:
`BVECM_models.tex` (May 2; its PDF is byte-identical to `drvec/literature/BVECM_models.pdf`),
`BVECM_models_proofs.tex` (the appendix that `BVECM_models.tex:627` `\input`s; identical to
`old_version/BVECM_models_proofs.02.05.2026.tex` except for the header comment and an added paragraph at l.74-78),
`proofs.tex` (Apr 29, correction log), `BVECM_models_proofs.original.tex`, `bvecm_example_addon.tex` (May 13),
`bvecm_presentation.tex` (May 12), `results_Wheat_1848.tex`, `CI11_notes.tex` (Treadway, 2009), `OnCointegration.tex`.
I also read the article's "accompanying software", `~/Dropbox/SRC/drv_project/src/main.c` and its
outputs in `drv_project/bin/`. The earlier studies were read first and are not redone:
`drvec/docs/estudio_mauricio_2026-09-23/M3/STUDY_M3.md` and `drvec/docs/ESTUDIO_MAURICIO_2026-09-23.md`.*

Scripts in this directory:

| script | what it checks |
|---|---|
| `s1_theorem1.py` → `s1.out` | Appendix Thm 1 and Cor 2, direct direction (exact simulation, 7 configurations). Step 6's explicit formulas. |
| `s2_converse_classes.py` → `s2.out` | (a) the VEC(2) counterexample to the converse; (b) the MA converse of Cor 2 fails; (c) the "only if" fails without identification (common left factor); (d) the article's full-VAR example lies outside the Thm-1 image; (e) the drvec MA classes and the legacy MA class in VEC and Ȳ coordinates (sympy) |
| `s3_tables.py` → `s3.out` | arithmetic and textual consistency of the empirical tables, the add-on and the presentation |

Notation: the article uses z = (z₁; z₂), α = (I_r; −β), w = z₁ − β′z₂, Ỹ = (Δz₂; w).
drvec uses Y = (Y₁; Y₂), B₂ = −β, W = w, and Ȳ = (∇Y₂; W).
"Θ\*" is the MA of the stationary VARMA on Ȳ, and "Θ" is the MA of the VEC. They are related by
Θ\* = C̄ΘC̄⁻¹.

---

## 0. Verdict table

| # | Claim | Where | Verdict |
|---|---|---|---|
| 1 | Main-text "WARMA form" (2.1)–(2.8), with Θ = [[0,0],[0,γ]] | `BVECM_models.tex:127-151` | **INTERNALLY INCONSISTENT.** (2.3) implies Δz₁ₜ ≡ 0. (2.4) contradicts (2.2). γ is (m−r)×r but sits in an (m−r)×(m−r) block. It puts the MA in the **Δz₂ block**, which is the opposite of the appendix. Transcribed from `OnCointegration.tex:83-115`. |
| 2 | Definition 3 (restricted WARMA): Δz₂ = ΣΨⱼw₍ₜ₋₁₋ⱼ₎ + η, with η white; Φ(B)w = a; MA only on w (Cor 2) | proofs `:52-79`, `:325-336` | This is an **ASSUMPTION** of the appendix. No economic motivation is given. The article itself says it is **not** the estimated model (`:74-78`, main `:226` fn, `:266`). |
| 3 | Thm 1, direct direction (A = ΣM_l, Γᵢ = −Σ_{l>i}M_lα′, ε = (a+β′η; η)) with the corrected M₁ (−I_r) | proofs `:99-136`, `:142-261` | **CORRECT.** Verified exactly (rel. err ≤ 7e−15, s1) |
| 4 | Thm 1 converse: "any VEC with Π = Aα′ of rank r can be written in WARMA form" | proofs `:137-139` | **FALSE.** Counterexample: VEC(2) with rank Γ₁ = 2 (s2a, which confirms M3) |
| 5 | Step 6 proof: "Φ₁ = α′A + I_r", "γ = A₂"; recursion "uniquely determined" | proofs `:263-292` | **FALSE in general.** Φ₁ = α′A + I holds only for p = 1, and γ = A₂ only for k = 0 (s1: errors 0.2–1.5). The recursion presupposes Γᵢ = −ΣMα′, i.e. it assumes what it has to prove (circular). |
| 6 | Cor 2 (MA inheritance), direct: same A, Γᵢ; ε = (β′η + Θ(B)a; η) | proofs `:325-370` | **CORRECT**, provided (a,η) is jointly white noise (the appendix only says "possibly correlated", `:71-72`). The regrouping Θ̃ⱼ = [[Θⱼ, −Θⱼβ′],[0,0]] is **drvec's own addition** (T6b), and it is correct (s1). |
| 7 | Cor 2 converse: "every VEC model with MA error structure can be transformed into the WARMA form … via Mauricio" | proofs `:349-351` | **FALSE.** VEC(1) + Θ with a non-zero Y₂ row has no Def-3 form (s2b: lagged-Δz₂ t-stats of −216 … −3.3). Mauricio's procedure yields a **general** VARMA on Ỹ, not the Def-3 form. |
| 8 | drvec T6 "only if (5),(6)" without identification | DEMOSTRACIONES T6 | **FALSE without identification** (confirms the M3 gap). Explicit counterexample (s2c): the same triangular process has an invertible, non-coprime VEC–MA representation that violates (5) and (6). Path difference 1.7e−13. |
| 9 | Remark: "formulas … remain valid under the full VAR setup" | main `:266`, proofs `:74-78` | **FALSE for Γᵢ.** It holds for A only when p = 1. The full VAR(1) on Ỹ gives Γ₁ = [[0, φ₁₂+βφ₂₂],[0, φ₂₂]], which is outside the Thm-1 image (s2d). |
| 10 | Estimated model: "VARMA(2,1) in the triangular Phillips form" | main `:518`, results `:5` | The software (`drv_project/src/main.c:1662-1680`, `:26-49`) fits a **full VAR(p) on (w, ΔL)** + **MA(1) on the ΔL (Δz₂) equation only** (θ₂₂ free, θ₁₁ = θ₁₂ = θ₂₁ = 0). That is **outside -marow, -mawarma and -warma**, and inside -matri and -mafree (s2e). |
| 11 | Wheat tables: g = ω/(1−δ), l = 1/(1−δ) | main `:536-546`, results `:23-33` | g and l are arithmetically consistent with ω and δ. But l = 1/(1−δ) contradicts the article's own mean-lag definition ν′(1)/ν(1) = δ/(1−δ) (`:411`), and "H₀: l̄=0 ⇔ δ=0" (addon `:638`). |
| 12 | Text vs main tables (δ ≥ 0.77; g significant except S/L β=1; London weakly exogenous for V/L with p > 0.05) | main `:561`, `:607` | **INCONSISTENT with the tables they describe.** They match the older `results_Wheat_1848` run, not the current tables (s3). |
| 13 | Add-on numerical illustration α̂₁ = −0.305 | addon `:377-403` | **INCONSISTENT.** It plugs the "AR root" column (0.651) in as φ₁₁, and Σα₂ (0.044) in as φ₂₁. Table 2 gives Σα₁ = −0.457. |
| 14 | Add-on D_t (Step 2/3), Γ₁ display, Cholesky p₂₁, Granger condition | addon `:545`, `:578`, `:289`, `:353`, `:221`, `:368` | **ERRORS.** The "+1" slip is back in D₁ (off by c₍ₜ₋₁₎). The columns of Γ₁ are swapped. p₂₁ is taken from Σ_a, not Cov(u). "γ₂₂ = 0" is not part of z₁ ↛ z₂ (s3). |
| 15 | An argument in the article for the restricted MA class as a **default for generic data** | whole corpus | **NONE.** The model-building paragraph (`:155`) prescribes specification from evidence: q = 0, stepwise AR, then "multiplicative MA reformulation" for **visible** MA structure. |

---

## 1. The model hierarchy in the article, and where the zero rows come from

### 1.1 Level 0: cointegration and normalisation (`BVECM_models.tex:70-106`)

- Property 1: Δz stationary **and invertible**.
- Property 2: rank-r α with α′z stationary.
- r = 1 "in what follows" (`:79`).
- α and r known (`:81`).
- Normalisation α = (I_r; −β) (`:85-104`).

No dynamic class is assumed at this level.

### 1.2 Level 1: the "WARMA form". There are three different objects under this one name.

**(i) Main text (2.1)–(2.8), `:119-153`**, copied from Treadway's `OnCointegration.tex:83-115`:
```
Δz₂ₜ = γ wₜ₋₁ + ηₜ                     (2.1)
wₜ = wₜ₋₁ + γ′Δz₂ₜ + ξₜ               (2.2)
Δzₜ = (0; γ) wₜ₋₁ + (0; ηₜ)            (2.3)
wₜ = (0  γ′) Δzₜ + ξₜ                  (2.4)
... Θ = [[0,0],[0,γ]]                  (2.8)
```
This block is not a coherent model:
- (2.3) sets the Δz₁ rows to 0, so Δz₁ₜ ≡ 0.
- (2.4) drops the wₜ₋₁ of (2.2).
- In (2.8), γ is (m−r)×r but occupies an (m−r)×(m−r) block.
- Its MA sits in the **Δz₂ block**, which is the opposite of the appendix.

Nothing downstream uses it, but it is the only "WARMA" in the main text that carries a Θ.

**(ii) Appendix Definition 3 + Corollary 2 (the "restricted WARMA")**, proofs `:52-72`, `:325-336`:
```
wₜ = α′zₜ,   Φ(B) wₜ = Θ(B) aₜ          (Φ, Θ: r×r; Θ invertible, :336)
Δz₂ₜ = Σ_{j=0..k} Ψⱼ wₜ₋₁₋ⱼ + ηₜ       (Ψ₀ = γ)
(aₜ, ηₜ) zero-mean innovations, "possibly correlated"
```
The class is the following. The stationary block w is an autonomous VARMA. The common-trend block
Δz₂ has **no own lags, no lags of Δz₂, and no moving average**. Its only dynamics are the feedback
from lagged disequilibria, and its innovation is white. In Ȳ coordinates this is literally
```
Φ*(L) = [[I, −LΨ(L)],[0, Φ(L)]],   Θ*(L) = diag(I, Θ(L)),   innovations (η, a)
```
It is drvec's `-warma` (s2e: `-warma` → Θ\* = [[0,0],[0,T₁₁]]).

**(iii) What is estimated.** Main `:226` footnote: "the theoretical analysis in the Appendix
considers a restricted WARMA form, but the same formulas hold for the full VAR". Main `:266`:
"While the formal derivations in the Appendix start from a restricted WARMA specification
(Definition 3), the estimation procedure and the above example adopt a full VAR model for the
stationary vector (Δz₂′, w′)′". Proofs `:74-78`, added in the May-2 final: "Although the
theoretical derivations below use this restricted form, the estimation software and the
empirical application employ a full VAR for (Δz₂′,w′)′".

The presentation is the most explicit. At `:209-210`: "All four φᵢⱼ(k) are free. This is a **full
VAR** for Ỹ, not restricted triangular." At `:493-496`: "The equivalence theorem covers the
restricted case (φ₁₂=0)".

The MA actually estimated (drv_project `main.c`) has the following structure:
```
:26    int global_q = 1;
:47    npar += 4 * global_p;   /* VAR COMPLETO */
:49    if (global_q >= 1) npar += 1;  /* θ₂₂(1) */
:1676  theta1[1][2][2] = x[idx++];  /* θ₂₂(1) - solo para ecuación 2 */
:1678  theta1[1][1][1] = 0.0; theta1[1][1][2] = 0.0; theta1[1][2][1] = 0.0;
:1742  armax->w[t][1] = A_t − vtmp1[t] − β·L_t   /* ecuación 1: ECT (w) */
:1747  armax->w[t][2] = ΔL_t                     /* ecuación 2: Δz₂ */
```
The same θ₂₂-only line appears in every dated version of `main.c` in `src/olds/` (Jan–Feb 2026).
The outputs print "VARMA(2, 1) - MA diagonal fixed / Structure: triangular Phillips (MA diagonal
fixed)". A/L estimates:
- `bin/AL.2.out_fullAR2.txt`: full VAR(2), **θ₂₂ = 0.7146 (s.e. 0.116)**.
- `bin/AL.2.results.txt`: **θ₂₂ = 0.821**.

So the article's empirical model has Θ\* = diag(θ, 0) in (∇Y₂, W) order, i.e. MA on the
**common-trend equation only**. In VEC coordinates (s2e, sympy) that is
```
Θ = C̄⁻¹ Θ* C̄ = [[0, −B₂θ],[0, θ]]   (= [[0, βθ],[0, θ]])
```
The Y₂ row of Θ is non-zero. This is **exactly the complement of `-marow`** (Θ\* = [[0,0],[·,T₁₁]]).
The parameter is the one drvec calls T₂₂, the "pathology" direction of `-matri`/`-mafree`.

### 1.3 Level 2: the stationary VARMA (`:163-215`)

Ỹ = (Δz₂; w), with C̃ and H̃ as in Mauricio (`:171-188`). The VEC (`:193-195`) is written **without**
MA, although the text says Θ̃(L) depends on "Θᵢ" (`:200`). "This mapping is one-to-one" (`:200`) is
true onto its image (M3 T1). "Uniqueness … avoiding identification problems" (`:207`): the
transformation is unique, but identification of the VARMA is not implied by it (M3 §7).

### 1.4 Level 3: the BEC and Π forms (`:217-329`)

These start from the levels VARMA Φ(B)z = Θ(B)a with a **general** Θ (`:273-299`). They require no MA
restriction. "Non-scalar MA structures often imply cross-equation dependencies, and the matrix Θ
necessarily introduces coupling" (`:224`).

### 1.5 Answer to Q1

The source is Phillips (1991): y₁ = β′y₂ + u₁, Δy₂ = u₂, with u = (u₁,u₂) a general stationary
linear process. The article's own methodological source is Treadway's `CI11_notes.tex`. There the
"TECF" (7.1)–(7.3) is an ARMA on (W, ∇Z₂) with **full** Φ″(B) and a **full**
Θ″(B) = [[I,−A],[0,I]]Θ(B)[[I,A],[0,I]]. That is Mauricio's free model.

"The ∇Y₂ rows of Θ are zero", or Θ = [T₁₁ T₁₁B₂′; 0 0], comes from exactly one place:
**Definition 3 + Corollary 2 of the appendix**.

- It is **(a) an assumption of that appendix**: MA only in the w equation, white η, and Δz₂ driven
  only by lagged w. No economic motivation is offered (nothing about exogenous drivers or absence of
  feedback). The only justification is the label "based on Phillips' (1991) triangular
  representation" (`:111`), and Phillips does not impose it.
- It is then **(b) derived** as a consequence in VEC coordinates: Cor 2 gives ε, and drvec's T6b
  regroups it into Θ̃. The derivation is correct (§2).
- It is **not (c)** the article's estimation parameterisation. The article says so, and its software
  uses a full VAR with MA on the **other** block.

The presentation backup (`:662-663`) describes Phillips' optimality result as holding "in the
restricted triangular system (φ₁₂=0, z₂ₜ follows a pure random walk)". That misstates Phillips
(1991), whose u₂ is a general stationary process. It is also stronger than Def 3, where Δz₂ may
depend on lagged w.

---

## 2. Corollary 2 (MA inheritance) and Theorem 1, line by line

### 2.1 Theorem 1, direct direction (proofs `:99-136`, proof `:142-261`)

- **Step 1** (`:143-157`): Δw = (Φ₁−I)w₋₁ + Σ_{j≥2}Φⱼw₋ⱼ + a. ✓
- **Step 2** (`:159-180`): Δz₁ = β′Δz₂ + Δw. Substituting gives the w₍ₜ₋₁₎ coefficient β′Ψ₀ + Φ₁ − I,
  which is the top block of M₁. ✓ (The −I_r was missing in the original draft. The fix is correct.)
- **Step 3** (`:182-198`): Δz = Σ_{l=1}^{p*} M_l w₍ₜ₋ₗ₎ + ε with p* = max(p,k+1). The M_l for l ≥ 2 are
  (β′Ψ_{l−1} + Φ_l; Ψ_{l−1}). ✓
- **Step 4** (`:200-228`): w₍ₜ₋ₗ₎ = w₍ₜ₋₁₎ − α′Σ_{i<l}Δz₍ₜ₋ᵢ₎, then exchange the sums. ✓
- **Step 5** (`:230-246`): A = ΣM_l and Γᵢ = −Σ_{l>i}M_lα′. ✓

Verified by exact simulation in s1 with contemporaneously correlated (a,η), for
(m,r,p,k,q) ∈ {(2,1,1,0,1), (2,1,2,1,1), (3,1,2,1,2), (4,2,3,2,1), (5,3,2,3,3), (2,1,1,0,0), (3,2,1,0,1)}.
The relative error is ≤ 7.4e−15. Also verified: rank Γᵢ ≤ r and Γᵢα⊥ = 0 exactly.

**Hypotheses:** (a,η) jointly white noise (contemporaneous correlation allowed, no lagged
cross-correlation), and w stationary. The appendix wording "zero-mean innovations (possibly
correlated)" (`:71-72`) is weaker than what is needed. The original draft stated it correctly
(`BVECM_models_proofs.original.tex:196`: "zero-mean white noise processes … with cross-covariance
Σ_aη").

### 2.2 Theorem 1, converse (`:137-139`, Step 6 `:263-292`)

**Statement:** "any VEC representation with Π = Aα′ of rank r can be written in WARMA form". **FALSE.**
Every Thm-1 image has Γᵢ = N_iα′, so Γᵢα⊥ = 0. The VEC(2) with M = 2, r = 1, A = (−0.3, 0.2)′,
β = 1 and Γ₁ = [[0.3,0.2],[−0.1,0.25]] has the following properties:
- It is a valid I(1) rank-1 process. The companion moduli are 1, 0.49, 0.49, 0.39.
- Γ₁α⊥ = (0.5, 0.15)′ ≠ 0 and rank Γ₁ = 2 (s2a).
- Its VAR representation is unique: projection coefficients are unique for a non-degenerate
  process. So no Def-3 representation exists.

This confirms the same-day finding.

**The proof** fails at three places:
1. "exploiting the triangular structure, one can express the right-hand side solely in terms of
   lagged w and Δz₂" (`:284-285`). That is possible only if Γᵢα⊥ = 0, which is what is to be
   proved.
2. "Φ₁ = α′A + I_r" (`:289`). From Thm 1, α′M₁ = Φ₁ − I and α′M_l = Φ_l, so α′A + I = Σ_lΦ_l.
   This equals Φ₁ only when p = 1. s1: |α′A+I−Φ₁| = 0.20–0.43 for p ≥ 2.
3. "Define γ = A₂" (`:269`). A₂ = Σⱼ Ψⱼ, which equals γ = Ψ₀ only when k = 0. s1: |A₂ − γ| = 0.07–1.5.

The Apr-29 correction log (`proofs.tex:321-340`) states the recursion correctly *given* the form
of Γᵢ: "each step involves only M_{i+1} as unknown, uniquely determined since α′ has full row rank".
But that presupposes (5). The final version keeps the wrong explicit formulas.

**What is true** (the "if" of drvec T6): if Γᵢ = N_iα′ for all i, then the M_l are recovered by
M_{i+1} = −(Γᵢ − Γᵢ₊₁)-rows in α′ coordinates, and M₁ = A − Σ_{l≥2}M_l. The Def-3 parameters are
then the blocks. For a pure VAR, **(5) is necessary and sufficient**, because the VAR representation
is unique.

### 2.3 Corollary 2 (proofs `:325-352`), direct direction

The statement: same α, A, Γᵢ; innovation ε = (β′η + Θ(B)a; η)′.

The proof: "Steps 1–3 apply with a replaced by Θ(B)a; M_l unchanged because the MA enters only
through the error term". ✓ The substitution touches only the additive error in Step 1, and Steps
2–5 are linear in it.

**Hypotheses:** Θ(B) with roots of |Θ(B)| outside the circle (`:336`), and (a,η) jointly white (as
above). The corollary **stops at ε**. It does not give a VEC-MA form "Θ̃(L)Aₜ" with a white
innovation.

**drvec's T6b regrouping.** Put Aₜ = (aₜ + β′ηₜ; ηₜ). The map is invertible, with
a = A₁ − β′A₂ and η = A₂. Then
ε₁ = A₁ − Σⱼ Θⱼ(A₁,ₜ₋ⱼ − β′A₂,ₜ₋ⱼ), so Θ̃ⱼ = [[Θⱼ, −Θⱼβ′],[0,0]] = [[Θⱼ, ΘⱼB₂′],[0,0]].
It is correct (s1 checks this exact form, rel. err ≤ 7.4e−15). det Θ̃(z) = det Θ(z), so invertibility
is inherited. As M3 noted, this regrouping is **drvec's own**. The article never writes a Θ̃ with
zero bottom rows.

### 2.4 Corollary 2, converse (`:349-351`)

"Every VEC model with MA error structure can be transformed into the WARMA form
(warma_ma)–(dz2_ma) via the constructive procedure of Mauricio (2005)". **FALSE.**

Mauricio's procedure maps a VEC-MA to a **general** VARMA on Ỹ, with full Φ\*, full Θ\* and a
full Σ\*. It does not map it to the Def-3 form. Counterexample (s2b): take the VEC(1) with
A = (−0.4, 0.1)′, β = 1, Θ₁ = [[0,0],[0,0.6]], Σ with correlation 0.3, and 2·10⁵ observations.
Regressing Δz₂ₜ on 8 lags of Δz₂ and 8 lags of w gives t-statistics on the Δz₂ lags of
−216, −114, −66, −39, −22, −13, −6.8, −3.3. Definition 3 forces these to 0 (Δz₂ depends on lagged w
only, with a white innovation). The original draft (`proofs.original.tex:267`) at least
conditioned the inversion on "the usual identification conditions (Reinsel 1997, ch. 6)", but
it still made the same overclaim.

### 2.5 drvec Theorem 6 ("iff") and the identification gap

**Direct:** ✓ (§2.1, §2.3).

**"If" with (5),(6):** ✓ (M3). Given (6), Θ̃Aₜ₋₁ = (Θ_w aₜ₋₁; 0), and the top/bottom rows rebuild
Def 3 (DEMOSTRACIONES T6 proof).

**"Only if" without identification: FALSE**, with an explicit counterexample (s2c). Take a
triangular VEC(1)-MA(1): φ = 0.6, θ = 0.5, γ = 0.2, β = 1. Left-multiply the levels VARMA by
(I − GL), G = [[0.2,0.1],[0.3,0.4]]. This gives a VEC(2)-MA(2) of the **same process**:
- Π_new = (I−G)Aα′ has rank 1 with the same α.
- The MA is invertible: companion moduli 0, 0.1, 0.5, 0.5.
- AR roots are unchanged except for the added stable factor.
- Simulated from the same innovations, the paths agree to 1.7e−13.

Yet Γ₁,new α⊥ = (0.3, 0.7)′ ≠ 0, which violates (5), and the Y₂ row of Θ₁,new = (0.3, 0.4), which
violates (6). The representation is not left-coprime, and identification (left-coprimeness plus an
echelon or Hannan-type order condition) excludes it. So the correct statement is: *"an
**identified** VEC-MA admits a Def-3 representation iff (5) and (6)"*.

This confirms the same-day finding that "the converse needs the VEC representation to be
identified". The BVECM's own converses (Thm 1 `:137`, Cor 2 `:349`) are false even with
identification (§2.2 and §2.4 are identified counterexamples).

### 2.6 The Corollary 3 / add-on side results (convergence operator)

Appendix Cor 3 (`:373-463`): D_t = (Δc; 0) − ΣM_l c₍ₜ₋ₗ₎. ✓ (Checked by hand for p* = 1:
D₁ = Δc − α₁c₋₁, D₂ = −α₂c₋₁.)

The add-on reintroduces the −I_r slip:
- Step 2 (`:545`) writes −(φ₁₁+βφ₂₁)c₋₁ for row 1. That is α₁ + 1, not α₁.
- Step 3 (`:578`) writes "−(α₁+1, α₂)′c₋₁ = −M₁c₋₁", but M₁ = (α₁, α₂)′.
- Its own Step 1 (`:495`) has it right: D₁ = c − (φ₁₁+βφ₂₁)c₋₁ = Δc − α₁c₋₁.

s3 gives the difference as exactly −c₍ₜ₋₁₎.

---

## 3. The article's empirical work: which class, what evidence, is it consistent?

### 3.1 Which class

The article estimates **the free AR class and a restricted MA class that is neither the article's
theory class nor drvec's default**.

**AR part:** a full VAR(p) on (w, Δz₂). By s2d, a full VAR(p) on Ỹ corresponds to a Mauricio VEC
with p lagged differences in which only the **last** Γ_p is restricted: its z₁ columns are zero,
because Φ̄_{p+1} = −F_pC̄⁻¹H̄ must vanish. So the full VAR(p) on Ỹ satisfies
Mauricio(levels VAR p) ⊂ full-VAR(p)-on-Ỹ ⊂ Mauricio(levels VAR p+1). It is **not** inside the
Def-3 class (5): Γ₁α⊥ ≠ 0 unless φ₁₂ + βφ₂₂ = φ₂₂ = 0. The article's own results report
significant Δz₂ → Δz₂ lags 1 and 2 (`BVECM_models.tex:601`; `results_Wheat_1848.tex:65-77`). That
is incompatible with Definition 3, whose Δz₂ equation has no lagged Δz₂.

**MA part:** θ₂₂ only, i.e. Θ\* = diag(θ on Δz₂, 0 on w). In VEC coordinates this is
Θ = [[0, βθ],[0, θ]] (s2e).

**Evidence for the restriction.** None is reported:
- No LR or Wald test of θ₁₁, θ₁₂, θ₂₁ = 0.
- No comparison against a full Θ.
- No MA estimates at all in the tables.

The only adequacy evidence is "Ljung–Box statistics show no evidence of residual autocorrelation"
(`results_Wheat_1848.tex:48`, with no numbers), and it was dropped from the main text. In the
legacy outputs, θ₂₂ = 0.71 (s.e. 0.12) on A/L, i.e. **clearly non-zero**, in exactly the entry
drvec's `-marow` fixes at 0.

### 3.2 Internal consistency (s3)

1. **g and l arithmetic:** consistent. Recomputed from ω and δ within rounding in both tables.
2. **Mean lag:** the tables use l = 1/(1−δ). The article defines l̄ = ν′(1)/ν(1) (`:411`), which for
   ν = ω/(1−δB) equals δ/(1−δ). So every reported l is **one year longer** than its own definition.
   The test "H₀: l̄ = 0 ⇔ δ = 0" (addon `:638`) matches δ/(1−δ), not 1/(1−δ).
3. **Text vs main Table 1** (`:561`):
   - "estimates of φ are all above 0.77": S/L with β̂ has δ = 0.70.
   - "g … with the exception of S/L under β=1, statistically significant": in Table 1, S/L β=1 has
     t = 3.08 (significant) and A/L β=1 has t = 1.74 (not). The sentence matches the **1848** run
     (S/L t = 1.41, A/L t = 3.58).
   - "half-lives 3 to 5 years for most pairs": ln½/ln δ gives 8.7–9.2 (A/L), 2.8–2.9 (V/L),
     2.0–2.7 (S/L) and 3.7–4.2 (P/L).
4. **Weak exogeneity of London** (`:607`): "weakly exogenous in the pairs with Vienna and Strasbourg
   (p > 0.05 under both specifications)". But main Table 2 gives V/L p = 0.025 and 0.035
   (`:585-586`). The claim matches `results_Wheat_1848` (0.059, 0.067). The presentation (`:438`)
   prints "V/L … Yes (p=0.025)". The add-on (`:409-410`) calls London "weakly exogenous (Wald
   p=0.035, borderline rejection)", which is a rejection at 5%.
5. **The weak-exogeneity test itself:** H₀ "all αᵢ(k) = 0" (Table-2 notes) is stronger than weak
   exogeneity (row i of A = Σ_kαᵢ(k) is zero). The presentation (`:354-355`) acknowledges this.
6. **Add-on numerical illustration** (`:377-403`): it labels Table-1's "AR root" (0.651) as
   φ̂₁₁(1) and Table-2's Σα₂ (0.044) as φ̂₂₁(1). It derives α̂₁ = −0.305, while Table 2 reports
   Σα₁ = −0.457 for the same fit. It also uses VAR(1) formulas on a VARMA(2,1) fit, and its setup
   says "q = 1 MA lag" (`:91`) for a model without MA (`:102-108`).
7. **Add-on algebra:**
   - Γ₁ is displayed as [[φ₁₂+βφ₂₂, 0],[φ₂₂, 0]] (`:289`, `:353`). The correct form is
     [[0, ·],[0, ·]] (its own `:266-267`).
   - The Cholesky factor p₂₁ = σ₁₂/σ₁₁ (`:221`) is taken from Σ_a but applied to u = (a₁+βa₂, a₂).
     The resulting Cov(e₁,e₂) = β(σ₁₁σ₂₂ − βσ₁₂σ₂₂ − 2σ₁₂²)/σ₁₁ ≠ 0, which is precisely the pitfall
     Treadway warns about (`CI11_notes`, after (8)).
   - "Granger non-causality z₁ ↛ z₂: α₂ = 0 **and** γ₂₂ = 0" (`:367-369`): with Γ₁'s z₁ column
     zero, the condition is α₂ = 0 alone.
8. **Two different runs:** break dates 1839/43/41/47 (main) vs 1848 (results file). The main text
   inherited prose from the 1848 run. That is the source of items 3 and 4.

---

## 4. Is there an argument for the restricted class as a DEFAULT for generic data?

**No.**

- The restricted class appears only as the appendix's vehicle for the proofs. The article states
  that it is not what is estimated (`:74-78`, `:226`, `:266`).
- The presentation lists "Full VAR vs. restricted triangular" as an **open theoretical issue**
  (`:493-497`, `:659-690`), in the direction of wanting the *more general* model covered.
- The article's specification philosophy (`:155`, from Treadway `OnCointegration.tex:117`) is
  data-driven: start at q = 0, then stepwise AR with diagnostics, then "Visible MA structure, not due
  to contributions of a few extreme residuals, can then be added by multiplicative MA
  reformulation", then pruning. That does not fit a fixed, a-priori MA zero pattern.
- The article's software default is itself a restriction (θ₂₂ only), but it is the **opposite**
  one. It is application-specific (bivariate price pair, `datamat` columns hard-wired). It is never
  argued for in the text and never tested.
- drvec's own `LEGACY_NOTES.md §4` read the legacy structure as "specific to the triangular form
  where the MA only corrects the Δz₂ equation". drvec's later "triangular class" (-warma) puts the MA
  in the **W** equation. The two programs use "triangular" for opposite MA placements.

---

## 5. Assessment for the decision

### 5.1 Facts

1. **`-warma` is a faithful implementation of the article's *theory* class** (Def 3 + Cor 2):
   Φ\* = [[0,Ψ],[0,Φ]] and Θ\* = diag(0,Θ_w). M3 verified the code, and s2e verified the zero
   pattern. In Mauricio's VEC coordinates this class is:
   - Γᵢ = Fᵢ = NᵢB′ (each m×m Γᵢ loses m(m−r) parameters);
   - Θⱼ = [[Tⱼ, TⱼB₂′],[0,0]] (each Θⱼ loses m²−r² parameters; the cross-block is tied to B₂
     non-linearly);
   - Λ, B₂ and Σ free.

   So it **is a special case of Mauricio (2006)** with exactly those restrictions. They are a zero
   pattern in Ȳ coordinates and a β-dependent pattern in VEC coordinates.
2. **`-mawarma`** is the MA half of that class with F free. It is not a model the article states or
   estimates.
3. **`-marow`** (Θ = [T₁₁ T₁₂; 0 0], F free) **appears nowhere in the article**. It is a superset of
   the MA half of the article's theory class (−warma ⊂ −mawarma ⊂ −marow ⊂ −mafree for the MA
   part) and a drvec construction.
4. **The article's *empirical* model is outside `-marow`.** Full VAR on Ỹ plus MA on the Δz₂
   equation only gives Θ = [[0, −B₂θ],[0,θ]]. This is inside `-matri` (T₁₁ = 0) and `-mafree`.
   Mauricio's own mink–muskrat estimates also lie outside `-marow` (ESTUDIO_MAURICIO §3).
5. **The theorem-level justification cited for `-marow` does not come from the article.** "Theorem
   6b = Corollary 2 of the BVECM" supports `-warma`/`-mawarma` (the MA structure *given* Def 3). It
   does not support `-marow`. It gives no reason to believe real data satisfy Def 3. Corollary 6.3,
   which is what `drvec.c:279-303`/`:8170-8186` actually rely on, is false as stated (M3 §8).
6. **drvec's own measurements** (not re-verified here) point both ways:
   - HOMOLOGATION §4q/§4r: the free Θ is poorly recovered at n = 61–250 and parks at the
     invertibility boundary. The structured class recovers θ = 0.9 with little bias.
   - HOMOLOGATION §4s and the rolling-origin comparison: when the differenced block carries MA, the
     default is misspecified (forecast loss ≈ 10% of a s.d., bands up to 9% too wide). At q = 1 the
     **free class forecasts better in 8 of 9 bank series**, because "the data want" Θ₂₂.
   - The legacy software estimated exactly that Θ₂₂ on the article's wheat data: 0.71 (s.e. 0.12),
     interior.

### 5.2 What each default gains and loses

**Default `-mafree` (Mauricio's model):**
- *Gains*
  - It is the model of the paper drvec implements.
  - It nests the article's theory class, the article's empirical MA class and Mauricio's example.
  - Every restricted rung becomes an LR-testable nested hypothesis.
  - The rank-test theory cited (Yap–Reinsel) is for Θ unrestricted.
  - It imposes no a-priori exclusion of MA in the common-trend block.
- *Loses*
  - Identifiability and numerical behaviour at small n (4 MA parameters vs 1–2 for M=2, r=1).
  - Boundary fits (T₂₂ → 1, overdifferencing).
  - Rank-condition failures can occur in any direction, not only through the r×r block.

**Default `-marow`:**
- *Gains*
  - Parsimony.
  - It removes the T₂₂ → I route (the only surviving true content of C6.3).
  - Measured estimability on the bank.
- *Loses*
  - Faithfulness to both sources: it is neither Mauricio's model nor the BVECM's theory or empirical
    model.
  - It misspecifies any process whose common-trend block has its own MA. That is precisely the
    article's empirical specification, Mauricio's example, and, by drvec's own measurements, most of
    the bank at q = 1.
  - Its documented justification rests on C6.3, which is false, and on a BVECM citation that
    supports a different class.

**Default `-warma` (the article's theory class):**
- *Gains:* it is the only rung that is faithful to a stated model of the article, with a closed-form
  inverse.
- *Loses:* it adds the AR restriction (5). drvec's §4i found the inherited class rejected in 8 of
  11 cases, although that was measured against a boundary alternative, and §4p did not reject the AR
  half in 7 of 8 pairs. The article's own wheat results contradict it (significant Δz₂ → Δz₂ lags).

### 5.3 Recommendations

These are kept separate from the facts above.

1. Whatever the default, **the documentation should stop attributing `-marow` to the BVECM**. The
   BVECM's theory class is `-warma`. Its estimated class is "full VAR + MA on Δz₂ only", which is
   closest to `-matri` with T₁₁ = 0. Its converses (Thm 1 `:137`, Cor 2 `:349`) are false.
   DEMOSTRACIONES T6 needs "identified" in the "only if".
2. If a restricted default is kept, it should be justified **only** by the HOMOLOGATION
   measurements, declared as drvec's own restriction, and accompanied by an automatic LR (or IC)
   comparison against `-mafree` and `-matri`. The HOMOLOGATION q = 1 forecasting result and the
   legacy θ₂₂ are direct evidence that `-marow`'s zero is often wrong on price data.
3. The article's methodology (`:155`) favours specification by evidence: fit the ladder and choose
   by test and diagnostics, rather than imposing a zero pattern by default. On the article alone,
   `-mafree` as the default, with `-marow`/`-matri`/`-warma` as tested simplifications, is the
   choice most consistent with both Mauricio and the BVECM. The measured small-sample cost of the
   free class is a real argument the other way, and the user has to weigh it.

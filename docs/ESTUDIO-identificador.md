# The identifier in atsw-gui — an integration study

**Status:** study, 2026-10-04. Nothing is built yet. The way in is decided
(option 4, section 4). Section 9 lists the decisions still open.

**What prompted it.** On 2026-10-04 the analyst decided that ART enters the GUI
(`TODO.md`, "ART entra en el GUI"). That note left two questions open: *which*
version, and *how* it enters. The analyst added a requirement:

> Identification can be partial, and happen at different points of the
> process: it can start from the identification graphs or from the models.

This study answers both questions under that requirement. It draws on three
sources:

- **ART_18.2's C engine.** The repo is `davidesg/art-identifier-c`, tag
  `ART_18.2`.
- **art-python.** It is the reference implementation of the method.
- **atsw-gui.** Its engine contract, `lib/`, its GUIs and its rules
  (`CONTRATO.md`, `DISENO-repositorio.md`, `AUDITORIA-art.md`).

---

## 1. What is decided already, and what it constrains

| decision | where | consequence for the identifier |
|---|---|---|
| ART enters the GUI | `TODO.md`, 2026-10-04 | this study |
| The GUI is the instrument of **full control** and of teaching (P4) | `DISENO-madre.md` | it shows evidence, and the analyst decides |
| No auto-ARIMA in the GUI: *"a full-control GUI should not have a button that decides for you"* | `AUDITORIA-art.md` §5 | the identifier **proposes** a ranked list with its evidence. It never estimates on its own. |
| This school identifies **visually**: ACF, PACF, mean against standard deviation | `AUDITORIA-art.md` §4 | the core of the window is the empirical correlogram beside the theoretical one of each candidate |
| Formal tests (ADF/KPSS, MEG) enter the GUI | `TODO.md`, 2026-09-26 | ART_18.2 has ADF/KPSS in C, already checked against statsmodels: they come with it |
| Engines are subprocesses, never linked libraries | `gui/atsw/Makefile`, `lib/engine` | the identifier is an engine with a CLI |
| C engines live only in atsw-gui; they enter by `git subtree add`, with history | `DISENO-repositorio.md` §4, §9 | `engines/art` enters by subtree, and the standalone repo is archived afterwards |
| Derive, never overwrite; never estimate in the window | `lib/analisis/an_sugerir.c` | a chosen candidate becomes a **child `.inp`** that opens in the editor |

---

## 2. Which version, and what it is

**ART_18.2** is tagged and merged on 2026-10-04 (merge `4262838`). It is the
version to integrate. ART_19 (the neural rewrite) is frozen.

### What it does

ART_18.2 is an **identifier** of ARMA/SARIMA orders. From a series it does
four things:

1. It applies log, d and D, and removes the deterministic harmonics when D = 0.
2. It runs the HAC F test of seasonality, and ADF and KPSS.
3. It builds a **shortlist of up to 14 candidates (p,q)(P,Q)**. The candidates
   come from a small neural network (MLP) that reads the correlogram, the
   classical cut-offs of the ACF and PACF, a mixed-ARMA grid (Hannan-Rissanen
   plus AICc), the seasonal bases, and white noise when Ljung-Box admits it.
4. It **ranks** them as art-python does ("option B"). The first key is the
   similarity of the empirical ACF/PACF to each candidate's theoretical
   ACF/PACF. Within 0.04 of the best, the fewest parameters win, then pure
   models before mixed ones, then the AICc.

### What it is not

It does not estimate by maximum likelihood. The coefficients it computes
(Yule-Walker, Hannan-Rissanen) serve only to draw each candidate's
theoretical correlogram. The estimation remains fue's.

### Measured quality

| measurement | result |
|---|---|
| exact order, 12 non-seasonal models, n = 200 | **C 66 %**, art-python 66 %, pmdarima 51 % |
| true model in the shortlist | 94 % |
| seasonal battery (monthly, n = 216), exact (p,q)(P,Q) | 80 % (28 % before 18.2) |
| same top model as art-python on the same series | 86 % |
| time per series | 5–8 ms (C), against about 1.5 s for art-python |

### How it relates to art-python

The **reference is art-python**. The C follows it, as the seasonal alignment
of 18.2 shows. Three differences remain, and they belong to 18.3:

- The C generates its candidates with the MLP. art enumerates every candidate
  that passes the significance gates.
- The C's default mode is not option B. Only `--mlp-direct` returns the
  option-B list.
- The C has no D = 1 route (B2), which re-identifies on ∇ₛ.

None of them blocks the integration (section 7).

---

## 3. The requirement: identification at different points

Identification is not a single moment of the process. In art-python it
happens at least at four points (`guided_identification`,
`describe_identification`, `diagnose`). The GUI already has a place for each
of them.

### The entry points

| | where the analyst is | what is fixed | what the identifier answers | art-python |
|---|---|---|---|---|
| **E1** | the **data** (model `m00`): the raw series | nothing | λ (Box-Cox), d (ADF/KPSS), the seasonal route (F test), then orders | λ, d and seasonality nodes; `suggest_orders` |
| **E2** | the **identification graphs**: fug/`gtk_fmg` with a λ, d, D chosen | λ, d, D | the orders (p,q)(P,Q) on *that* transformed series | `identification_analysis(d, D, lam)` |
| **E3** | a **base model**: an estimated `.pre` with harmonics, interventions and a mean, but no ARMA | the base model | the ARMA orders of its **residuals**, to put on top of the base | `guided_identification(pre_path)`, call 4 |
| **E4** | an **estimated model** whose diagnosis fails (Q fails, residual ACF out of the band) | the current model | what is missing: the residual correlogram identified, the regular or the seasonal part, or both | `diagnose` + `_alternativas_desde` (a rule), and call 4 run on the residuals |

### Partiality

Partiality cuts across the four points. The analyst can fix part of the
answer and ask for the rest:

- **only the orders** (λ, d, D given): E2;
- **only the seasonal part** (p, q kept, or vice versa);
- **only what is missing on top of a model** (E3, E4).

### What already exists for each case

- **Fixing λ, d, D and asking for the orders:** both art and the C can do it.
- **Identifying on residuals:** art does it at call 4 (E3). The C can do it
  mechanically: residuals passed with d = D = 0. It needs a flag so that it
  does not remove harmonics from residuals that already had them taken out
  (section 5).
- **Fixing the regular orders and asking only for the seasonal ones (or the
  reverse):** **no tool does this today, in art or in the C.** `suggest_orders`
  has upper limits but cannot pin an order. This is new method. Under the
  "Python first" rule (new methods are defined and measured in art-python
  first), it goes to art-python first and is then carried to the C. Until then the GUI can offer it as a
  **filter** of the full list: show only the candidates whose regular part is
  the given one. That is honest, because it filters the evidence and does not
  invent any.
- **Identifying from the ACF/PACF alone, without the series:**
  - **possible** for the MLP, the cut-offs and the similarity ranking;
  - **not possible** for the AICc, Hannan-Rissanen or Ljung-Box.

  The GUI always has the series, so this mode is **not needed**, and it is
  left out.

---

## 4. How it enters: the three options of `TODO.md`, and a fourth

| option | what it is | for | against |
|---|---|---|---|
| 1. `art_gui` / `art_cli` as one more program | the ART_18 GUI launched by the madre | quick | `art_gui` reads files on its own and knows nothing of the project, lineage or derivation. It runs a nested `gtk_main` and passes pointers inside text strings. It is a second way of working inside the taller. |
| 2. Only its pieces (ADF/KPSS, seasonal test, AR factorization) | into the existing GUIs | consistent | leaves out the identification of orders, which is what the analyst decided to bring |
| 3. Cite it | — | — | already discarded |
| **4. Engine + analysis window** (recommended) | `engines/art` as an engine with a CLI, and a `lib/analisis` window, `an_identifica`, hosted by the madre and fue_gui | one way of working; derivation and lineage; reusable at E1–E4; the formal tests come with it | ART_18's engine has to be refactored (section 5) |

Option 4 contains option 2. ADF/KPSS and the seasonal test come inside the
engine, and the GUI shows them at E1, which closes part of the 2026-09-26
decision. AR factorization stays in the plan of the formal tests: it reads an
*estimated* operator, so it belongs to fue's `.out`, not to the identifier.

---

## 5. The engine: `engines/art`

### Entry

- `git subtree add --prefix=engines/art`, from `art-identifier-c`, tag
  `ART_18.2`. It is built outside Dropbox, as drvec was (§4).
- Only the engine enters:
  - **sources:** `src/model_detection.c`, `seasonal_detection.c`,
    `unit_root_tests.c`, `ARMA.c`, `root.c`, `ml_classifier.c`;
  - **the network:** `include/model_weights.h` (476 KB: the compiled MLP
    weights) with `ml/` (`train.py`, `export.py`, `model_weights.json`) for
    retraining provenance;
  - **the batteries:** `tests/benchmark_*.py`.
- `gui/`, `python/art19`, `old/`, the regressor JSON files and the
  GTK-specific build stay out.
- After the import, the standalone repo gets its last README pointing here
  and is archived.

### Refactoring before or right after entry

The ART_18 audit found that the engine **is not a clean engine yet**:

| defect | file | change |
|---|---|---|
| the entry point takes a **file name** and re-reads it up to **4 times** (seasonal test, unit roots, main load, dead path) | `model_detection.c:1532`, `seasonal_detection.c:538` | `art_identify(const double *x, int n, const ArtOptions*, ArtResult*)`: array in, transformed once, the same array to every test |
| global state: `g_mlp_direct` and the progress callback | `model_detection.c:43-47` | into `ArtOptions`; the engine becomes reentrant |
| the results only **go to stdout**: about 130 `printf`, and the benchmark scrapes text | `model_detection.c`, `seasonal_detection.c` | results in `ArtResult`, messages in `result->messages[]`, a verbosity flag |
| plots passed as **pointers inside text** (`"PLOT_DATA_READY:%p"`) | `send_plot_data` | removed: the window draws from the result |
| no coefficients or theoretical correlogram **per candidate**: only for the best one | `ModelCandidate` | each candidate carries its coefficients, its theoretical ACF/PACF, its AICc and its weight |
| dead code: the old `deseasonalize` path, Mahalanobis, `apply_weights_to_features` | `model_detection.c` | removed |
| the seasonal test accepts only s ∈ 2..12 (s = 24 or 52 fail) | `seasonal_detection.c:552` | any s ≥ 2, as art |
| a non-positive series with `--log` silently stays in levels | `transform_data` | an error, reported |
| no cancel flag | — | checked inside the loops |
| harmonic removal always runs when D = 0 and s > 1 | `:1797` | a flag for E3/E4, residuals whose harmonics are already modelled |

### The CLI and its output

It follows the contract (`CONTRATO.md`, `lib/engine`):

- no shell, and exit codes 0–4 with `engine.h`'s meanings;
- output next to the input;
- **fug's precedent:** it reads fue's `.inp` (series, λ/d/D line, frequency),
  ignores the model, and writes suffixed files so as not to overwrite fue's.

What it writes:

- **`X_art.out`**: human-readable, in the school's format. The tests (λ,
  ADF/KPSS, seasonal F), the shortlist with similarity, AICc and weight, the
  proposed candidate, and the messages.
- **`X_art.cand`**: a machine-readable listing for the window. One line per
  candidate with its orders, scores, coefficients and theoretical ACF/PACF,
  plus the empirical ACF/PACF and the band. The repository's C has no JSON
  library. A line format like `.cns`/`.dag`, read by a small reader in
  `lib/`, follows the house style. (Decision 9.4.)

Arguments:

- for E1/E2: the `.inp`, plus λ, d, D;
- for E3/E4: a **`.pre`/`.out`**. The engine reads the residuals through
  `lib/outfile` (`lee_out` already returns them), with d = D = 0 and no
  harmonic removal. The number of ARMA parameters already in the model is
  passed too, so that Ljung-Box gets its degrees of freedom.

### The MLP

The candidates depend on the network. If the MLP is removed, nothing is built
(`mlp_ok` is never non-zero). The weights are a compiled header with no
dependencies: they enter as data, with their provenance in `ml/`. Generating
candidates without the MLP (art's enumeration by gates) is 18.3. When it
lands, the MLP becomes optional, and the GUI does not change, because it
reads the same result.

### Duplicates with `lib/`

Under the §9 rule, these are measured and listed when the engine enters:

- `load_data` against `lib/datos`;
- the ACF/PACF against `lib/fugplot`'s, together with the number of lags,
  which in ART are a network input and must not change until 18.3;
- its line search against `lib/optim`. It has none.

---

## 6. The window: `lib/analisis/an_identifica`

### It is not a port of `art_gui`

From `art_gui` it keeps only two drawings: the overlay of empirical
(thick bar) and theoretical (thin bar with a dot) correlograms, and the
seasonal dummies with their bands. Its machinery stays out: the nested main
loops, the threads polled through strings, a band fixed at 2/√100, a Cancel
button that does nothing. It is an analysis window like `an_diagnosis` and
`an_sugerir`:

- it has an `AnHost`;
- it is hosted by the madre (`gui/atsw/src/main.c`, next to diagnosis and
  anomalies) and by `fue_gui` (`gui/fue/src/analisis.c`);
- it draws with `lib/fugplot` + `lib/preview`, so the graphs look like
  fug's and fue's.

### What it shows, top to bottom

1. **The point and what is fixed.** For example: *"E3 — residuals of
   IPC_ES_m04 (harmonics + 2 interventions), d = D = 0"*, or *"E2 —
   ∇ln y, D = 0, s = 12"*.
2. **The tests** (E1 only): λ, ADF/KPSS with their recommended d, the
   seasonal F with p-value and dummies. Each comes with its sentence, and none
   is a verdict. At E2 they are hidden, because λ, d and D are already given.
3. **The empirical ACF/PACF** with the ±1.96/√n band. This is the central
   graph of the school.
4. **The shortlist.**
   - Columns: candidate, pattern similarity, AICc, Akaike weight, and a mark
     for ties within 0.04.
   - Selecting a row **overlays its theoretical correlogram** on the empirical
     one. This is the teaching core: the analyst *sees* why one candidate fits
     and another does not.
   - The first row is marked "proposed", **not selected**.
5. **Partial filters:** keep the regular part (p,q) and see only seasonal
   candidates, or the reverse (section 3).
6. **Derive.** The chosen candidate goes into a **child `.inp`**.
   - It goes through `pr_deriva`, with lineage and an optional reason, and it
     is built **from the `.pre` of the base model when there is one** (E3/E4),
     as `an_sugerir` does.
   - It opens in the editor. **It is not estimated**: AUDITORIA §5.

### Where it is launched

| point | from | data passed |
|---|---|---|
| E1 | the madre, on the data (`m00`) | the series |
| E2 | `gtk_fmg`, from the graph being viewed, with its λ, d, D | the `.inp` + λ, d, D |
| E3 | the madre or `fue_gui`, on an estimated base model | the `.pre`/`.out` |
| E4 | the diagnosis window, when Q fails | the `.pre`/`.out` |

At E4, the diagnosis keeps judging and the identifier proposes. The
`dictamen` names what fails but stops before recommending (`lib/dictamen`),
and that does not change. The link is a button, *"identify the residuals"*, that
opens `an_identifica` on the same model.

---

## 7. Consistency with art-python, and what 18.3 changes

### The rule

The C follows art-python. Whatever the C shows that art does not compute is a
defect.

### Known differences, and their effect on the GUI

| difference | effect on the window | when |
|---|---|---|
| candidates from the MLP, not by gates | the ordering of the list beyond the first: on nearly white ARMA the C and art agree on the first candidate (39/40) but not on the top three | 18.3 |
| no D = 1 route (B2) | at E1 with stochastic seasonality, the window cannot offer the alternative "D = 1, re-identified". Workaround: the analyst moves to E2 with D = 1 | 18.3 |
| identification with fixed regular orders | does not exist in art either: section 3 | art first, then the C |
| art's P ≥ 1 and Q ≥ 1 rule | it belongs to fue's engine, not to the identifier: the **child `.inp`** must respect it, when deriving | at the derivation |

### Conformance

`engines/art/tests/` should include:

- the three batteries (`benchmark_c.py`, `benchmark_seasonal.py`,
  `compare_seasonal_with_art.py`) as regression with tolerances;
- a golden of `X_art.out` on the corpus series (IPC_ES, WTI, airline), byte
  for byte on Linux and with `conformidad/referencia.sh` elsewhere.

---

## 8. The work, in phases

| phase | what | where | rough size |
|---|---|---|---|
| 0 | settle the decisions of section 9 | — | — |
| 1 | refactor the engine to arrays and results (section 5), with the 18.2 batteries unchanged before and after | `art-identifier-c`, as 18.2.1 | 3–5 days |
| 2 | subtree into `engines/art`; Makefile in fue's style; CLI with `.inp`/`.pre`, `X_art.out`, `X_art.cand`; exit codes; golden tests; wire `MOTORES`, CI `DIRS`, `check-all.sh`, `atsw_programa` | atsw-gui | 2–3 days |
| 3 | `lib/` reader of `X_art.cand`; the window `an_identifica` at E2 and E3 (the simplest: λ/d/D or the model already given) | atsw-gui | 4–6 days |
| 4 | E1 (tests, λ, d, route) and E4 (from the diagnosis); derivation through `pr_deriva` | atsw-gui | 3–4 days |
| 5 | partial filters; then the fixed-order identification once art-python has it | art-python → C → GUI | later |
| — | 18.3 (candidates by gates, option B by default, B2) | `engines/art` | later, without changing the window |

The size is a rough order, not a commitment. The engine phases are the best
known; the window depends on decisions 9.2 and 9.3.

---

## 9. Decisions for the analyst

1. **The way in.** **DECIDED, 2026-10-04: option 4**, the engine plus the
   analysis window (`art_gui` as is was the alternative).
2. **The points for the first version:** E2 and E3 alone (the cheapest, with
   no tests), or all four from the start?
3. **The tests at E1:** does the window show ADF/KPSS and the seasonal F? This
   is the 2026-09-26 line ("the GUI teaches them as well"). If yes, they come
   with the engine.
4. **The machine format of the result:** a line file in the house style
   (`X_art.cand`, recommended) or JSON (which would need a library or a
   hand-written parser)?
5. **The MLP in the taller:** accepted as compiled data until 18.3 makes it
   optional, or should 18.3 come before the integration?
6. **Where the refactoring is done:** in `art-identifier-c` before the subtree
   (recommended: the batteries live there), or in `engines/art` after it?
7. **The name:** `engines/art` and the binary `art`, or something that does
   not collide with art-python's MCP server (`art-mcp`)?

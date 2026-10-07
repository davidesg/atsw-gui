# drvec — TODO (mejoras pendientes)

## MEJORA-1 — Restricciones lineales sobre `B₂` (contrastes LR sobre una fila de β)

**Estado:** HECHA el 2026-10-07 (rama `feature/fixb2row`): `-fixb2row i v`, repetible,
combinable con `-alpha`/`-weakex`. Validación: invarianza del LR a la
normalización a 10 decimales en el caso de motivación (6.2131573620 /
6.2131573621; conjunto con `-weakex` 6.2695198728 / ...729); `-fixb2row 1 v` ≡
`-fixb2 v` con s = 1; 18 comprobaciones nuevas en `tests/run_tests.sh`.
Pendiente: la generalización `-beta file` (β = Hφ) y el uso con `-lrtest`.

**Qué falta.** `-fixb2 v` fija **todas** las entradas de `B₂` al mismo valor. No hay
forma de imponer una restricción sobre **una** fila (o una combinación lineal) de
β dejando el resto libre, así que los contrastes de hipótesis sobre β distintos de
«todo `B₂` = v» sólo se pueden hacer con el Wald de exclusión.

**Por qué importa: el Wald no es invariante.** Caso de motivación (paper de
dolarización, Ecuador; ficheros en
`Dropbox/dolarization/Python/art_drvec/VECM_TRI/`): trivariante
(ln G/Y, ln IPC_USA, ln IPC_EC), Caso 3, p = 2, r = 1, `-fdhess`.

| parametrización | ℓ (r = 1) | β_USA | e.t. | Wald H₀: β_USA = −1 |
|---|---|---|---|---|
| `GY.pre PU.pre PE.pre` (normalizado en IPC_EC) | −279.8178 | −0.512 | 0.239 | 4.17, p = 0.041 |
| `GY.pre PU.pre GAPc.pre` (GAP = PE − PU, deterministas PE − PU) | −279.8178 | +0.488 (= β_USA + 1) | 0.343 | 2.02, p = 0.155 |

Mismo óptimo (ℓ, θ = 0.486, α idénticos tras la transformación), pero el Wald
cambia de rechazar al 5 % a no rechazar. El LR sería invariante.

**Propuesta (a analizar):**
- `-fixb2 i:v` — fija sólo la fila `i` de `B₂` (en el orden del `.inp`) en `v`;
  repetible. Con un valor elegido a priori el LR frente al modelo libre es χ²
  con tantos g.l. como entradas fijadas (β superconsistente).
- Generalización: `-beta file` con `B₂ = H φ + h` (restricciones lineales tipo
  Johansen), análogo a `-alpha file`.
- Combinación con `-weakex` / `-alpha` para contrastes conjuntos (p. ej. la
  Prueba C del paper: α_USA = 0 ∩ β_USA = −1, χ²(2)).

**Cosas a mirar antes de implementar:**
- identificación con `r ≥ 2` (la normalización `[I_r ; B₂]` ya fija r² entradas);
- interacción con BUG-29 (`-lrtest` con restricciones en M ≥ 3) y BUG-33
  (punto de arranque de `-fixb2`);
- que el informe declare la restricción impuesta y no publique e.t. para las
  entradas fijadas.

**Validación:** el LR de `-fixb2 PU:0` sobre `GY PU GAPc` debe ser idéntico al
de la restricción equivalente sobre `GY PU PE` (invarianza), y en un caso de
Johansen con restricción β publicada (p. ej. Johansen–Juselius 1990) reproducir
el LR del libro.

## MEJORA-2 — Regresores exógenos estacionarios (I(0)) en el VECM

**Estado:** abierta (2026-10-07).

**Qué falta.** drvec no admite variables exógenas: toda serie de entrada entra
en el sistema como endógena y en `Y` (I(1)). Un regresor estacionario que sólo
debe entrar en el corto plazo (VECM condicional / «augmented VECM», p. ej.
Johansen 1992, Pesaran–Shin–Smith 2000, Harbo et al. 1998) no tiene vía de
entrada.

**Caso de motivación** (paper de dolarización; ficheros en
`Dropbox/dolarization/Python/art_drvec/VECM_AUG/`): (ln G/Y, ln GAP) + ln BXE
(cobertura comercial, I(0) por art: SF 7.4, DCD θ̂ = 1). Sin exógenas, la única
vía es meter BXE como tercera serie del sistema: aporta su propia relación
(rango +1), el test de rango asintótico pierde potencia para separarla (con
r = 1 el vector mezcla las dos relaciones y corrige BXE, no GAP) y el escalón
diagonal del ladder no puede reproducir el univariante d = 0 de BXE (en r = 0
se diferencian todas las series). Con r = 2 sale la estructura correcta
(BXE − 0.04·G/Y, n.s.; GAP − 0.371·G/Y), pero el rango hay que imponerlo con
información univariante externa.

**Propuesta (a analizar):**
- `-exog x.pre [x2.pre ...]`: series I(0) (sus deterministas restados desde el
  `.pre`, como las endógenas) que entran como `Σ_k D_k x_{t-k}` en las
  ecuaciones de `∇Y` (contemporánea opcional, `-exoglags k`), sin ecuación
  propia; alineadas por fecha.
- Opción de entrada **restringida al espacio de cointegración** para regresores
  I(1) débilmente exógenos (modelo condicional de Johansen 1992 / Harbo et al.
  1998), que cambia la distribución del contraste de rango → tablas o bootstrap
  condicionado en `x`.
- Wald de exclusión de la exógena por ecuación y conjunto; el bootstrap del
  rango debe condicionar en la trayectoria observada de `x`.

**Cosas a mirar:** distribución del LR de rango con exógenas (Harbo et al. 1998
para I(1); con I(0) en el corto plazo la asintótica no cambia); escalón
diagonal (las univariantes pasan a ser ARIMAX: certificar contra `drtran`);
interacción con `-weakex`/`-alpha`.

**Validación:** con `-exog` vacío, idéntico al actual; con una exógena de
coeficientes nulos simulada, LR de exclusión ~ χ²; reproducir un ejemplo
publicado de VECM con exógenas (p. ej. `urca::ca.jo(dumvar=)` para la parte de
corto plazo).

# Resultados — batería de modelos de referencia (drvarma vs ART)

Compañero de `MODELS_PLAN.md`. Registra todas las pruebas realizadas, las decisiones y los
hallazgos. Muestra **2002-01 … 2019-12** (216 obs) para estimación; datos reales hasta
**2023-11** para evaluación out-of-sample. Transformación base: **log (λ=0) + ∇ (d=1)**, freq 12.
Fuente: `data/IPC.xlsx` (Sheet1).

---

## 1. Univariantes de IPC con ART (referencia, parámetros fijos)

Identificación guiada en ART (servidor MCP). Reglas aplicadas: índices → λ=0; preferir AR
(persistencia) frente a MA en empate; no simplificar hasta cerrar el modelo; eliminación de
armónicos por **LR conjunto + estacionalidad residual**, no por t individuales; MEG sobre el
último modelo y construcción incremental.

| Serie | Modelo definitivo | Notas |
|-------|-------------------|-------|
| **IPC_ES** | λ=0, d=1, D=0, armónicos k=1..6 + **AR(1)** (φ≈0.40) + μ | `cases/IPC_ES/work/IPC_ES_m10` |
| **IPC_FR** | + **SAR(1)₁₂** (Φ≈0.19), parte regular ruido blanco | MEG marcó freq 3/5 estoc.; se mantuvo determinista |
| **IPC_DE** | **AR(3)** + **SAR(1)₁₂** + armónicos | `IPC_DE_mar3sar`; ganó al estocástico-estacional en previsión |

Hallazgos metodológicos:
- **IPC_ES**: simplificar (quitar k=5) reintroducía estacionalidad residual → no se simplifica.
- **IPC_FR**: t individuales bajos pero LR conjunto rechaza eliminar k=3,4 (el SAR infla SE).
- **IPC_DE**: MEG marcó freq 1,2 estocásticas (LR 13.9/4.0). Se construyó el modelo
  estocástico (ifadf[1,2]+MA_f) iterativamente desde mar3sar; mejoraba AIC/BIC in-sample
  **pero** los MA_f → θ²≈0.90/0.93 (cuasi-cancelación de las raíces unitarias = frecuencias
  *de facto deterministas*) y la **previsión out-of-sample fue peor**. ⇒ se cerró con el
  **determinista (mar3sar)**. **Regla: MEG significativo in-sample ≠ mejor previsión; la
  cuasi-cancelación θ²→1 es la alarma y la previsión OOS es el árbitro.**

Previsión recursiva (params fijos, orígenes 12/2019→12/2021, h=1/12/24; n=25/25/24). MAPE:

| h | IPC_FR | IPC_ES | IPC_DE |
|---|-------:|-------:|-------:|
| 1 | 0.24% | 0.37% | 0.31% |
| 12 | 2.0% | 3.6% | 3.1% |
| 24 | 4.9% | 7.0% | 7.4% |

Todas infra-predicen a largo (quiebre inflacionista 2021-22); FR la más predecible.
CSVs: `cases/<serie>/work/<serie>_recursive_eval*.csv`. Script: `cases/recursive_eval.py`.

---

## 2. WTI — modelo base univariante

`cases/WTI/work/WTI_ar1.pre`. Box-Cox decide λ=0 (no por regla índice). d=1, D=0.
**ARIMA(1,1,0), sin deriva, sin estacionalidad**: (1 − 0.299·B)∇lnWTI = a, φ=0.30 (momentum),
σ̂ₐ=8.29%. Q ruido blanco ✓. Decisiones: estacionalidad del crudo considerada espuria
(desplomes de otoño; F mucho menor que los IPC); deriva no significativa → eliminada.
Caveats aceptados: JB no-normal (colas) y flag de estacionalidad residual.

---

## 3. Trivariante IPC (Grupo 1): VARMA diagonal vs completo

`data/models_group1/IPC3*.inp`. VAR(3), q=0, −deseason auto, −mean.

- **Validación (diagonal)**: el VAR(3) diagonal **factoriza exactamente** en los univariantes
  (IPC_ES diag ≡ univariante drvarma a 4 decimales en 24 meses). μ y AR coinciden con ART
  (ES φ₁≈0.42, FR regular ruido blanco, DE φ₃ marginal).
- **Completo vs diagonal**: cross-effects significativos in-sample (Wald χ²(18)=64, Q
  multivariante 252→151) con estructura causal ES→FR/DE; **pero en previsión OOS el completo
  NO mejora** (igual o peor que diagonal/univariante en h=1/12/24). Las dependencias cruzadas
  no aportan a la previsión.
- Validación cruzada de sistemas: drvarma-univariante ≈ ART-univariante.

Comparación recursiva: `cases/recursive_compare.py`, `cases/recursive_compare.csv`.

---

## 4. Pass-through WTI → IPC (bivariantes WTI + IPC_x)

`data/passthrough/WTI_IPC_*`. VAR(p), q=0, −deseason auto (WTI nunca estacional: F=1.44),
−mean. Orden estudiado 1/2/3.

### Estructura (VAR(1))

| País | φ₂₁ retardado | β contemp.=Σ₂₁/Σ₁₁ | dirección | Q(56) | elasticidad LP |
|------|--------------:|-------------------:|-----------|------:|---------------:|
| **ES** | 0.0104 (t=4.5) | 0.0154 | WTI→IPC unidir. ✓ | pasa (0.36) | **2.7%** |
| **FR** | 0.0098 (t=6.5) | 0.0075 | WTI→IPC unidir. | falla (0.006)* | **1.35%** |
| **DE** | 0.0029 (ns) | 0.0109 | sin retardado | falla (0.018)* | **1.1%** |

\* FR/DE: el VAR(p) regular + deseason armónico deja autocorrelación residual (FR necesita
SAR(1)₁₂; DE AR(3)+estacional). p≥2/3 introducen feedback espurio IPC→WTI.

- **Elasticidad de largo plazo** (1% permanente en crudo → % nivel IPC): **ES 2.7% > FR 1.35%
  > DE 1.1%**. España el más sensible al petróleo, Alemania el menos.
- **Efecto contemporáneo** (Σ₂₁) significativo en los tres; en **DE es prácticamente el único
  canal** (el retardado no es significativo).

### Verdicto de previsión (full +WTI vs diagonal sin WTI, params fijos)

**Incorporar el WTI NO mejora la previsión incondicional de inflación en ninguno de los tres
países** (diferencias mínimas; peor a h=1, marginal a h=24). Razón: para prever IPC con WTI hay
que prever el crudo (≈paseo aleatorio sin deriva) → su previsión se aplana en ~3 meses y añade
ruido, no señal; el coeficiente retardado es pequeño.

### El canal contemporáneo y opciones (no implementadas)

La **media condicional** del IPC no usa Σ₂₁ (no se conoce la innovación del crudo de t) — sí
la IRF (vía Cholesky). Σ₂₁ es el vínculo más fuerte (y para DE, el único). Para explotarlo hay
que aportar WTI contemporáneo:
1. **Previsión condicional a un escenario de WTI** (futuros/plano/estrés): añade
   (Σ₂₁/Σ₁₁)(WTIₜ − E[WTIₜ|pasado]).
2. **Nowcast** aprovechando el desfase de publicación (crudo en tiempo real, IPC con retraso):
   WTIₜ conocido → la opción operativamente valiosa y la única que rescata el pass-through de DE.
3. **Función de transferencia** con WTIₜ exógeno contemporáneo.
4. **SVAR recursivo** (WTI ordenado primero).

---

## 5. Cambio en drvarma: `-estwin N` (previsión recursiva con parámetros fijos)

`src/drvarma.c`. Nuevo flag **`-estwin N`**: estima los parámetros sobre las primeras N obs
(crudas), fija deseason+ARMA a esa ventana, y escribe `<base>.recursive` con previsiones desde
**cada origen** (params fijos) hasta el final de los datos. Permite comparar modelos a varios
orígenes sin reestimar (como ART vía fuf). Validado: el origen de entrenamiento reproduce
**exactamente** el modelo estimado solo en N. Para q=0 no requiere recomputar residuos (la
previsión no los usa). Deseason también restringido a la ventana.

---

## 6. Hallazgo transversal

En todas las pruebas (estacionalidad estocástica de DE, dependencias cruzadas del trivariante,
pass-through del crudo): **un efecto significativo in-sample no implica mejor previsión
out-of-sample**. La validación por previsión recursiva (params fijos, múltiples orígenes) es el
árbitro.

## 7. Inventario de ficheros

- Univariantes IPC: `cases/IPC_{ES,FR,DE}/work/` (.pre, .fuf, recursive_eval CSVs).
- WTI: `cases/WTI/work/WTI_ar1.{pre,fuf.inp}`, `cases/WTI/build_wti.py`.
- Trivariante: `data/models_group1/IPC3*`, `cases/recursive_compare.py`.
- Pass-through: `data/passthrough/WTI_IPC_*{.inp,_p{1,2,3}.out,_full.recursive,_diag.recursive}`.
- Scripts de evaluación: `cases/recursive_eval.py`, `cases/recursive_compare.py`,
  `cases/forecast_compare.py`.
- ART (caso Alemania añadido al repo art-python, rama `meg-review-and-de-case`; TODO de MEG en
  su `TODO.md`).

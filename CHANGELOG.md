# Control de cambios — ART

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/).
Versionado por hitos del identificador ARMA/SARIMA.

Leyenda de estado de cada entrada de 18.2:
`[ ]` pendiente · `[~]` en progreso · `[x]` hecho y validado con benchmark.

---

## [18.2] — sin publicar — Corrección estadística del identificador

Release de **mantenimiento** sobre ART_18 (18.0, esta carpeta). No incluye la
reescritura neuronal
(ART_19, ver `TODO.md`). Corrige defectos que provocaban **sobre-identificación
de órdenes** y **coeficientes distorsionados**. Plan detallado en
`TODO_ART_18.2.md`. Hallazgos originados en la revisión crítica de
`src/model_detection.c`.

### Mejorado (Improved) — desempate por parsimonia en el ranking AICc ✅

- `[x]` **Desempate por parsimonia (Box-Jenkins) entre modelos AICc-indistinguibles**
  (`rank_shortlist_by_fit`, `model_detection.c`). El top-1 lo decidía el AICc *crudo*,
  que en empates de ajuste (ΔAICc<`TAU`) elegía por ruido y solía sobre-ajustar (AR(1)
  leído como ARMA(1,1)/(2,0), etc.). Ahora, dentro de la franja `ΔAICc < TAU` (modelos
  estadísticamente equivalentes), gana el de **menos parámetros**; a igual `k`, desempata
  el mayor prior del MLP. `TAU=1.0` por defecto (override `ART_TIE_TAU`).
  *Efecto medido (300 reps, top-1):*

  | Serie | 18.0 | 18.2 | Δ |
  |---|---|---|---|
  | AR(1) φ=0.7 | 72% | **81%** | +9 |
  | MA(1) θ=0.6 | 70% | **79%** | +9 |
  | AR(2) | 4% | 2% | −2 |
  | MA(2) | 1% | 3% | +2 |
  | ARMA(1,1) | 24% | 19% | −5 |
  | ARMA(2,1) | 2% | 0% | −2 |

  Gran mejora en los modelos **puros de bajo orden** (los más frecuentes en la
  práctica) con coste pequeño en los mixtos *borderline* (donde la respuesta
  parsimoniosa suele ser defendible). Cambio **solo en C**, sobre 18.0; sin tocar
  ACF ni MLP.

### Pendiente — el agujero de orden ≥2 sigue abierto

- `[ ]` **AR(2) raíces reales (~2-4%) y ARMA(2,1) (~0-2%) siguen perdiendo** porque la
  parsimonia los simplifica y el AICc no los distingue de (1,1)/(1,0). El discriminador
  fiable es el **patrón del correlograma** (la PACF de un AR(2) corta en 2; la de un
  ARMA tiende a cero), pero la ambigüedad *corte vs cola* hace que un suelo por PACF
  rompa el MA. Es trabajo de identificación más fino (parsimonia *acotada por el
  correlograma*) — candidato a un experimento aparte / parte de la reescritura ML
  (ART_19). El MLP **no** sirve como prior global: es anti-parsimonioso y hunde los
  AR/MA puros (probado: AR(1) 73%→18%).

### Probado y REVERTIDO — no mejoró el benchmark

> Registro honesto para no repetir el experimento. **Revertido a 18.0** (`git
> checkout` de `src/model_detection.c`, `ml/train.py`, `include/model_weights.h`).

Se implementó: (a) ACF muestral insesgada/PSD `cov/n` en C **y** Python; (b) corte
real en `determine_effective_orders`; (c) sin reescalado de coeficientes (raíces
deciden); (d) **MLP reentrenado** sobre ACF/PACF **teórica exacta de `ARMA.c`** (port
de ψ-weights) + sesgo `(n-k)/n` + ruido de Bartlett. Resultado (300 reps, top-1):

| Serie | 18.0 | 18.2 probado | Δ |
|---|---|---|---|
| AR(1) | 73% | 75% | +2 |
| MA(1) | 72% | 62% | **−10** |
| AR(2) | 4% | 5% | +1 |
| ARMA(1,1) | 25% | 20% | **−5** |
| ARMA(2,1) | 2% | 2% | 0 |

**Por qué falló:** el top-1 lo fija el AICc, no el MLP (reentrenar apenas movió nada);
el ruido de Bartlett (independiente lag-a-lag) no replica el ruido real correlacionado
de la ACF muestral; y la ACF con taper empeora la discriminación MA(1) vs ARMA(1,1)
vía las estimaciones que alimentan el AICc. El estimador `(n-k)` del 18.0, aunque no
sea PSD, estaba mejor afinado para este pipeline. **Lección:** no tocar la ACF/MLP sin
antes arreglar el AICc, que es el que manda.

<details><summary>Detalle de los cambios probados (referencia)</summary>

- `[revertido]` **ACF muestral sesgada / no-PSD** (`model_detection.c:396`).
  El estimador dividía la covarianza por `n-k` y la varianza por `n`, inflando las
  autocorrelaciones de lags altos por un factor `n/(n-k)` y rompiendo la
  semidefinición positiva de la matriz de autocovarianzas (Durbin-Levinson y
  Yule-Walker podían devolver valores inestables o |·|>1).
  *Cambio:* estimador estándar Box-Jenkins `Σcov / Σsq` (denominador `n` en ambos),
  en **C** (`calcular_ACF_muestral`) **y en Python** (`ml/train.py:compute_acf`).
  *Hallazgo de validación:* corregir solo C creaba un **desajuste train/inferencia**
  (el MLP se entrenó con la ACF `(n-k)`), que regresionaba MA(1) de 74%→67%. La ACF
  muestral corregida queda además en la **misma normalización `γ_k/γ_0` que la ACF
  teórica de `ARMA.c`**, de modo que la comparación empírica-vs-teórica en
  `evaluate_model_similarity` deja de ser "peras con manzanas".
  *Resolución:* reconstruir el MLP alrededor de la ACF/PACF corregida (ver más abajo).

- `[ ]` **`determine_effective_orders` tomaba el último pico, no el corte**
  (`model_detection.c:204-231`). Un único pico espurio en un lag alto fijaba un
  `p_max`/`q_max` enorme; combinado con la ACF inflada, sobre-identificaba de forma
  sistemática.
  *Cambio:* detección del lag de corte real + `break` + cota de orden máximo.
  *Efecto medido:* _(pendiente)_.

- `[ ]` **Reescalado silencioso de coeficientes "para estacionariedad"**
  (`model_detection.c:2173-2177`, `2287-2289`). Usaba `Σ|φ|<1`, condición ni
  necesaria ni suficiente; distorsionaba modelos estacionarios legítimos (p.ej.
  AR(2) φ=(0.6,0.35)) y esos coeficientes alterados se usaban para AICc y reporte.
  *Cambio:* verificación de raíces real (`check_ar_roots`/`check_ma_roots`) y
  rechazo del candidato inestable en vez de reescalarlo.
  *Efecto medido:* _(pendiente)_.

</details>

### Pendiente — correctness independiente del AICc (aún no implementado)

> Estos son fixes de corrección que **no dependen** de la ACF/MLP y se pueden hacer
> sin afectar al ranking; evaluar uno a uno con benchmark antes de incluir.

- `[ ]` **Coeficientes default mágicos al fallar Hannan-Rissanen**
  (`model_detection.c:936-937`, `1076-1079`, `2537-2538`). Inyectaba `0.3/(i+1)` y
  puntuaba el candidato por AICc sobre coeficientes ficticios.
  *Cambio:* descartar el candidato cuando la estimación falla.
  *Efecto medido:* _(pendiente)_.

- `[ ]` **Similitud de modelos de orden alto con coeficientes inventados**
  (`model_detection.c:1074-1094`). La rama `p+q+P+Q>10` evaluaba el ajuste contra la
  serie usando coeficientes default, no estimados.
  *Cambio:* estimar o saltar; no comparar contra coeficientes ficticios.
  *Efecto medido:* _(pendiente)_.

- `[ ]` **Off-by-one en diferenciación de orden d≥2** (`model_detection.c:347`).
  El término `- diff` descartaba una observación extra por cada diferencia adicional
  y dejaba el último valor obsoleto.
  *Cambio:* corregir el límite del bucle; conservar `n-d` observaciones.
  *Efecto medido:* _(pendiente)_.

### Cambiado (Changed)

- `[ ]` **Salida más clara entre "similitud de patrón" y "peso de Akaike"**
  (`model_detection.c:1547-1557`, `1744-1755`). El campo `similarity` significa cosas
  distintas en modo grid (similitud ACF/PACF ∈ [0,1]) y en `mlp_direct` (peso de
  Akaike). Se etiquetan de forma distinta en los mensajes finales.

- `[ ]` **Documentado que el AICc es CSS condicional** (comparable entre candidatos,
  no con statsmodels/pmdarima en valor absoluto).

### Notas de validación

- Línea base de referencia: **18.0** = HEAD de esta carpeta `ART_18` antes de los
  fixes (no confundir con la carpeta hermana `ART_18.1/`, una rama que no funcionó
  bien y queda descartada).
- Medición before/after con `git stash` del estado HEAD (=18.0) vs. árbol con los
  fixes. Top-1 del shortlist = orden verdadero, 100 reps, n=300.
- Cada `[x]` debe enlazar el delta de benchmark (acierto exacto y ±1 por tipo de
  modelo) que justifica el cierre del ítem.

---

## [18.0] — base (HEAD de `ART_18`, referencia del benchmark)

Punto de partida del control de cambios y referencia de comparación. Incluye, según
el historial de git:

- Identificación Box-Jenkins / SARIMA conectada a la GUI.
- Ranking por AICc con muestra común y pesos de Akaike como confianza de candidato.
- Hannan-Rissanen iterado con corrección de signo de `theta`.
- Detección estacional (rejilla SARIMA) y detección armónica.
- Tests de raíz unitaria (ADF/KPSS) y benchmark frente a pmdarima.

> La carpeta hermana `ART_18.1/` fue un intento aparte que **no funcionaba bien** y
> se descarta como base. Las versiones anteriores no tienen entrada de changelog; el
> historial está en `git log`.

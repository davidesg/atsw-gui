# TODO — ART_18.2 (release de mantenimiento / corrección estadística)

> Rama de mantenimiento sobre ART_18. **No** es la reescritura ML (eso es ART_19,
> ver `TODO.md`). El objetivo de la 18.2 es corregir los defectos estadísticos
> que hacen que el identificador **sobre-identifique órdenes** y reporte
> coeficientes distorsionados. Origen: revisión crítica de `model_detection.c`.
>
> Regla de oro: cada cambio se valida con `benchmark.py` / `benchmark_pmdarima.py`
> (acierto de orden exacto y ±1) **antes y después**, para demostrar que mejora o,
> como mínimo, no empeora.

## Fase 0 — Línea base (antes de tocar nada)

- [ ] `make clean && make cli` compila sin warnings nuevos
- [ ] Guardar línea base de `benchmark.py` (acierto exacto, ±1, ms/modelo) en
      `benchmark_results_18.0.json`
- [ ] Series de control reproducibles para diff manual:
  - [ ] AR(1) φ=0.7, AR(2) φ=(0.6,0.35), MA(1) θ=0.5, ARMA(1,1), ARMA(2,1)
  - [ ] `wti.txt` (216 obs) y `GY.txt` (68 obs)
- [ ] Anotar para cada una el orden identificado actual (para comparar tras los fixes)

## Fase 1 — Bugs de corrección estadística (prioridad ALTA)

### 1.1 ACF muestral insesgada (PSD) — `src/model_detection.c:396`
- [x] Cambiar `acf[k] = (cov/(n-k))/variance` por el estimador estándar (2026-09-30)
      `acf[k] = cov_sum / sq_sum` (mismo denominador `n` en numerador y varianza)
- [x] Quitar el clamp manual a `[-1,1]` una vez la ACF es PSD (debería cumplirse solo)
- [ ] Verificar que Durbin-Levinson (PACF) ya no produce |pacf|>1 en las series de control
- [ ] Re-benchmark: esperado ↓ sobre-identificación en lags altos

### 1.2 Corte real en `determine_effective_orders` — `src/model_detection.c:204-231`
- [ ] El bucle de p y de q debe quedarse con el **lag de corte** (último significativo
      antes de una racha sostenida de no-significancia), no con el último pico global
- [ ] Añadir `break` tras detectar el corte (hoy solo rompe en el `else`)
- [x] Acotar `effective_p_max`/`effective_q_max` a un orden razonable (p.ej. ≤ 5)
- [ ] Verificar con AR(1)/MA(1): el orden efectivo debe ser 1, no 20+

### 1.3 No falsear estacionariedad por reescalado — `src/model_detection.c:2173-2177, 2287-2289`
- [x] Sustituir el reescalado `0.95/Σ|φ|` por verificación de raíces real (2026-09-30: `contract_poly`, conserva el periodo; ver CHANGELOG)
      (`check_ar_roots` / `check_ma_roots`, ya existen)
- [ ] Si el modelo es inestable/no invertible → **rechazar** el candidato
      (no devolver coeficientes alterados)
- [ ] Confirmar que AR(2) φ=(0.6,0.35) (estacionario, Σ=0.95) ya **no** se reescala

## Fase 2 — Robustez de la estimación y el ranking (prioridad MEDIA)

### 2.1 Eliminar coeficientes "default" mágicos — `:936-937, 1076-1079, 2537-2538`
- [x] Si Hannan-Rissanen falla, **descartar** el candidato en vez de inyectar
      `phi=0.3/(i+1)` y puntuarlo por AICc sobre coeficientes ficticios
- [x] Marcar el candidato como no puntuable (`prob`/score = peor valor)

### 2.2 Rama de orden alto (`p+q+P+Q>10`) — `:1074-1094`
- [x] No comparar similitud con coeficientes inventados; estimar o saltar el modelo (se salta, y se dice)
- [ ] (Alternativa) documentar el límite y degradar con claridad

### 2.3 Off-by-one en diferenciación d≥2 — `src/model_detection.c:347`
- [x] Corregir `for (... i < params->n_points - diff ...)` → no descartar la última obs (también en D)
- [ ] Test: serie con `d=2` conserva `n-2` observaciones (hoy `n-3`)

## Fase 3 — Consistencia de la salida (prioridad MEDIA/BAJA)

### 3.1 Separar "similitud de patrón" de "peso de Akaike"
- [ ] En modo `mlp_direct`, `similarity` lleva un peso de Akaike; en grid, una similitud
      ACF/PACF ∈ [0,1]. Etiquetar cada uno distinto en los mensajes finales
      (`:1547-1557`, `:1744-1755`) para no confundir al usuario
- [ ] Documentar en la salida que el AICc es **CSS condicional** (comparable entre
      candidatos, no con statsmodels/pmdarima en valor absoluto)

### 3.2 Path de logaritmo en tests de raíz unitaria — `:1614-1617`
- [x] No mezclar escala log/nivel cuando hay valores ≤0; avisar explícitamente (`log_in_place`)

## Fase 4 — Validación final

- [ ] `benchmark.py`: acierto exacto y ±1 ≥ línea base 18.0 en todos los tipos
- [ ] `benchmark_pmdarima.py`: comparativa frente a pmdarima tras los fixes
- [ ] Monte Carlo 200 réplicas AR(1)/AR(2)/MA(1)/ARMA(1,1): tasa de sobre-identificación ↓
- [ ] Tests en `tests/` pasan (`make test` si existe)
- [ ] Actualizar `CHANGELOG.md` con resultados (antes/después) por cada fix
- [ ] Etiquetar commit: `ART_18.2`

---

## Anexo — Nota de decisión: CNN para la detección (OPCIONAL, no en 18.2)

> Evaluado para 18.2; **se deja como opción futura**, no entra en esta release.

Coste de cómputo de una CNN: **despreciable** (incluso una CNN 1D sobre serie cruda
de L≈200 son ~20–30 MFLOPs, ~0.1–1 ms; el MLP actual son ~60 kFLOPs). El coste real
es otro:

1. **Despliegue/dependencias (caro):** ONNX Runtime (`-lonnxruntime`) son +15 MB y
   enlazado dinámico → **rompe `build_windows_static.sh`**. Alternativa: escribir
   conv1d a mano en C (~200–400 líneas) y replicar *exactamente* el preprocesado de
   entrenamiento (fuente clásica de bugs train/inference).
2. **Datos/entrenamiento (moderado):** dataset con longitudes variables, near-unit-root,
   outliers/breaks + invarianza a escala/nivel.
3. **Beneficio marginal (dudoso para ARMA puro):** para ARMA lineal gaussiano la
   ACF/PACF son estadísticos casi suficientes; una CNN sobre serie cruda en gran parte
   re-aprende la autocorrelación que ya se le da hecha. El beneficio aparece sobre todo
   en datos reales sucios (no linealidad, heterocedasticidad, rupturas).

**Recomendación si algún día se hace:** CNN 1D sobre el **vector ACF/PACF** (L≈40),
no sobre la serie cruda → ~400 kFLOPs, embebible como header (sin ONNX, sin romper
Windows), reutiliza el pipeline ACF/PACF actual. Captura la mayor parte del beneficio
sin el coste de despliegue.

**Prerrequisito:** carece de sentido entrenar nada sobre la ACF actual hasta cerrar
los fixes 1.1 (ACF insesgada) y 1.2 (corte real): hoy la red se alimenta de una ACF
inflada y ninguna arquitectura corrige eso.

## Criterios de aceptación (definition of done)

1. AR(1)/MA(1) puros se identifican con orden **1**, no inflado.
2. Ningún coeficiente reportado proviene de un default mágico ni de un reescalado silencioso.
3. La ACF muestral es PSD (PACF dentro de `[-1,1]` sin clamp).
4. El benchmark no empeora en ningún tipo de modelo y mejora en los AR/MA puros.
5. `CHANGELOG.md` documenta cada cambio con su justificación y su efecto medido.

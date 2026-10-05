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

- [x] `make clean && make cli` compila sin warnings nuevos en `src/` (2026-10-04; los de `cli/main_cli.c` y `src/root.c` (`dxold`) son anteriores)
- [x] ~~Guardar línea base de `benchmark.py`~~ — la de la 18.0 no se guardó; la línea base es la del 2026-10-01 (`tests/results_three_engines.txt`) (acierto exacto, ±1, ms/modelo) en
      `benchmark_results_18.0.json`
- [x] ~~Series de control para diff manual~~ — sustituidas por las baterías reproducibles con semilla (`benchmark_c.py`, `benchmark_seasonal.py`):
  - [x] AR(1) φ=0.7, AR(2) φ=(0.6,0.35), MA(1) θ=0.5, ARMA(1,1), ARMA(2,1)
  - [x] `wti.txt` (216 obs) y `GY.txt` (68 obs)
- [x] ~~Anotar para cada una el orden identificado actual~~ (las baterías lo registran) (para comparar tras los fixes)

## Fase 1 — Bugs de corrección estadística (prioridad ALTA)

### 1.1 ACF muestral insesgada (PSD) — `src/model_detection.c:396`
- [x] Cambiar `acf[k] = (cov/(n-k))/variance` por el estimador estándar (2026-09-30)
      `acf[k] = cov_sum / sq_sum` (mismo denominador `n` en numerador y varianza)
- [x] Quitar el clamp manual a `[-1,1]` una vez la ACF es PSD (debería cumplirse solo)
- [x] Verificar que Durbin-Levinson (PACF) ya no produce |pacf|>1 (2026-10-04: 1800 series con AR casi unitario, paseos aleatorios y MA casi no invertibles, n=30-200, 40 retardos: máx |pacf| = 0,992; igual a statsmodels `ldb` a 4e-13)
- [x] Re-benchmark: sobreidentificación media 7 % (`benchmark_c.py`)

### 1.2 Corte real en `determine_effective_orders` — `src/model_detection.c:204-231`
- [x] El bucle de p y de q debe quedarse con el **lag de corte** (último significativo
      antes de una racha sostenida de no-significancia), no con el último pico global
- [x] Añadir `break` tras detectar el corte (el `break` del `else` ya es el corte: la primera racha de tres retardos tranquilos; comprobado 2026-10-04)
- [x] Acotar `effective_p_max`/`effective_q_max` a un orden razonable (p.ej. ≤ 5)
- [x] Verificar con AR(1)/MA(1): el orden efectivo debe ser 1, no 20+ (2026-10-04: 1 en el 82-91 % de los AR(1) y el 76-78 % de los MA(1), 0 en el 85 % del ruido blanco; el resto, retardos significativos por azar)

### 1.3 No falsear estacionariedad por reescalado — `src/model_detection.c:2173-2177, 2287-2289`
- [x] Sustituir el reescalado `0.95/Σ|φ|` por verificación de raíces real (2026-09-30: `contract_poly`, conserva el periodo; ver CHANGELOG)
      (`check_ar_roots` / `check_ma_roots`, ya existen)
- [x] ~~Rechazar~~ Contraer SOLO lo que está fuera del círculo unidad, conservando el periodo, como art-python `_contract` (BUG-0198, la referencia). El C contraía también raíces estacionarias por encima de 0,90/0,95 (2026-10-04: ARMA(1,1) 0,95/0,5, acierto exacto 20 → 35 %)
      (no devolver coeficientes alterados)
- [x] Confirmar que AR(2) φ=(0.6,0.35) (estacionario, Σ=0.95) ya **no** se reescala (raíces dentro: `contract_poly` no lo toca)

## Fase 2 — Robustez de la estimación y el ranking (prioridad MEDIA)

### 2.1 Eliminar coeficientes "default" mágicos — `:936-937, 1076-1079, 2537-2538`
- [x] Si Hannan-Rissanen falla, **descartar** el candidato en vez de inyectar
      `phi=0.3/(i+1)` y puntuarlo por AICc sobre coeficientes ficticios
- [x] Marcar el candidato como no puntuable (`prob`/score = peor valor)

### 2.2 Rama de orden alto (`p+q+P+Q>10`) — `:1074-1094`
- [x] No comparar similitud con coeficientes inventados; estimar o saltar el modelo (se salta, y se dice)
- [x] (Alternativa, no necesaria: se salta y se dice)

### 2.3 Off-by-one en diferenciación d≥2 — `src/model_detection.c:347`
- [x] Corregir `for (... i < params->n_points - diff ...)` → no descartar la última obs (también en D)
- [x] Test: serie con `d=2` conserva `n-2` observaciones (cada pasada resta una; `transform_data`)

## Fase 3 — Consistencia de la salida (prioridad MEDIA/BAJA)

### 3.1 Separar "similitud de patrón" de "peso de Akaike"
- [x] En modo `mlp_direct`, `similarity` lleva un peso de Akaike; en grid, una similitud
      ACF/PACF ∈ [0,1]. Etiquetar cada uno distinto en los mensajes finales
      (`:1547-1557`, `:1744-1755`) para no confundir al usuario
- [x] Documentar en la salida que el AICc es **CSS condicional** (comparable entre
      candidatos, no con statsmodels/pmdarima en valor absoluto)

### 3.2 Path de logaritmo en tests de raíz unitaria — `:1614-1617`
- [x] No mezclar escala log/nivel cuando hay valores ≤0; avisar explícitamente (`log_in_place`)

## Fase 3b — Estacionalidad coherente con art-python (2026-10-04)

Comparación del camino estacional del C con art-python (la referencia). Lo
acotado entra en la 18.2; lo demás, a la 18.3.

- [x] La F de detección es la F HAC de art (BUG-0206: identificación con HAC).
      Corregido el estimador: la carne se dividía por n y el retardo se contaba
      dos veces. Coincide con art a la precisión impresa (n = 60-400).
- [x] El contraste, siempre sobre d=1 y 100·log, como `describe` de art,
      sea cual sea la d o el log del usuario.
- [x] Sin estacionalidad detectada, P y Q siguen buscándose (el modo clásico
      los ponía a 0; art nunca los restringe).
- [x] Con D=0 y s>1, los armónicos se retiran de w antes de la ACF/PACF
      (`remove_harmonics`, la `_remove_harmonics` de art).
- [x] `--deseasonalize` pasa a ser un alias: su ruta restaba dummies de
      100·log a niveles sin `--log`, forzaba Q=0, no miraba la detección y
      fallaba con s=1.
- [x] Cada base regular de la lista corta entra también con (P,Q)=(0,0): un
      AR(1) puro no entraba si la red proponía P o Q.
- [x] El ruido blanco es candidato si Ljung-Box no rechaza (art BUG-0044/0048),
      con los retardos de fug.
- [x] Límites por defecto de la CLI: p ≤ máx(3, s/2), q ≤ 2, P ≤ 1, Q ≤ 1.
- [x] Batería estacional por ficheros: `tests/benchmark_seasonal.py`.

**A la 18.3:**
- [ ] Candidatos enumerados por las puertas (no por la red) y opción B por
      defecto; la ruta D=1 reidentificando los órdenes (B2).
- [ ] Retardos de fug y umbral 1,96/√n en los rasgos de patrón: lo usa la red
      como entrada y se entrenó con los actuales, así que va con el cambio de
      candidatos.
- [ ] La regla P≥1 y Q≥1 → Q=0 de art es una restricción del motor de fue,
      no del identificador (`suggest_orders` sí ordena (1,1)): va donde se
      estime con fue.

## Fase 4 — Validación final

- [x] Acierto exacto ≥ línea base en todos los tipos (`benchmark_c.py` y `benchmark_three_engines.py`, frente a la línea base del 2026-10-01: la de la 18.0 no se conservó)
- [x] Comparativa frente a pmdarima y art-python: `tests/results_three_engines_18.2.txt` (C 66 %, art 66 %, pmdarima 51 %)
- [x] Monte Carlo 200 réplicas AR(1)/AR(2)/MA(1)/ARMA(1,1): sobreidentificación 10 %, 2 %, 12 %, 8 % (`tests/benchmark_c.py 200 200 77`, igual que `main`)
- [x] Tests en `tests/`: no hay `make test`; las baterías corren limpias
- [x] Actualizar `CHANGELOG.md` con resultados (antes/después) por cada fix
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

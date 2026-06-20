# TODO — ART_18 → ART_19

## Fase 0: Línea base y verificación de ART_18

- [ ] Compilar `art_cli` y `art_gui` desde cero (`make clean && make cli`)
- [ ] Ejecutar test básico: serie AR(1) simulada (`-s -n 200 -p 1 --phi 0.7`)
- [ ] Ejecutar test básico: serie MA(1) simulada (`-s -n 200 -q 1 --theta 0.5`)
- [ ] Ejecutar test básico: ARMA(1,1) (`-s -n 200 -p 1 -q 1 --phi 0.5 --theta 0.3`)
- [ ] Ejecutar test con datos reales: `wti.txt` (WTI mensual, 216 obs)
- [ ] Ejecutar test con datos reales: `GY.txt` (68 obs)
- [ ] Monte Carlo pequeño: 100 réplicas AR(1) → medir precisión y tiempo
- [ ] Monte Carlo pequeño: 100 réplicas ARMA(1,1) con y sin Mahalanobis
- [ ] Documentar línea base: accuracy, ms/modelo, tasa de acierto por tipo de modelo

## Fase 1: Entrenamiento del nuevo ML (Python)

- [ ] Diseñar dataset mejorado (train_v2.py)
  - [ ] 70% sintético (ARMA/SARIMA con ruido GARCH, outliers, breaks)
  - [ ] 30% series reales (FRED, M3 competition)
  - [ ] Tamaños de muestra variables: 50–2000
  - [ ] Near-unit-root: φ ≥ 0.90
- [ ] Entrenar estimador neuronal de coeficientes (coeff_regressor)
  - [ ] Arquitectura: 86 → 256 → 256 → 128 → 14 (máx coeficientes)
  - [ ] Loss: Huber(δ=0.1), condicionado en órdenes
  - [ ] 200K muestras, 200 épocas, cosine annealing
  - [ ] Validación: MAE de coeficientes por tipo de modelo
- [ ] Entrenar métrica siamesa (similarity_scorer)
  - [ ] Arquitectura: Siamese (82 → 64 → 32 embedding), distancia coseno
  - [ ] Triplet loss: modelo correcto más cerca que incorrectos
  - [ ] 100K triplets (empírico, correcto, incorrecto)
- [ ] Entrenar extractor CNN 1D de features (feature_extractor)
  - [ ] Entrada: serie cruda de longitud variable
  - [ ] Convoluciones dilatadas (dilation 1,2,4,8,16) + global avg pool
  - [ ] Salida: embedding 128 dims
- [ ] Exportar todos los modelos a ONNX
- [ ] Evaluar en benchmark contra ART_18: precisión, velocidad, robustez

## Fase 2: ART_19 — Motor C con ONNX Runtime

- [ ] Integrar ONNX Runtime en el Makefile (`-lonnxruntime`)
- [ ] Reemplazar `extract_pattern_features()` → ONNX inference (feature_extractor)
- [ ] Reemplazar búsqueda en rejilla → ONNX inference (coeff_regressor)
  - [ ] Para cada combinación (p,q,P,Q) viable, predecir coeficientes
  - [ ] Mantener verificación de raíces (estabilidad/invertibilidad)
- [ ] Reemplazar `pattern_similarity()` → ONNX inference (similarity_scorer)
- [ ] Mantener en C: Yule-Walker, ACF/PACF teórica, tests de raíz unitaria, detección estacional
- [ ] Añadir flag `--onnx` para elegir entre motor clásico y neuronal
- [ ] Añadir salida de top-k modelos con scores de confianza
- [ ] Compilar y verificar que `art_cli` funciona con ambos motores

## Fase 3: CLI en Python (art_cli.py)

- [ ] Implementar `art_cli.py` con interfaz compatible con el CLI de C
  - [ ] Mismos flags: `-i`, `-s`, `-n`, `-p`, `-q`, `-P`, `-Q`, `-d`, `-D`, `-S`, `--phi`, `--theta`, etc.
  - [ ] Añadir `--engine` (classical|neural|auto)
  - [ ] Añadir `--top-k` para mostrar k mejores modelos
  - [ ] Añadir `--plot` para generar gráficos ACF/PACF (matplotlib)
  - [ ] Añadir `--format` (text|json|csv)
- [ ] Modo simulación Monte Carlo (`--reps`)
  - [ ] Barra de progreso con tqdm
  - [ ] Métricas agregadas: accuracy, MAE coeficientes, tiempo
  - [ ] Matriz de confusión de órdenes
- [ ] Modo análisis de archivo (`-i`)
  - [ ] Carga con pandas (CSV, TXT, Excel)
  - [ ] Detección automática de frecuencia
  - [ ] Informe detallado: modelo, coeficientes, diagnóstico
- [ ] Integración ONNX en Python (onnxruntime)
- [ ] Tests unitarios (pytest)
- [ ] Empaquetar como `pip install art19`

## Fase 4: Validación y benchmark

- [ ] Benchmark ART_18 vs ART_19 (clásico) vs ART_19 (neuronal)
  - [ ] 1000 réplicas AR(1), AR(2), MA(1), MA(2), ARMA(1,1), ARMA(2,1), ARMA(2,2)
  - [ ] 500 réplicas SARIMA(1,0,0)(1,0,0)_12
  - [ ] 50 series reales (wti.txt, GY.txt, + externas)
- [ ] Métricas:
  - [ ] % acierto orden exacto
  - [ ] % acierto orden ±1
  - [ ] MAE / RMSE de coeficientes
  - [ ] Tiempo medio por modelo (ms)
  - [ ] Similitud ACF/PACF media
  - [ ] Tasa de modelos inválidos (raíces fuera del círculo unidad)
- [ ] Tabla comparativa y gráficos
- [ ] Documentar conclusiones en Plan.md

## Fase 5: GUI (opcional, largo plazo)

- [ ] Migrar GUI de GTK+3 a Python (PySide6 o similar) — o mantener C con nuevos callbacks
- [ ] Visualización de top-k modelos
- [ ] Bandas de confianza para coeficientes
- [ ] Gráficos de diagnóstico (residuos, Q-Q plot, Ljung-Box)

---

## Notas

- El motor clásico (ART_18) se preserva íntegro como fallback y referencia.
- ART_19 en C usa ONNX Runtime; el header estático `model_weights.h` queda obsoleto.
- El CLI Python es independiente: no wrappea el binario C, reimplementa la lógica en Python.
- Todo el código nuevo va en directorios separados: `nn/` para entrenamiento, `python/` para CLI Python.

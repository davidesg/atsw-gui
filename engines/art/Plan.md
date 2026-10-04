# Plan — ART_19: Identificación Automática de Modelos ARMA/SARIMA con Deep Learning

> **Objetivo**: Evolucionar ART_18 a ART_19 reemplazando los componentes heurísticos (búsqueda en rejilla, similitud artesanal, features manuales) por redes neuronales entrenadas, manteniendo el motor clásico como fallback y referencia. Añadir un CLI en Python con las mismas capacidades más visualización y flexibilidad.

---

## 1. Arquitectura general

```
┌─────────────────────────────────────────────────────────┐
│                   ART_19 (C + ONNX)                      │
│                                                          │
│  Serie temporal                                          │
│      │                                                   │
│      ▼                                                   │
│  ┌──────────────┐    ┌──────────────────┐               │
│  │ Transformación│    │ Detección         │  ← C (GSL)   │
│  │ log, diff     │    │ estacional (HAC)  │               │
│  └──────┬───────┘    └────────┬─────────┘               │
│         │                     │                          │
│         ▼                     ▼                          │
│  ┌──────────────┐    ┌──────────────────┐               │
│  │ ACF/PACF     │    │ Tests raíz       │  ← C (GSL)   │
│  │ empírica     │    │ unitaria         │               │
│  └──────┬───────┘    └──────────────────┘               │
│         │                                                │
│         ▼                                                │
│  ┌──────────────────────────────────────┐               │
│  │  ┌────────────────────┐              │               │
│  │  │ engine=classical    │              │               │
│  │  │  - extract_features │   ← C puro  │               │
│  │  │  - grid_search      │              │               │
│  │  │  - pattern_similarity│             │               │
│  │  └────────────────────┘              │               │
│  │  ┌────────────────────┐              │               │
│  │  │ engine=neural       │              │               │
│  │  │  - feature_extractor│ ← ONNX      │               │
│  │  │  - coeff_regressor  │              │               │
│  │  │  - similarity_scorer│              │               │
│  │  └────────────────────┘              │               │
│  └──────────────────────────────────────┘               │
│         │                                                │
│         ▼                                                │
│  ┌──────────────────────────────────────┐               │
│  │ Top-k modelos + scores               │               │
│  │ (p,d,q)(P,D,Q)_s, coeficientes,      │               │
│  │  similitud, confianza                 │               │
│  └──────────────────────────────────────┘               │
└─────────────────────────────────────────────────────────┘
```

**Dos motores, un binario**: El flag `--engine classical|neural|auto` selecciona el backend. En modo `auto`, prueba el neuronal y cae en clásico si ONNX no está disponible.

---

## 2. Componentes neuronales (reemplazos)

### 2.1 Feature Extractor (reemplaza `extract_pattern_features`)

| Atributo | ART_18 (actual) | ART_19 (propuesto) |
|----------|----------------|---------------------|
| Entrada | ACF[1..12], PACF[1..12] | ACF[1..40], PACF[1..40] |
| Salida | 30 features manuales | embedding 128 (aprendido) |
| Método | Heurísticas (cortes, decaimiento, picos) | CNN 1D con dilataciones |
| Parámetros | 0 (cableado) | ~150K |
| ONNX size | — | ~600 KB |

**Arquitectura**:
```
ACF[1..40] + PACF[1..40]  (80 dims)
    │
    ├─ Conv1D(k=3, dilation=1, 32 filters) → ReLU
    ├─ Conv1D(k=3, dilation=2, 32 filters) → ReLU
    ├─ Conv1D(k=3, dilation=4, 32 filters) → ReLU
    ├─ Conv1D(k=3, dilation=8, 32 filters) → ReLU
    ├─ Conv1D(k=3, dilation=16, 32 filters) → ReLU
    │
    ├─ Concatenate → 160 dims
    ├─ Linear(160 → 128) → ReLU
    ├─ Linear(128 → 128) → ReLU
    └─ Output: 128-dim embedding
```

### 2.2 Coefficient Regressor (reemplaza búsqueda en rejilla)

| Atributo | ART_18 (actual) | ART_19 (propuesto) |
|----------|----------------|---------------------|
| Entrada | (p,q,P,Q) + grid de coeficientes | (p,q,P,Q) + ACF[1..40] + PACF[1..40] |
| Salida | Mejor combinación de la rejilla | Coeficientes directos (φ, θ, Φ, Θ) |
| Método | Grid search (0.30 → 0.10) | MLP regresor |
| Velocidad | 10–60 s/modelo | ~2 ms/modelo |
| Parámetros | 0 | ~250K |
| ONNX size | — | ~1 MB |

**Arquitectura**:
```
Input: [p, q, P, Q] + ACF[1..40] + PACF[1..40] + s  (86 dims)
    │
    ├─ Linear(86 → 256) → ReLU → Dropout(0.1)
    ├─ Linear(256 → 256) → ReLU → Dropout(0.1)
    ├─ Linear(256 → 128) → ReLU
    └─ Linear(128 → 14)   # máx 14 coeficientes (4+4+2+1)
         │
         └─ Mask: solo p+q+P+Q coeficientes son válidos
```

**Loss**: Huber(δ=0.1), que es menos sensible a outliers que MSE pero más precisa que MAE para gradientes pequeños.

### 2.3 Similarity Scorer (reemplaza `pattern_similarity`)

| Atributo | ART_18 (actual) | ART_19 (propuesto) |
|----------|----------------|---------------------|
| Entrada | Features teóricas vs empíricas | ACF_teo[1..40], PACF_teo[1..40], ACF_emp[1..40], PACF_emp[1..40] |
| Método | Pesos fijos (0.60, 0.25, 0.15) + penalizaciones ad-hoc | Red siamesa con distancia coseno |
| Parámetros | 15 constantes mágicas | ~90K |
| ONNX size | — | ~350 KB |

**Arquitectura**:
```
ACF_teo[1..40], PACF_teo[1..40]         ACF_emp[1..40], PACF_emp[1..40]
         │                                        │
         ▼                                        ▼
   ┌───────────┐                          ┌───────────┐
   │ Encoder    │  (shared weights)        │ Encoder    │
   │ 80→64→32  │                          │ 80→64→32  │
   └─────┬─────┘                          └─────┬─────┘
         │                                        │
         └──────────────┬─────────────────────────┘
                        │
                  cosine_similarity(e1, e2)
                        │
                  score ∈ [-1, 1]
```

**Entrenamiento**: Triplet loss con margen 0.2. Para cada serie empírica, el modelo verdadero (anchor positivo) debe estar al menos 0.2 más cerca que cualquier modelo incorrecto (anchor negativo).

---

## 3. Métricas de evaluación

### 3.1 Métricas primarias

| Métrica | Definición | Objetivo ART_19 vs ART_18 |
|---------|-----------|---------------------------|
| **Exact Order Accuracy** | % de réplicas donde (p,q,P,Q) coinciden exactamente | +10-15 pp |
| **Order Accuracy ±1** | % donde cada orden difiere en ≤1 | +5-10 pp |
| **Coefficient MAE** | Mean Absolute Error de φ, θ, Φ, Θ | Reducción 30-50% |
| **Coefficient RMSE** | Root Mean Squared Error | Reducción 30-50% |
| **Time per model** | Milisegundos por evaluación | Reducción 100-1000× |
| **Invalid model rate** | % de modelos con raíces fuera del círculo unidad | <2% |
| **ACF/PACF similarity** | Correlación entre ACF/PACF teórica del modelo detectado y la empírica | ≥ ART_18 |

### 3.2 Métricas secundarias

| Métrica | Definición | Notas |
|---------|-----------|-------|
| **Top-k coverage** | % de casos donde el modelo verdadero está en el top-k | k=1,3,5 |
| **Confidence calibration** | Correlación entre confianza predicha y acierto real | Ideal > 0.8 |
| **Robustez a outliers** | Accuracy con 5% de outliers vs sin outliers | Degradación < 5 pp |
| **Robustez a near-unit-root** | Accuracy para φ ≥ 0.90 | > 70% |
| **Robustez a muestras pequeñas** | Accuracy para n=50, 100, 200, 500 | Curva de aprendizaje |
| **Seasonal detection accuracy** | % de detección correcta de s y órdenes estacionales | Específico para SARIMA |

### 3.3 Benchmark design

```
Modelos a evaluar (1000 réplicas cada uno, n=200):
├── AR(1)    φ=0.7
├── AR(2)    φ=[0.5, 0.3]
├── MA(1)    θ=0.5
├── MA(2)    θ=[0.4, 0.3]
├── ARMA(1,1)  φ=0.5, θ=0.3
├── ARMA(2,1)  φ=[0.5, 0.2], θ=0.3
├── ARMA(2,2)  φ=[0.4,0.3], θ=[0.3,0.2]
├── AR(2) near-unit-root  φ=[1.3, -0.4]  (raíz ≈ 0.95)
├── SARIMA(1,0,0)(1,0,0)_12  φ=0.5, Φ=0.6
└── Series reales (WTI, GY, + FRED)
```

---

## 4. Diseño del CLI Python (`art_cli.py`)

### 4.1 Principios

- Interfaz compatible con `art_cli` de C (mismos flags).
- No wrappea el binario C: reimplementa la lógica en Python puro + ONNX Runtime.
- Procesamiento paralelo con `concurrent.futures` para Monte Carlo.
- Salida en texto, JSON o CSV.
- Gráficos opcionales con matplotlib.

### 4.2 Estructura de módulos

```
python/
├── art_cli.py              # Punto de entrada
├── art19/
│   ├── __init__.py
│   ├── engine.py            # Motor de identificación
│   │   ├── ClassicalEngine  # Reimplementación del algoritmo ART_18
│   │   └── NeuralEngine     # ONNX Runtime inference
│   ├── simulator.py         # Simulación ARMA/SARIMA
│   ├── features.py          # ACF, PACF, features
│   ├── seasonal.py          # Detección estacional (puerto de C)
│   ├── unit_root.py         # Tests ADF/KPSS (puerto de C)
│   ├── similarity.py        # Métricas de similitud
│   ├── onnx_models.py       # Carga y caché de modelos ONNX
│   ├── cli.py               # Argument parser
│   ├── report.py            # Formateo de resultados
│   └── plot.py              # Gráficos (matplotlib)
├── tests/
│   ├── test_engine.py
│   ├── test_simulator.py
│   └── test_integration.py
└── setup.py
```

### 4.3 Ejemplos de uso

```bash
# Análisis de archivo (motor clásico)
python art_cli.py -i data/wti.txt -d 1 -S 12 --engine classical

# Análisis con motor neuronal + top-3
python art_cli.py -i data/wti.txt -d 1 -S 12 --engine neural --top-k 3 --plot

# Simulación Monte Carlo
python art_cli.py -s -n 200 -p 1 -q 1 --phi 0.5 --theta 0.3 \
    --reps 1000 --engine both --output results.json --format json

# Auto-detección (prueba neuronal, cae en clásico)
python art_cli.py -i data/GY.txt --engine auto --plot
```

---

## 5. Estrategia de integración ONNX en C

### 5.1 Dependencia

```makefile
# Makefile (adición)
ONNX_LIBS = -lonnxruntime
ONNX_CFLAGS = -I/usr/include/onnxruntime

# Target ART_19
$(TARGET_CLI): $(OBJS) $(CLI_OBJ) | $(BIN_DIR)
    $(CC) $(LDFLAGS) -o $@ $^ $(GSL_LIBS) -lm -lpthread $(ONNX_LIBS)
```

### 5.2 API en C (wrapper mínimo)

```c
// include/onnx_inference.h
typedef struct {
    OrtEnv *env;
    OrtSession *session;
    OrtMemoryInfo *mem_info;
    int input_dim;
    int output_dim;
} OnnxModel;

OnnxModel* onnx_load(const char *path);
int onnx_predict(OnnxModel *m, const float *input, float *output);
void onnx_free(OnnxModel *m);
```

### 5.3 Flujo de inferencia

```c
// En model_detection.c, adaptive_grid_search():
if (use_neural) {
    OnnxModel *feat_ext = onnx_load("models/feature_extractor.onnx");
    OnnxModel *coeff_reg = onnx_load("models/coeff_regressor.onnx");
    OnnxModel *sim_scorer = onnx_load("models/similarity_scorer.onnx");

    float input_features[128];
    onnx_predict(feat_ext, acf_pacf_input, input_features);

    for (cada combinacion p,q,P,Q) {
        float input_coeff[86];
        onnx_predict(coeff_reg, input_coeff, coefficients);
        if (check_ar_roots(phi, p) && check_ma_roots(theta, q) ...) {
            float input_sim[164];
            onnx_predict(sim_scorer, input_sim, &score);
            // Actualizar best_candidate
        }
    }
}
```

---

## 6. Cronograma estimado

| Fase | Semana | Entregable |
|------|--------|-----------|
| Fase 0 | 1 | Línea base ART_18 documentada |
| Fase 1 | 2–4 | 3 modelos ONNX entrenados y validados |
| Fase 2 | 5–6 | ART_19 compilado con `--engine neural` funcional |
| Fase 3 | 7–8 | `art_cli.py` funcional con tests |
| Fase 4 | 9 | Benchmark completo y documentación |
| Fase 5 | 10+ | GUI (opcional) |

---

## 7. Riesgos y mitigaciones

| Riesgo | Prob | Impacto | Mitigación |
|--------|------|---------|------------|
| ONNX Runtime no disponible en el sistema | Media | Alto | Modo `auto`: cae en clásico. El CLI Python usa `onnxruntime` via pip. |
| El regresor de coeficientes produce modelos inestables | Media | Alto | Verificación de raíces en C como guardarraíl. Si falla, usar Yule-Walker para AR y grid search para MA. |
| Overfitting del similarity scorer a datos sintéticos | Alta | Medio | 30% de datos de entrenamiento son series reales. Evaluar en benchmark con series reales. |
| La CNN de features no supera a los features manuales | Media | Bajo | Mantener feature extractor clásico como fallback. El scoring siamesa funciona con ambos. |
| Dependencia circular ONNX ↔ GSL en compilación | Baja | Medio | ONNX se enlaza dinámicamente. Si no está, `#ifdef HAS_ONNX` desactiva el motor neuronal. |

---

## 8. Referencia: lo que NO cambia

Estos componentes de ART_18 se preservan sin modificación:

- **ACF/PACF muestral** — `calcular_ACF_muestral()`, `calcular_PACF_muestral()`
- **ACF/PACF teórica** — `calcular_ACF_PACF_SARIMA()` vía ψ-weights + Durbin-Levinson
- **Verificación de raíces** — `check_ar_roots()`, `check_ma_roots()`
- **Estimación Yule-Walker** — `estimate_ar_yule_walker()`
- **Detección estacional** — `harmonic_regression_differenced_basis()` con HAC
- **Tests de raíz unitaria** — `perform_unit_root_tests()` (ADF + KPSS)
- **GUI GTK+3** — `art_gui` se mantiene (opcional migrar a Python en Fase 5)

---

## 9. Línea base ART_18 — Resultados Fase 0

**Fecha**: 2026-06-13  
**Máquina**: Linux, `art_cli` compilado con `gcc -O2`, GSL 2.x  
**Parámetros fijos**: n=200, sin diferencias, sin estacionalidad, seed=42

### 9.1 Tests unitarios (1 réplica)

| Modelo verdadero | Detectado | Acierto | Similitud | Tiempo (ms) |
|-----------------|-----------|---------|-----------|-------------|
| AR(1) φ=0.7 | AR(1) | ✅ | 0.965 | 58.8 |
| MA(1) θ=0.5 | **AR(3)** | ❌ | 0.965 | 33.8 |
| ARMA(1,1) φ=0.5, θ=0.3 | ARMA(1,1) | ✅ | 0.946 | 13.3 |
| WTI (d=1, 216 obs) | AR(1) | — | 0.953 | 66.7 |
| GY (d=0, 68 obs) | AR(2) | — | 0.812 | 111.4 |

### 9.2 Monte Carlo (100 réplicas)

| Modelo | Precisión exacta | Similitud media | Tiempo medio (ms) | Mahalanobis |
|--------|-----------------|-----------------|-------------------|-------------|
| AR(1) φ=0.7 | **70.0%** | 0.955 | 47.2 | NO |
| ARMA(1,1) φ=0.5, θ=0.3 | **16.0%** | 0.926 | 13.1 | NO |
| ARMA(1,1) φ=0.5, θ=0.3 | **16.0%** | 0.926 | 12.7 | SÍ |

### 9.3 Hallazgos clave

1. **AR puros**: El algoritmo funciona bien (70% precisión). Yule-Walker es rápido y efectivo. El 30% de fallos son mayoritariamente AR(1) detectado como ARMA(1,1) o MA(2) — sobreparametrización leve.

2. **MA puros**: Fallo sistemático. El algoritmo prefiere AR(3) sobre MA(1) porque la similitud de Yule-Walker AR de orden alto compite con la del modelo MA verdadero. Es un problema de identificabilidad inherente al enfoque ACF/PACF: un MA(1) con θ=0.5 tiene una ACF que decae rápido (corte en lag 1), pero su PACF decae geométricamente, patrón que un AR(3) puede aproximar bien.

3. **ARMA**: Solo 16% de precisión exacta. El algoritmo sobre-identifica AR(1) en ~70% de los casos porque para φ=0.5 y θ=0.3 pequeño, el componente MA es difícil de distinguir de ruido. La similitud de Yule-Walker AR(1) frecuentemente supera a ARMA(1,1) por la penalización de parsimonia (+1 parámetro extra para ARMA).

4. **Mahalanobis**: **Cero impacto** en precisión o similitud para ARMA(1,1). La distancia de Mahalanobis en el espacio de features de 30 dimensiones no añade poder discriminatorio sobre la similitud de patrones. Esto sugiere que el espacio de features actual ya está saturado de información.

5. **Tiempos**: 13–67 ms por modelo. La búsqueda en rejilla domina el tiempo en modelos con q>0 (más combinaciones de coeficientes MA que probar). AR puros son más rápidos por Yule-Walker.

6. **Datos reales**: WTI(d=1) ≈ AR(1) con buen ajuste. GY muestra warning de raíz unitaria — el test ADF/KPSS funciona correctamente como guardarraíl.

### 9.4 Implicaciones para ART_19

Estos resultados confirman las prioridades establecidas en el Plan:

- **Prioridad #1 — Estimador neuronal de coeficientes**: El grid search no solo es lento sino que no ayuda a distinguir AR(1) de ARMA(1,1). Un regresor entrenado podría aprender las diferencias sutiles en la forma completa de la ACF/PACF.

- **Prioridad #2 — Métrica siamesa**: La similitud actual tiene un sesgo fuerte hacia modelos AR (por Yule-Walker + penalización de parsimonia). Una métrica aprendida con triplet loss podría calibrar mejor el equilibrio parsimonia/ajuste.

- **Prioridad #3 — Features profundos**: Los 30 features manuales no capturan suficiente información para distinguir MA(1) de AR(3) o ARMA(1,1) de AR(1). Una CNN sobre la ACF/PACF completa (40 lags) extraería patrones más ricos.

- **El Mahalanobis actual es prescindible**: No aporta valor incremental. En ART_19 se puede eliminar o reemplazar por el scoring siamesa.

# Plan de trabajo — batería de modelos de referencia

**Objetivo doble:**
1. **Validar drvarma** comparando sus resultados (univariante diagonal vs. completo) contra una
   referencia univariante independiente (**ART**).
2. Construir una **batería de modelos de referencia** (datos + especificaciones + salidas) para la
   futura **migración de drvarma a Python**.

Fecha de inicio: 2026-06-22.

---

## Convenciones de datos (Grupo 1)

- **Dataset:** IPC eurozona — 3 series mensuales **IPC_ES, IPC_FR, IPC_DE**.
- **Muestra:** 01/2002 – 12/2019 (216 obs), truncada antes del COVID para validación out-of-sample 2020.
- **Transformación:** logaritmos (Box-Cox λ=0) + 1ª diferencia regular (d=1). Frecuencia 12.
- **Horizonte de previsión:** 12 meses (todo 2020).
- **Ficheros:** `data/models_group1/`
  - `IPC_ES.inp`, `IPC_FR.inp`, `IPC_DE.inp` — univariantes (1 columna, formato fue-style).
  - `IPC3.inp` — trivariante (3 columnas).
  - Origen de datos crudos: `data/IPC.xlsx` (hoja Sheet1: FECHA, IPC_FR, IPC_DE, IPC_ES, …).

---

## Grupo 1 — pasos

### Paso 1 — Univariantes con **ART** (referencia)
- Estimar un modelo **univariante por serie con ART** (servidor MCP `art`).
- **Los órdenes los identifica ART** (no se fija AR(3) a priori): se toman los modelos que
  resulten del proceso de identificación de ART para cada serie.
- Guardar: especificación elegida por ART + **previsiones univariantes** (nivel + bandas) de cada serie.
- *Estado:* **pendiente** — requiere las herramientas `mcp__art__…` (servidor `art` añadido a la
  config del proyecto v.04; pendiente reiniciar la sesión para que carguen).

### Paso 2 — drvarma **VARMA diagonal** vs. univariantes ART
- Estimar con drvarma un VARMA **diagonal** (`-diagar -diagcov`, q=0) sobre `IPC3.inp`.
- Un VAR diagonal con covarianza diagonal **factoriza** en 3 univariantes independientes →
  sus previsiones deberían ser **básicamente iguales** a las de ART (validación cruzada).
- Comparar previsiones diagonal-drvarma vs. univariante-ART por serie.

### Paso 3 — drvarma **VARMA completo** vs. diagonal
- Estimar el VARMA **completo** (sin flags diagonales) sobre `IPC3.inp`.
- Comparar sus previsiones con las de la estructura **diagonal** → cuantificar el efecto de las
  **dependencias cruzadas**.

### Orden del VAR (decisión diferida)
- El **orden del VAR** se decide tras ver los univariantes de ART. **Candidato: VAR(3)**
  (el VAR(3) desestacionalizado resultó el primero con residuos ~ruido blanco y normales).
- Para que la comparación diagonal↔univariante sea exacta, el orden AR debe ser **el mismo** en
  ambos; se fijará una vez elegido.

---

## Decisiones abiertas
- ¿Desestacionalizar? El baseline es sin desestacionalizar (log+d=1). La opción `-deseason`
  (armónica) está disponible en drvarma y `D=1` (diferencia estacional) también; se decidirá si el
  grupo 1 incluye una variante desestacionalizada.
- Orden(es) AR definitivos, tras los univariantes de ART.

## Estado (2026-06-23) — ver resultados en `MODELS_RESULTS.md`
- **Paso 1 (univariantes ART): HECHO** — IPC_ES AR(1), IPC_FR SAR(1), IPC_DE AR(3)+SAR(1); + base WTI ARIMA(1,1,0).
- **Paso 2 (VARMA diagonal): HECHO** — factoriza exactamente en los univariantes (validado).
- **Paso 3 (VARMA completo vs diagonal): HECHO** — cross-effects significativos in-sample pero **no mejoran la previsión**.
- **Grupo pass-through (WTI→IPC, bivariantes): HECHO** — elasticidades ES 2.7% > FR 1.35% > DE 1.1%; WTI exógeno; **el pass-through no mejora la previsión incondicional**; canal contemporáneo no explotable sin nowcast/escenario.
- **Infra**: añadido `-estwin N` a drvarma (previsión recursiva con parámetros fijos, multi-origen); scripts de evaluación en `cases/`.

## Grupos siguientes (esbozo)
- Variantes desestacionalizadas (armónica vs. `D=1`).
- Otros datasets (p. ej. PSW) y otras estructuras (VARMA con q>0).
- Cada grupo: datos + spec + salidas drvarma + referencia ART, como casos de prueba para la
  migración a Python.

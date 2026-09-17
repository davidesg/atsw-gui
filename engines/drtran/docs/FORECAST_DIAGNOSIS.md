# Diagnosis de previsión de drtran — el multivariante se justifica si PREDICE mejor

**Tesis (Friedman, instrumentalista).** Un modelo de transferencia (multivariante)
extiende un univariante añadiéndole entradas. En muestra **nunca** ajusta peor: la
verosimilitud crece con los parámetros libres, así que un `ω` significativo o un `ℓ`
mayor son **necesarios pero NO suficientes**. La única justificación válida del
multivariante es **utilitarista**: que **prevea mejor fuera de muestra** que el
univariante. Esta diagnosis lo mide y lo contrasta.

> **Regla:** *un drtran (transferencia) se conserva sobre su univariante ⇔ mejora la
> previsión out-of-sample.* Un modelo que ajusta mejor in-sample pero no baja el
> RMSE fuera de muestra **falla el criterio**.

La matemática formal (definiciones, el test de Diebold–Mariano con su varianza HAC y
la corrección HLN, la prueba del solapamiento) está en **`docs/forecast_diagnosis.tex`**.
Este documento es el **procedimiento operativo**: cómo se hace y cómo se implementa,
para no re-verificarlo cada vez.

---

## 1. El diseño

**Previsión recursiva de parámetros FIJOS** (estilo `-estwin` de drvarma), out-of-sample:

1. **Ventana de estimación** `E` (`-estwin E`): estima el modelo **una vez** en
   `1..E`; los parámetros `θ̂_E` quedan **fijos**.
2. **Orígenes balanceados** `e = E .. n−H`: el origen de previsión **avanza un dato
   cada vez**; cada origen prevé `H` pasos con `θ̂_E` fijo. Balanceado = cada origen
   tiene `H` reales de test ⇒ **cada horizonte se puntúa con el mismo `N = n−H−E+1`
   errores** (ningún horizonte se queda sin datos).
3. **Prever el output exige prever el input.** En la transferencia,
   `ŷ(e+h|e)` usa `x̂(e+j|e)`, así que el modelo de la entrada se proyecta también.
   *(Ésta es la clave del resultado empírico: si el input es impredecible, la
   transferencia inyecta ruido a horizonte largo.)*

**Dos ejes independientes** (decisión del investigador):
- **Ventana `E`** (`-estwin`): de dónde salen los parámetros.
- **Origen `e`** (`-O`): desde dónde se prevé. SPS en tiempo real: `e = n` (el final
  actual). Evaluación: `e` rueda sobre el hold-out.

## 2. Las métricas

Error de previsión de cada modelo `M∈{A,B}` a `h` pasos desde el origen `e`:
`ε^M(e,h) = ŷ^M(e+h|e) − y(e+h)`. Por horizonte:

| símbolo | qué es |
|---|---|
| `RMSE^M(h)` | raíz del error cuadrático medio sobre los `N` orígenes |
| `r(h) = RMSE^B/RMSE^A` | ratio; `r<1` ⇔ B más preciso a horizonte `h` |
| `DM*(h)`, `p` | Diebold–Mariano (pérdida cuadrática, HAC a `h−1`, corrección HLN); `DM>0` ⇔ B mejor |
| `N` | nº de ERR (orígenes) que promedia cada `RMSE(h)` — **constante** por balanceo |
| `N_ef ≈ N/h` | bloques **no solapados**; el N efectivo del test |

**El solapamiento (por qué el DM a horizonte largo es frágil).** Las previsiones de
orígenes consecutivos a `h` pasos comparten `h−1` meses de destino ⇒ la diferencia de
pérdidas `d(e,h)` está autocorrelada hasta el retardo `h−1` (de ahí el **HAC a `h−1`**),
y el nº de observaciones **efectivamente independientes** cae a `N/h`. A `h=24` con
`N=37`, `N_ef≈1.5`: el ratio es sólido, pero el `p` del DM se apoya en poquísimos datos.
**Lectura:** el **signo y el ratio** son fiables a todo horizonte; el **`p`** es fuerte
a corto y frágil a largo. Por eso el script imprime `N_ef` junto a `N`.

## 3. Cómo se ejecuta con drtran (el how-to)

**Paso 1 — el panel de errores por (origen, horizonte) de cada modelo**, con `drtran -C`:

```bash
# A = univariante (sin transferencia, -0):
drtran Y.pre X.pre -0            -estwin E -f H -C A.csv
# B = transferencia (p.ej. b=0 r=0 s=1):
drtran Y.pre X.pre -b 0 -r 0 -s 1 -estwin E -f H -C B.csv
```

`-estwin E` estima en `1..E` (params fijos) y rueda el origen `E..n−H`; `-f H` fija el
horizonte; `-C file` vuelca el CSV `origin,horizon,actual,forecast,error`. drtran ya
imprime además la tabla `RMSE/MAE/MAPE` por horizonte de **cada** modelo.

**Paso 2 — comparar los dos (ratio + Diebold–Mariano)**, post-proceso:

```bash
python3 examples/passthrough/forecast_compare.py A.csv B.csv \
    --horizons 1,2,6,12,24 --labels uni,transfer --out cmp.csv
```

`forecast_compare.py` empareja los dos CSV por `(origen, horizonte)` y computa
`RMSE_A, RMSE_B, ratio, DM*, p, N, N_ef` por horizonte. Es la réplica, sobre los CSV
del motor C, del `sps/forecast_compare.py` de SF_MEG.

## 4. El caso empírico — pass-through petróleo→IPC (dos regímenes)

**Modelos** (datos INE índice general + WTI spot, `examples/passthrough/`):
- **A univariante:** `ln IPC = (1−0.40B)⁻¹(∇⁻¹)(a) + drift + 11 armónicos`.
- **B transferencia:** `A + ν(B)·ln WTI`, con `b=0, r=0, s=1` (ω₀≈0.016, t≈9).

`B` es **muy significativo in-sample** en ambos periodos (`ℓ` sube de −767 a −718).
Fuera de muestra, sin embargo, **el veredicto depende del régimen**:

| h | **P1 2015–19 (calmo)** ratio · p | **P2 2020–25 (turbulento)** ratio · p |
|---|---|---|
| 1 | 0.931 · 0.22 | 0.993 · 0.78 |
| 2 | 0.908 · 0.07 | 0.995 · 0.86 |
| 6 | 0.950 · 0.32 | 1.043 · 0.20 |
| 12 | 0.923 · 0.06 | 1.035 · 0.10 |
| 24 | **0.826 · 0.007** | **1.050 · 0.01** |

*(P1: train 2002–14, N=37; P2: train 2002–19, N=49; H=24.)*

- **P1 (calmo): la transferencia GANA**, más a horizonte largo (−17% RMSE a h=24). El
  crudo es informativo y su previsión (paseo aleatorio) basta.
- **P2 (turbulento): la transferencia PIERDE** a horizonte largo (+5%, DM p=0.01). El
  crudo se vuelve impredecible (crash COVID, pico Ucrania) → inyecta ruido de su propia
  previsión.

**La lección.** El valor predictivo del pass-through es **dependiente del régimen**: el
petróleo ayuda cuando los tiempos son estables y **falla cuando más lo necesitarías**
(crisis), porque en la crisis el input se vuelve impredecible. **La significatividad
in-sample (idéntica en ambos periodos) no dice nada del valor out-of-sample, que aquí
cambia de signo con el periodo.** Por eso la diagnosis se corre sobre **más de un
hold-out**; el cambio de signo de `r−1` entre regímenes *es* el hallazgo.

## 5. Para el puerto a Python — diagnosis ESTÁNDAR

En el puerto (ver [[drtran-python-port]]), esta comparación debe ser una **herramienta
de diagnosis de primera clase**, invocable al especificar cualquier drtran:

```python
diag = model.predictive_validation(
    estwin=E, horizons=[1,2,6,12,24], periods=[(2015,2019),(2020,2025)])
diag.verdict()   # ¿el multivariante bate al univariante nested? por régimen
```

Requisitos del port:
- Reutiliza el forecast de `fue`/`drvarma` (params fijos, origen desacoplado de la
  ventana) para generar el panel `ε(e,h)` de A (univariante nested, `ω≡0`) y B.
- Implementa `RMSE(h)`, `ratio`, Diebold–Mariano (HAC a `h−1` + HLN) y `N_ef` — el
  código de referencia es `examples/passthrough/forecast_compare.py` y la matemática
  `docs/forecast_diagnosis.tex`.
- El **modelo A es el propio B con la transferencia anulada** (`ω=0`), estimado en la
  misma ventana: la comparación es siempre contra el univariante **nested**, no contra
  un univariante ajeno.
- Emite el **veredicto instrumentalista** por horizonte y por régimen, con la reserva
  del `N_ef` a horizonte largo escrita en la salida.

**Homologación:** el port debe reproducir al dígito los CSV/tabla del motor C sobre
`examples/passthrough` (P1 y P2), igual que el resto del puerto usa el C como oráculo.

## Punteros
- Matemática y test: `docs/forecast_diagnosis.tex` (`.pdf`).
- Motor: `drtran -estwin/-O/-C` (ayuda `-h`, sección FIXED-WINDOW ESTIMATION).
- Post-proceso: `examples/passthrough/forecast_compare.py`.
- Origen del ejercicio (D-vs-S, Abraham–Box): `SF_MEG/empirical/FORECAST_COMPARISON.md`.

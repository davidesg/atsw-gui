# Los modelos legacy m6 — estudio y análisis de carencias

**Fecha:** 2026-07-13
**Fuente:** `../drv-source/` (`drv.c`, `m6-1`, `m6-2`, `m6-3`, y `m6-3/otro/previs/`)
**Objetivo:** que drtran pueda **estimar y prever** estos modelos, hoy escritos a mano.

Complementa a `M6-1_DECODED.md`, `M6-2_DECODED.md` y `M6-3_DECODED.md`, que decodifican
la *estructura* de cada modelo. Aquí se estudia la **maquinaria** que los hace
funcionar y se compara con lo que drtran tiene hoy.

---

## 1. Qué son

El sistema del **mercado laboral español** (datos de la EPA). Las 7 series:
**P** (población), **EA**, **EP**, **EI**, **EU**, **EC** (componentes) y **A**
(un agregado contemporáneo). El informe de previsión (`m3.out`) los publica como
*POBLACION*, *ACTIVOS*, *PARADOS*, con **nivel**, **variación trimestral** y
**variación anual**, cada una con su desviación típica. Datos **trimestrales**,
desde 1976, ~80 observaciones.

| | m | p | q | npar | datos | precisión |
|---|---|---|---|---|---|---|
| **m6-1** | 6 | 0 | 4 | 60 | diferenciados | `double` |
| **m6-2** | 6 | 0 | 7 | 51 | diferenciados | **`long double`** |
| **m6-3** (previsión) | 7 | **6** | 7 | 52 | **niveles** | **`long double`** |

---

## 2. La arquitectura: DOS programas, DOS `shootx`

Este es el hallazgo central del estudio, y no está en los decodificados.

### Estimación (`drv.c`)

- Los deterministas se **restan del nivel**; luego se **diferencia explícitamente**
  en `shootx [5.3]`, **serie a serie con su propio operador**:
  `(1−B)²` para P, EP, EI, EU, EC; `(1−B)(1−B⁴)` para EA. Todas pierden 5
  observaciones (el máximo).
- El VARMA resultante es **estacionario** (`p=0`, `q=4`): eso es lo que `elf`
  necesita.

### Previsión (`forsil.c` + `msfo.c`)

- **Otro `shootx` distinto**, con la diferenciación **metida DENTRO de Φ(B)**:
  `phi1[1][i][i]=1, phi1[4][i][i]=1, phi1[5][i][i]=−1` ⟹ `Φ_ii(B) = (1−B)(1−B⁴)`.
- El modelo pasa a estar **sobre NIVELES** y con **raíces unitarias** en el AR
  (`p=6`). Eso sería inadmisible para `elf`… pero **forsil no llama a `elf`**: solo
  ejecuta la recursión de previsión, que es una ecuación en diferencias y no exige
  estacionariedad.
- Resultado: **las previsiones salen directamente en nivel**, sin integrar nada.

> Es una idea elegante: la misma estructura, escrita dos veces, una estacionaria
> para la verosimilitud y otra no estacionaria para prever. drtran resuelve lo mismo
> por el otro camino —prevé la serie estacionaria e integra con `rnsop`—, que es
> equivalente y más automático.

### El fichero `.inp` de previsión (`m3.inp`) lleva DOS bloques

```
7  69  11          <- nser, nobs de estimación, observaciones NUEVAS
<residuos de la estimación>   (i = 6..69; se pierden las 5 primeras por diferencias)
<datos en NIVEL>              (i = 1..80; incluye las 11 nuevas)
```

`forsil [7.2]` calcula **recursivamente los residuos de las observaciones nuevas**
con los **parámetros fijos** en sus valores estimados. Es el flujo de **seguimiento**:
llegan datos nuevos, no se reestima, se actualiza la previsión.

---

## 3. Inventario de mecanismos

| # | Mecanismo | Dónde |
|---|---|---|
| 1 | **m > 2** (6 y 7 series) | todos |
| 2 | **Covarianza estructurada** (no diagonal): `Σ₄₂, Σ₆₂, Σ₅₄` | `qq1` |
| 3 | **Red de transferencias**: EC→EU→EI→EP + atajo EC→EP | `theta1[k][i][j]` |
| 4 | **Parámetros compartidos**: `x7=x6`, `x16=x11` | la transferencia hereda la dinámica de su entrada — la firma de una ω(B)/δ(B) racional |
| 5 | **Coeficientes fijos** (`Θ₃₃ = 1 + 1·B`) | `theta1[1][3][3]=1.0` |
| 6 | **Ecuación contemporánea**: Φ(0) ≠ I (la fila de A) | m6-3 |
| 7 | **Reparametrización de invertibilidad** (x → 1/x) | m6-3 |
| 8 | **Normalización estructural**: `phi[k] ← Φ(0)⁻¹Φ(k)`, `Q ← (Θ(0)⁻¹Φ(0)) Q (·)′` | bloque `[6]`, marcado *NON-USER* |
| 9 | **Diferenciación por serie**, distinta en cada una | `[5.3]` |
| 10 | **Deterministas por serie** (escalones, impulsos, impulso compensado, armónicos) | `[5.1]` |
| 11 | **Deterministas con estructura doble** (`xius` / `xims`) cuya **diferencia** alimenta la ecuación de A, ponderada por los propios coeficientes contemporáneos | forsil `[5.4]` |
| 12 | **Previsión: nivel + variación trimestral + variación anual**, cada una con su DT | `f1/v1`, `f2/v2`, `f3/v3` |
| 13 | **Agregados** (ACTIVOS = Σ, PARADOS = A − Σ) con **varianza `c′Vc`** | forsil |
| 14 | **Observaciones nuevas** tras la estimación, residuos actualizados recursivamente, parámetros fijos | forsil `[7.2]` |
| 15 | **`long double`** (m6-2, m6-3: modelos sobre-diferenciados ⇒ MA casi no invertible) | `drv.h` |
| 16 | **`multshea.c`** — Shea (1989), *AS 242*: una **segunda** implementación de la verosimilitud, alternativa a `elfvarma` | m6-* |

---

## 4. Qué tiene ya drtran

| Mecanismo | drtran |
|---|---|
| Diferenciación por serie, con operador propio | ✅ y **mejor**: `rnsop` general (d, D **y factores irreducibles**), leído del `.pre`, con recorte a ventana común |
| Deterministas por serie (todos los tipos de fue + ω(B)/δ(B) racional) | ✅ |
| Coeficientes fijos vs libres | ✅ pero **solo desde los flags del `.pre`** |
| Transferencia racional ω(B)/δ(B)·B^b, con identificación y validación | ✅ (una entrada) |
| Verosimilitud exacta, SE correctas, previsión con bandas | ✅ |
| Normalización estructural Φ(0)⁻¹ | ❌ (está en drvarma, **no** en `tran_shootx.c`) |

**El cast de drtran (`tran_shootx.c`) fija `m = 2`, `Φ(0)=Θ(0)=I` y `Q` diagonal.**
Ahí está el techo.

---

## 5. Carencias, por orden de coste

### A. Bloqueantes para los m6

1. **m > 2.** Hoy `m=2` está cableado. Es el cambio estructural mayor: el vector de
   parámetros, el cast y el reporte dejan de ser "salida vs entrada" y pasan a ser
   un sistema.
2. **Covarianza no diagonal.** Hay que parametrizar `Q` estructurada manteniendo
   definida positiva y **sin reintroducir la redundancia de escala** que costó M0.8
   (factorización de Cholesky con la diagonal normalizada).
3. **Red de transferencias + parámetros compartidos.** Es *el* mecanismo: que `x6`
   aparezca en `Θ₄₄` (dinámica de EI) y en `Θ₃₄` (EI→EP) **es** lo que hace que la
   transferencia sea racional. Sin compartir parámetros no hay ω/δ.
4. **Ecuación contemporánea Φ(0) ≠ I** y su normalización. El código ya existe en
   drvarma; hay que traerlo al cast de drtran.

### B. Baratas y valiosas ya

5. **Variación periodo a periodo y variación anual, con su DT.** El informe legacy
   da exactamente lo que un economista quiere: no solo el nivel, sino *cuánto sube*
   este trimestre y *cuánto* respecto al año pasado, con sus bandas.
   **drtran ya llama a `forecast_model`, que calcula `v2` y `v3` (varianzas de las
   diferencias) — y las TIRA.** Los pesos ψ del nivel ya están calculados
   (`U = u * ψ`); los de la variación son su diferencia. Es una tarde de trabajo.
6. **Agregados con varianza `c′Vc`.** Combinaciones lineales de las previsiones con
   su banda correcta. Trivial una vez hay `m > 2`.
7. **Observaciones nuevas sin reestimar** (`nobspls`). El flujo de seguimiento.
   Encaja con el `.pre`, que ya trae los parámetros estimados.

### C. A vigilar

8. **`long double`.** m6-2 y m6-3 lo usan porque están **sobre-diferenciados** y su
   MA queda casi no invertible: `double` puede no bastar. drtran usa `real = double`.
   `real` es un typedef: el cambio es de una línea, pero hay que **medirlo**, no
   suponerlo.
9. **`multshea.c` (Shea, AS 242).** Una segunda verosimilitud, independiente de
   `elfvarma`. Es una **oportunidad de validación cruzada** que no estamos usando:
   dos implementaciones distintas que deben dar la misma logL.

---

## 6. La conclusión incómoda

drtran hace hoy **un caso particular** de lo que estos modelos hacen: una entrada,
una salida, covarianza diagonal, sin ecuaciones contemporáneas. Y lo hace bien —
homologado, validado, con las SE correctas.

Pero los m6 no son "drtran con más series": son **otro programa**. Lo que drtran
automatiza (el mapa parámetros → estructura VARMA) es justo lo que en los m6 está
**escrito a mano**, y lo que hay escrito a mano es más rico que lo que drtran sabe
generar.

El camino no es ampliar el cast a base de flags, sino lo que ya apunta el TODO:
**un pequeño DSL** para declarar la estructura —qué series, qué operadores, qué
parámetros se comparten, qué es fijo, qué covarianzas son libres, qué ecuaciones son
contemporáneas— y que drtran genere el `shootx` a partir de esa declaración. Los m6
son la **especificación de requisitos** de ese DSL: si el DSL puede expresar m6-1,
m6-2 y m6-3, está terminado.

## 7. Orden que propongo

1. **Variación + variación anual con DT** en la previsión (barato, útil ya, y no
   toca el cast).
2. **Validación cruzada con `multshea`** (barato: dos verosimilitudes que deben
   coincidir; red de seguridad antes de tocar el cast).
3. **Generalizar el cast a `m > 2`** con covarianza estructurada y Φ(0) — el salto.
4. **DSL de estructura** + parámetros compartidos y fijos.
5. **Reproducir m6-1**, que es el más limpio, y compararlo con su `.out`.
6. **Seguimiento** (`nobspls`) y agregados.

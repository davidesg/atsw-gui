# Los modelos legacy m6 — estudio y análisis de carencias

**Fecha:** 2026-07-13
**Fuente:** `../drv-source/` (`drv.c`, `m6-1`, `m6-2`, `m6-3`, y `m6-3/otro/previs/`)
**Objetivo:** que drtran pueda **estimar y prever** estos modelos, hoy escritos a mano.

Complementa a `M6-1_DECODED.md`, `M6-2_DECODED.md` y `M6-3_DECODED.md`, que decodifican
la *estructura* de cada modelo. Aquí se estudia la **maquinaria** que los hace
funcionar y se compara con lo que drtran tiene hoy.

---

## 1. Qué son

El sistema del **mercado laboral español** (datos de la EPA), trimestral, desde
1976, ~80 observaciones.

| # | serie | |
|---|---|---|
| 1 | **P** | POBLACIÓN (16 y más años) |
| 2 | **EA** | OCUPADOS: agricultura |
| 3 | **EP** | OCUPADOS: servicios privados |
| 4 | **EI** | OCUPADOS: industria |
| 5 | **EU** | OCUPADOS: servicios públicos |
| 6 | **EC** | OCUPADOS: construcción |
| 7 | **A** | **ACTIVOS** (población activa) — solo en m6-3 |

## 1b. ¿Es m6 un modelo de transferencia? SÍ

**Es una RED de funciones de transferencia**, no otra cosa:

```
   P  (autónoma)
   EA (autónoma)
   EC (autónoma) ── b=2 ──► EU ── b=1 ──► EI ── b=1 ──► EP
    └──────────────────── b=1 ─────────────────────────►┘

   ACTIVOS ◄── b=0 ── P, EA, EP, EI, EU        (ecuación de participación)
```

**La fila Φ(0) de ACTIVOS NO es una identidad contable.** Sus coeficientes están
etiquetados en el propio `forsil.c` como `omega P`, `omega EA`, `omega EP`,
`omega EI`, `omega EU` —omegas, numeradores de transferencia— y valen 0.44, 0.43,
0.45, 0.52, 0.30: son **estimados**, no unos. Es una relación **behavioral** (la
participación laboral), expresada como una transferencia **contemporánea (b=0)**
con cinco entradas.

**Las identidades contables están FUERA del modelo.** `forsil` las calcula después
de prever, como combinaciones lineales de las previsiones con varianza `c'Vc`:

- *OCUPADOS: AGREGACIÓN DE COMPONENTES* = Σ(series 2..6)
- *PARADOS* = ACTIVOS − OCUPADOS = serie 7 − Σ(2..6)

**Consecuencia para drtran: Φ(0) ≠ I NO hace falta.** Restar una transferencia
contemporánea es algebraicamente lo mismo:

```
Φ(0)·w :   A_t − Σ ω_j X_j,t = N_t        (lo que hace m6)
drtran :   w[1] = w_A − Σ nu_j(B) w_Xj    con nu_j(0) = omega_0 != 0
```

Es el mismo modelo con otra contabilidad. drtran ya lo hace (IPC ← WTI tiene b=0).
m6 usó Φ(0) porque el marco de `drv.c` obligaba a escribirlo todo como matrices
VARMA, no porque hiciera falta.

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

1. ~~**m > 2**~~ ✅ **HECHO**: cast de hasta 8 series, con una transferencia por
   entrada.
2. ~~**Parámetros compartidos**~~ ✅ **HECHO**: tabla de slots (libre / fijo /
   compartido), declarada en un fichero con `-c`.
3. ~~**Ecuación contemporánea Φ(0) ≠ I**~~ ❌ **NO HACE FALTA**: es una
   transferencia con b=0, que drtran ya sabe restar (ver §1b).
4. ~~**LA RED: varias SALIDAS**~~ ✅ **HECHO**. En m6, **EU es a la vez salida**
   (de EC) **y entrada** (de EI); lo mismo EI. Ahora **cualquier** serie puede
   recibir transferencias y ser a la vez entrada de otra. Se declara con `-n`:

   ```
   OUTPUT <- INPUT   b r s
   ```

   El cast resta a **cada** serie lo que recibe (`w[i] − Σ transferencias hacia i`),
   no solo a la serie 1; el resto es su ruido, y el VARMA sigue siendo diagonal. La
   previsión recorre la red en **orden topológico** y propaga los pesos ψ del
   sistema:

   ```
   Psi_ij(B) = d_ij·psi_i(B) + SUM_{k: out=i} nu_k(B)·Psi_{inp(k),j}(B)
   ```

   de modo que el error de previsión de una serie hereda las innovaciones de **todo
   lo que tiene aguas arriba**, cada una filtrada por los ν(B) que atraviesa. Un
   **ciclo** se rechaza: sería un sistema simultáneo, y entonces no se puede
   triangularizar restando transferencias.

   Comprobado sobre una cadena sintética `X → M → Y` (§2f de la batería): recupera
   las dos transferencias y los tres ARMA, y **con los mismos 7 parámetros libres**
   bate a la estrella por 213 puntos de logL (−1038.6 vs −1251.8). En la estrella, el
   enlace directo X→Y sale insignificante (t = 1.45): el efecto de X sobre Y es
   indirecto, y solo la red puede decirlo.
5. ~~**Covarianza no diagonal**~~ ✅ **HECHO**. Es la forma *reducida* de la
   dependencia contemporánea que NO se modela como transferencia — y m6-1 la usa
   **sin tener ninguna estructura contemporánea**, lo que prueba que no son
   sustitutos. Cada `q[i,j]` es un slot **fijo en cero** que se libera uno a uno:

   ```
   q[4,2] = free
   q[6,2] = free
   q[5,4] = free
   ```

   que son exactamente las tres que m6-1 libera de sus quince. La escala sigue
   anclada con `Q[1,1] = 1` (ver BRIDGE_DESIGN §10: la verosimilitud concentrada es
   invariante ante `Q → cQ`, así que sin normalizar el hessiano es **exactamente**
   singular — y por eso los errores estándar de las sigmas que publica m6-1, que
   salen del hessiano de BFGS, no significan nada).

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

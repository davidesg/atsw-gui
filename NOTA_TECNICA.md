# Notas de ingeniería sobre modelos de transferencia estimados por máxima verosimilitud exacta

**D. E. Guerrero** — julio de 2026
Nota técnica del proyecto `drtran`. No pretende ser publicable: pretende dejar por
escrito, y medido, lo que se aprende al reconstruir hoy una maquinaria de 1970.

---

## 0. Resumen

Los modelos son los de Box y Jenkins (1970). No hay nada nuevo bajo el sol. Pero al
reconstruirlos sobre un motor de verosimilitud exacta multivariante aparecen media
docena de **decisiones de ingeniería** que se apartan de las soluciones originales —
y de las de los programas de la época— de formas que *se pueden medir*. Esta nota las
enumera, las mide, y en un caso obtiene un resultado que no esperábamos: una
transferencia contemporánea y la covarianza de las innovaciones son **el mismo modelo**
cuando el ruido y el input tienen la misma estructura AR (§4).

Todas las cifras de esta nota son reproducibles: cada afirmación remite a una sección
de `test_battery.sh` (134 comprobaciones, todas verdes).

---

## 1. El linaje

```
Box & Jenkins (1970)            los modelos
   └─ programa de Jenkins
        └─ TASTE  (Treadway)    Pascal; identificación y estimación; backcasting
             └─ fue             univariante (ARIMA/MEG con intervención)
   Mauricio (1995, 1997)        verosimilitud EXACTA de un VARMA  ── elf
        └─ drvarma              estima VARMA arbitrarios
             └─ drtran          el puente: fue → VARMA → elf
```

La tesis de **M. Silvia Muñoz Polo** (*Estudios econométricos de las series temporales
de la industria española*, UCM 2001, dirigida por **A. B. Treadway**) es el documento
que fija la metodología de la escuela, y **estima con el motor de Mauricio** («el
software que implementa el algoritmo de MVE programado y diseñado por Mauricio (1995,
1996, 1997)», cap. 3). Es decir: el linaje ya estaba unido en 2001 *a mano*. Lo que
`drtran` hace es automatizarlo.

TASTE es de Treadway y es muy probablemente muy parecido —si no idéntico en concepción—
al programa de Jenkins. Quedó obsoleto con `elf`. Pero sus soluciones a dos problemas
(el arranque de la muestra y la identificación) siguen siendo pertinentes, y en un caso
**mejores que la nuestra** (§3.6).

---

## 2. La única idea estructural: el *cast*

Un modelo de transferencia

```
Y_t = Σⱼ [ωⱼ(B)/δⱼ(B)]·B^bⱼ · Xⱼ,t  +  N_t
```

se convierte en un **VARMA diagonal** de m = 1 + n_inp series:

- serie 1 = `w_Y − Σⱼ νⱼ(B)·w_Xⱼ` — es decir, **el ruido N**;
- serie j+1 = `w_Xⱼ`;
- Φ y Θ **diagonales** (cada serie con su propio ARMA), Q diagonal.

Todo el acoplamiento vive en la transferencia **restada**. El VARMA que ve `elf` no
tiene un solo elemento fuera de la diagonal. Esa es la clave, y explica dos cosas a la
vez: por qué el motor de Mauricio se puede usar **sin tocarlo**, y por qué el resultado
homologa con `fue` ejecutado por separado sobre cada serie (§1 de la batería, a seis
decimales, sobre seis modelos reales).

Todo lo demás son consecuencias.

---

## 3. Seis diferencias de ingeniería, medidas

### 3.1 El modelo del *input*: fijo (Box‑Jenkins) o conjunto (drtran)

La doctrina, literal (tesis, §2.6):

> «El modelo U del input **permanece inalterado** desde el inicio hasta el fin del
> proceso.»

`drtran` hace lo contrario: estima **todo a la vez** —el ARMA del output, el del input,
la transferencia, las deterministas y las medias— en una sola maximización.

**¿Cambia algo? No. Y eso es el resultado.** Sobre IPC_ES ← WTI:

| | ARMA del input FIJO | ARMA del input LIBRE |
|---|---|---|
| logL | −718.183933 | −718.183933 |
| ω₀ | 0.016402 | 0.016402 |
| ω₁ | 0.010792 | 0.010792 |

Idénticos. No es casualidad: con Σ **diagonal** y sin realimentación, la verosimilitud
conjunta **se factoriza** en L(N)·L(X), y la de X no depende de la transferencia. La
decisión de Box y Jenkins de no tocar el modelo del input **no es una aproximación: es
exactamente óptima** en ese caso. (Y es, exactamente, por lo que `drtran` homologa con
`fue`.)

Pero la factorización **se rompe en cuanto se libera σ₁₂** — y ese es justamente el
caso que la tesis (§2.4) señala como punto de partida para especificar una relación
bivariante. Ahí la estimación conjunta deja de ser un lujo. *(Batería §3c.)*

### 3.2 La descomposición σ²Q: la normalización que faltaba

`elf` supone `a_t ~ N(0, σ²Q)` y **concentra** σ². Mauricio dice, en las dos referencias
(1995, ec. 2.1; JTSA 2002, p. 474), que esa descomposición **no es única**, y que eso
*«raises no problem in the estimation of E[a_t a′_t], since interest lies in the product
σ²Q, but not in σ² nor in Q by itself»*.

**Tiene razón, y lo verificamos.** Liberando la escala de Q (como hacen el legacy y
drvarma) frente a normalizarla:

| | escala libre | Q[1,1] = 1 |
|---|---|---|
| logL | −767.424341 | −767.424341 |
| Σ̂ | 0.062666 / 68.838114 | 0.062666 / 68.838114 |
| SE(φ₁) | 0.062204 | 0.062204 |
| SE(μ) | 0.028502 | 0.028502 |
| **SE(log var₂/var₁)** | **0.193083** | **0.136399** |

La dirección plana es **exacta** (el criterio es invariante ante Q → cQ), pero es inocua
para logL, para Σ̂ y para los errores estándar de φ, θ, μ, ω, δ. El daño está confinado
a **la inferencia sobre la propia Q**: el parámetro de escala, que es *literalmente* no
identificable, recibe un `t = −0.19` con `p = 0.85` de aspecto respetable; y la razón de
varianzas, que sí lo está, sale con un error estándar **inflado un 42 %**.

`drtran` normaliza `Q[1,1] = 1` por una razón concreta: **decidir si una covarianza es
cero es un contraste**, y un contraste tiene que ser de verdad. Lo demás no lo
necesitaba. *(Detalle en BRIDGE_DESIGN §10; batería §2g.)*

### 3.3 El hessiano: exacto, no acumulado

`fue`, el legacy y `drvarma` calculan los errores estándar del **hessiano acumulado por
BFGS**. En los tres, la alternativa exacta (`fdhess` + `choldcp`) está **escrita y
comentada** — p. ej. `drv-source/m6-1/drvmlest.c:97-98`. Fue una decisión de coste de
1996 que nadie revisó.

Un hessiano de BFGS se construye con los pasos que el optimizador **da**; por eso nunca
ve las direcciones en las que no se mueve, y produce números finitos donde no debería
haberlos. Dos consecuencias medidas:

- Los errores estándar de `fue` son **inestables**: existen dos salidas del mismo modelo,
  con estimaciones puntuales idénticas, y `SE(μ)` = 0.073304 en una y 0.028316 en otra.
  (Documentado en `fue-1.13.1/ERRORES_ESTANDAR.md`.)
- Los `sd` de las nueve sigmas que publica `m6-1` son de esa especie.

`drtran` usa `fdhess`. Contrastado contra el valor teórico exacto: `SE(μ) = 0.028502`,
que es el de MCG exacto. *(Batería §1d.)*

### 3.4 «Un sólo output» frente a la red

El título de la sección 2.5 de la tesis es, literalmente, *«Representación del Modelo de
Transferencia de un **Sólo Output** (UT)»*. Y ese es el límite del aparato clásico: una
salida, k entradas exógenas.

Los sistemas reales de la escuela **no son eso**. En `m6` (mercado laboral español), EU
es **salida** de EC **y entrada** de EI; y EI es salida de EU y entrada de EP:

```
EC ── b=2 ──► EU ── b=1 ──► EI ── b=1 ──► EP
 └───────────────── b=1 ─────────────────►┘
```

Se resolvía escribiendo el `shootx` a mano, modelo por modelo. `drtran` lo generaliza a
un **DAG de transferencias** declarado en un fichero (`-n`), donde cada enlace resta su
transferencia a *su* salida; el VARMA sigue siendo diagonal, y la previsión recorre la
red en orden topológico propagando los pesos ψ del sistema. Un **ciclo** se rechaza: el
sistema sería simultáneo y no se puede triangularizar restando.

Medido sobre una cadena sintética X → M → Y: la red recupera las dos transferencias y,
**con los mismos 7 parámetros libres**, bate a la estrella por 213 puntos de logL
(−1038.6 vs −1251.8). En la estrella, el enlace directo X→Y que se ve obligada a
postular sale insignificante (t = 1.45) — porque el efecto de X sobre Y es *indirecto*, y
solo la red puede decirlo. *(Batería §2f.)*

### 3.5 Restricciones: transformar los datos, o declararlas

La tesis impone la Hipótesis de Neutralidad Monetaria (`g = v(1) = 1`, ganancia a largo
plazo unitaria) **transformando los datos**: reescribe el modelo como

```
Y_t − lnM1_{t−b}  =  [ω*(B)/δ(B)]·B^b · ∇lnM1_t  +  N_t
```

con `ω*(B) = ω(B) − δ(B)`. Es una solución elegante y de su época: la restricción es no
lineal en los parámetros, así que se saca del optimizador **a mano**, reparametrizando.
Tiene además la virtud de que el output resultante gana interpretación económica (saldos
reales).

`drtran` declara las restricciones en una **tabla de slots**: cada parámetro estructural
es LIBRE, FIJO o **ALIAS** (compartido), y se dice en un fichero (`-c`) con los mismos
nombres que el programa imprime:

```
delta1[1] = phi_2[B^1]     # el denominador de la transferencia ES el AR del input
omega2[1] = 0.0            # fijar
q[2,1]    = free           # liberar una covarianza
```

Esto cubre las restricciones **lineales** (compartir, fijar) — que son las que la tesis
usa para las tríadas ligadas por identidad (§6.1) — pero **no cubre la ganancia**, que
es no lineal. Carencia identificada, ver §6.

### 3.6 El arranque de la muestra: TASTE lo hacía mejor

La transferencia `Σₖ νₖ·X_{t−k}` necesita el pasado del input. En t = 1 no lo hay.

- **TASTE**: lo **retropredice** (`BACKTF.PAS`, `MBACK = −200`). Es la solución de Box y
  Jenkins.
- **drtran**: lo **trunca** — supone input pre‑muestral = 0.

Medido: con r = 1 y δ = 0.6, el error en t = 1 es el **63 % de sd(w_X)**, y decae como
δᵗ contaminando las primeras ~9 observaciones. Con 215–400 datos es irrelevante. Con las
**69 observaciones de m6** sería el 15 % de la muestra. Aquí la solución de 1979 es
mejor que la nuestra, y está apuntada.

---

## 4. Un resultado: ω₀ y σ₁₂ son el mismo modelo cuando φ_N = φ_X

Este es el hallazgo de la nota, y salió de un experimento que iba de otra cosa.

Al estimar IPC_ES ← WTI con transferencia **contemporánea** (b = 0) y **liberar** a la
vez la covarianza de las innovaciones, el ajuste se va a una esquina absurda:

| | b=0, Σ diagonal | b=0, σ₁₂ libre |
|---|---|---|
| logL | −718.184 | −718.170 |
| ω₀ | 0.0164 | **0.1520** |
| corr(a_N, a_X) | 0 | **−0.985** |
| t de σ₁₂ | — | **−2424** |

La verosimilitud **no mejora** (LR = 0.029, contra χ²(1) = 3.84) y sin embargo los
parámetros vuelan. Eso no es un fallo del optimizador: es una **cresta casi plana**, y
tiene una explicación exacta.

**Mírese la CCF preblanqueada**, que es la que se usa para identificar. Se filtra todo
por el modelo del input: `α_t = φ_X(B)X_t = a_X,t`, `β_t = φ_X(B)Y_t`.

- **(A) Transferencia contemporánea** `Y = ω₀X + N`, con `N ⊥ X`:
  ```
  β_t = ω₀·a_X,t + φ_X(B)N_t
  Cov(β_t, α_{t−k}) = ω₀·σ²_X   si k = 0,   y  0  para todo k > 0.
  ```
  Una **espiga limpia en k = 0**.

- **(B) Covarianza** `Y = N`, con `cov(a_N,t, a_X,t) = σ₁₂`:
  ```
  β_t = φ_X(B)·ψ_N(B)·a_N,t
  Cov(β_t, α_{t−k}) = σ₁₂ · [coef. de B^k en φ_X(B)ψ_N(B)]
                    = σ₁₂ · φ_N^{k−1}·(φ_N − φ_X)      para k ≥ 1
  ```
  La **misma espiga en k = 0**, más una cola proporcional a **(φ_N − φ_X)**.

> **Si φ_N = φ_X, la cola se anula y los dos modelos son EXACTAMENTE indistinguibles.**
> No es una colinealidad numérica: es un resultado de no identificación.

Y cuanto más se parezcan los dos AR, peor. Cola relativa a la espiga:

| caso | φ_N | φ_X | k=1 | k=2 | k=3 |
|---|---|---|---|---|---|
| IPC ← WTI (real) | 0.403 | 0.299 | **+0.104** | +0.042 | +0.017 |
| SYN (sintético) | 0.300 | 0.500 | −0.200 | −0.060 | −0.018 |
| φ_N = φ_X | — | — | **0.000** | 0.000 | 0.000 |

En IPC ← WTI, **toda** la información que separa los dos mecanismos es una señal del
**10.4 %** de la principal. Con 215 observaciones, eso no se distingue. De ahí la cresta.

**Consecuencias prácticas**, las tres verificadas (batería §3d):

1. Con **b ≥ 1** el problema desaparece: la transferencia no aporta nada en k = 0, y σ₁₂
   queda limpiamente identificada. Sobre SYN (b=2, verdad σ₁₂ = 0): `t = 1.33, p = 0.18`,
   correctamente no significativa, y los omegas intactos.
2. `drtran` **avisa** cuando un enlace con b = 0 tiene su covarianza libre.
3. **Y explica la doctrina de la escuela.** `m6-1` tiene covarianzas fuera de la diagonal
   y **ninguna** estructura contemporánea. Y la tesis (§2.4) dice que la especificación de
   una relación bivariante *«puede comenzar con la modificación de la matriz Σ»* — es
   decir: se usa **una** de las dos, σ₁₂ **o** estructura contemporánea. Nunca las dos.
   Nosotros lo hemos redescubierto chocando contra ello.

---

## 5. Un caso: la transferencia del crudo al IPC no es racional

Tres ajustes de IPC_ES ← WTI, mismo output y mismo input; **los dos primeros con
exactamente los mismos parámetros libres**:

| modelo | libres | logL | adecuación |
|---|---|---|---|
| ω₀ / (1 − δ₁B) — racional | 17 | −721.72 | **INADECUADO** (p = 0.033) |
| ω₀ + ω₁B — dos omegas | 17 | **−718.18** | adecuado (p = 0.196) |
| ω₀ + ω₁B / (1 − δ₁B) — anida al anterior | 18 | −718.18 | adecuado |

El racional **pierde con el mismo número de parámetros**. Y el tercero, que anida al
segundo, lo remata: `δ₁ = −0.010` con **t = −0.055**, LR = 0.003 contra χ²(1) = 3.84.
El denominador no aporta nada.

La razón está en los pesos:

```
racional  : 0.01680  0.00769  0.00352  0.00161  0.00074    (cola geométrica)
dos omegas: 0.01640  0.01079  0.00000  0.00000  0.00000    (se corta)
```

Los datos quieren `ν₁/ν₀ ≈ 0.66` **y `ν₂ = 0`**. Una decadencia geométrica no puede hacer
las dos cosas. La prueba de adecuación lo caza exactamente ahí: deja rastro del input en
el ruido en **k = 2, con r = −0.14** — negativo, porque el racional pone *de más* en ese
retardo.

**El aviso metodológico.** En el ajuste racional forzado, `δ₁ = 0.458` sale con
**t = 6.40**: aparentemente contundente. Es un espejismo. Con el denominador impuesto, δ
es *el único* camino para dar peso al retardo 1; su significación dice «hay respuesta en
el retardo 1», no «la respuesta es geométrica». El contraste honesto es el del modelo que
los anida, y ahí muere. *(Batería §3b.)*

Lectura económica: el traslado del crudo al IPC español es **inmediato y se agota en dos
meses**, no un retardo distribuido que se propaga. Y `ω = (0.0164, 0.0108)` reproduce lo
que `drvarma` tenía documentado (β = 0.0154, φ₂₁ = 0.0104).

---

## 6. Lo que la tesis enseña y drtran todavía no hace

Tres cosas, por orden de valor:

1. **El orden de reformulación.** La tesis (§2.6) establece una asimetría que no es
   obvia:

   > «La especificación inadecuada de la relación v(B) puede generar la apariencia (en
   > acf/pacf residuales) de especificación inadecuada del ruido […]. Sin embargo, la
   > especificación inadecuada del ruido **no puede** dar la impresión en ccf de
   > especificación inadecuada de la relación. Por estas razones, **se reformula v(B)
   > hasta que parezca adecuada antes de reformular** θ(B).»

   `drtran` imprime hoy los dos diagnósticos —univariante y CCF— sin decir cuál mirar
   primero. Debería decirlo: **primero la relación, después el ruido.** Es una línea de
   texto y evita un ciclo de trabajo mal orientado.

2. **La ganancia a largo plazo `g = v(1)`.** No se informa. Es la cantidad con
   significado económico (el efecto total y permanente del input sobre el output) y es la
   que la tesis quiere contrastar. Se calcula trivialmente —`Σω / (1 − Σδ)`— y su error
   estándar sale por el método delta con la matriz de covarianzas que ya tenemos. Con eso,
   la Hipótesis de Neutralidad Monetaria se **contrasta directamente**, sin necesidad de
   transformar los datos ni de estimar dos modelos.

3. **Las tríadas ligadas por identidad.** La tesis las trata (§2.4) imponiendo que los
   parámetros deterministas de las tres series verifiquen la identidad, *«con lo que se
   economiza el número de parámetros a estimar»*. `drtran` **ya puede expresarlo** —la
   tabla de slots comparte parámetros entre series— pero no lo hemos ejercitado, y las
   combinaciones lineales de previsiones con su varianza `c′Vc` (los agregados: OCUPADOS
   = Σ sectores; PARADOS = ACTIVOS − OCUPADOS) siguen sin implementarse.

---

## 7. Qué NO es nuevo

Casi todo, y conviene decirlo:

- Los modelos son de **Box y Jenkins (1970)**.
- El preblanqueo y la CCF para identificar (b, r, s) son suyos, cap. 11. `drtran` los
  implementa **igual** que la tesis los describe: filtrar el output por el modelo del
  input y cruzarlo con los residuos del input.
- La verosimilitud exacta multivariante es de **Mauricio (1995, 1997)**, y se usa **sin
  tocar una línea**. Cualquier discrepancia con `fue` se ha tratado, siempre, como un bug
  de `drtran`. Siempre lo era.
- La metodología —órdenes de diferenciación, MEG, intervención, tríadas, contraste de no
  invertibilidad, sobreajuste— es de la escuela de **Treadway**, y está en la tesis.
- La estimación conjunta con Σ diagonal es, como se ve en §3.1, **exactamente** lo que
  Box y Jenkins ya hacían por partes.

Lo que sí es de esta reconstrucción: la **normalización de Q** que hace contrastables las
covarianzas (§3.2), el **hessiano exacto** (§3.3), la **red** en lugar de un solo output
(§3.4), y el **resultado de no identificación** de §4 — que, sospecho, la escuela conocía
en la práctica (por eso `m6-1` está especificado como está) sin haberlo escrito.

---

## Referencias

- Box, G. E. P., Jenkins, G. M. y Reinsel, G. C. (1994). *Time Series Analysis:
  Forecasting and Control*, 3ª ed. Caps. 10–11.
- Mauricio, J. A. (1995). «Exact maximum likelihood estimation of stationary vector ARMA
  models». *JASA* 90, 282–291. — *el algoritmo que implementa `elfvarma`.*
- Mauricio, J. A. (2002). «An algorithm for the exact likelihood of a stationary vector
  autoregressive-moving average model». *JTSA* 23(4), 473–486. — *forma de innovaciones;
  algoritmo distinto, no cableado.*
- Mélard, G. (1984). «Algorithm AS 197: A fast algorithm for the exact likelihood of ARMA
  models». *Applied Statistics* 33, 104–114. — *el univariante, detrás de `fue`.*
- Muñoz Polo, M. S. (2001). *Estudios econométricos de las series temporales de la
  industria española*. Tesis doctoral, UCM. Director: A. B. Treadway. — *la metodología
  de la escuela, y el uso del motor de Mauricio.*
- Treadway, A. B. TASTE (Pascal). — *identificación y estimación; retropredicción del
  input.*

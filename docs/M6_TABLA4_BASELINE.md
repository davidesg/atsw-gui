# M6 — la diagonal del multivariante: los seis univariantes de la Tabla 4

**Propósito.** Construir los **seis `.pre` univariantes** (P, EA, EP, EI, EU, EC) que
forman la **diagonal de arranque** del multivariante del empleo (Relloso 1997), *grabando
la especificación publicada en la Tabla 4* como valores de partida. Son el paso 1→2 de la
metodología: `drtran ... -0` los re-estima conjuntamente con covarianzas libres (ver
`M6_EJERCICIO.md` §4 y `CASO_EMPLEO.md` §5).

**Qué NO es esto.** No es la identificación guiada paso a paso (eso es `M6_EA_GUIADO.md`,
un ejemplo pedagógico de *cómo se llegaría* al modelo). Aquí **no re-identificamos**: la
Tabla 4 es la fuente, y solo hay que codificarla bien. La decisión (jul-2026): *grabar
Tabla 4* vía `build_m6.py`, con **las intervenciones de la Tabla 4** (no las del
`m1.inp` legacy, que son las del modelo multivariante *final*, paso 4, y traen más
incidentes).

---

## 1. La Tabla 4, para las seis series de la diagonal

Relloso (1997), Tabla 4 (pp. 23-24 del `9720.pdf`), **representación MEG más estocástica**
(«se parte de los modelos con mayor número de factores estacionales estocásticos»). Los
θ, λ₁, λ₂ son parámetros de la estructura estocástica; σ̂ₐ en miles de personas.

| serie | operador | θ (f0) | λ₁ (π/2) | λ₂ (π, Nyquist) | σ̂ₐ | incidentes (Tabla 4) |
|---|---|---|---|---|---|---|
| **P**  | ∇²  | .82(.09) | — | — | 17.9 | (ninguno) |
| **EA** | ∇∇₄ | .43(.20) | −.68(.10) | −.72(.14) | 27.3 | I/85, II/87, I/89, IV/89, I/92 |
| **EP** | ∇∇₄ | .58(.15) | −.78(.07) | −.87(.12) | 35.0 | II/87 |
| **EI** | ∇∇₄ | .46(.14) | −1.0(.10) | −1.0(.04) | 18.9 | II/80, II/82, II/87, II/88, IV/92 |
| **EU** | ∇²  | .88(.05) | — | — | 16.7 | II/87, III/88, I/92, IV/92 |
| **EC** | ∇∇₄ | .60(.10) | −.83(.14) | −.94(.05) | 19.0 | I/92 |

Detalle de incidentes (fecha, tipo, ω̂₀; SE entre paréntesis). Tipo: **S**=escalón,
**1**=impulso, **IC**=impulso compuesto. Un segundo ω̂₁ y un `g=` marcan una intervención
**compuesta** (numerador de dos parámetros; `g` es la ganancia permanente, derivada):

- **EA** (todos S): I/85 86.5(11.5), II/87 63.3(20.1), I/89 −84.0(22.8), IV/89 −70.9(20.5), I/92 42.4(22.3).
- **EP**: II/87 S 188.2(29.7).
- **EI**: II/80 **1** −49.0(8.8); II/82 **IC** 18.3(5.7); II/87 S 63.0(6.9); II/88 S −52.7(15.0); IV/92 S **compuesta** ω₀=−61.8(15.8), ω₁=82.0(14.9), g=−143.8(18.9).
- **EU**: II/87 S **compuesta** ω₀=−60.8(11.6), ω₁=118.7(11.4), g=−179.5; III/88 **1** −60.3(11.1); I/92 S −44.6(16.2); IV/92 S **compuesta** ω₀=−44.5(15.9), ω₁=39.2(16.1), g=−83.7(21.7).
- **EC**: I/92 S −30.9(17.9).

### 1.1 La representación EFECTIVA de la diagonal = la del legacy m6-1

La Tabla 4 es un **menú** (varias filas por serie). La diagonal que grabamos sigue la
**diagonal del legacy `m6-1`**, verificada leyendo `drv-source/m6-1/drv.c` (bloque `x[]`
con etiquetas). El legacy modela **una sola serie con estacionalidad estocástica, EA**; las
demás sectoriales van **∇² con estacionalidad DETERMINISTA** (cos/sin/alter):

| serie | operador grabado | estacional | por qué |
|---|---|---|---|
| **P**  | ∇² MA(1) θ=.82 | — | sin estacionalidad |
| **EA** | ∇∇₄ θ,λ₁,λ₂ | **estocástica** | única; λ genuinos (−.68,−.72) |
| **EP** | ∇² MA(1) + cos/sin/alter | **determinista** | legacy (`x[36-38]`); Tabla 4 la deja π/2 estocástica pero el paso 4 la simplificó — se sigue al legacy (§6.4) |
| **EI** | ∇² MA(1) + cos/sin/alter | **determinista** | legacy (`x[45-47]`) = fila ∇² de Tabla 4; sus testigos ∇∇₄ pinchan en −1 |
| **EU** | ∇² MA(1) θ=.90 | — | sin estacionalidad |
| **EC** | ∇² MA(1) + cos/sin/alter | **determinista** | legacy (`x[58-60]`) = fila ∇² de Tabla 4 (σ=18.0<19.0); su testigo π/2 pincha en −1 en el conjunto |

**Covarianzas contemporáneas — el legacy libera exactamente TRES** (`drv.c` x[3],x[4],x[7]):
`σ₄₂` (EA·EI), `σ₆₂` (EA·EC), `σ₅₄` (EI·EU). Son **justo** las tres correlaciones fuertes
que emergen del paso 2 diagonal (§7): EA–EI −.31, EA–EC −.41, EI–EU +.35.

> **Nota — las intervenciones de Tabla 4 ≠ las del `m1.inp` legacy.** Los `.pre`
> reconstruidos por `build_m6.py` traían los incidentes del **modelo multivariante final**
> (p.ej. EP con 6 escalones). El multivariante descubre más anomalías; el univariante
> baseline (Tabla 4) tiene menos (EP: solo II/87). Para la diagonal de arranque van los de
> la **Tabla 4**. EA es la excepción feliz: sus 5 incidentes coinciden en ambos.

> **Representaciones alternativas.** La Tabla 4 da para varias series una segunda fila
> (∇² determinista, ∇²(1+B²)…). Tomamos siempre **la más estocástica** (∇∇₄ para
> EA/EP/EI/EC). En EI los testigos salen en la frontera (λ₁=λ₂=−1.0): la estacionalidad es
> *de facto* determinista, y la representación ∇∇₄ con λ=−1 es algebraicamente equivalente
> a la ∇² determinista. Se graba ∇∇₄ (ver §6, punto abierto).

---

## 2. La codificación `.pre` del MEG ∇∇₄ (el formato exacto)

Verificado contra un `.pre` MEG generado por ART (`EA_meg2.pre`) **y** contra el lector de
drtran (`src/fue_pre_reader.c`, líneas 542-591). Para una serie ∇∇₄ trimestral con **ambas
frecuencias estacionales estocásticas**:

```
** Number and orders of regular MA operators:
2 1 1                 ← 2 factores, orden 1 cada uno
**
 0.43  1              ← θ  (MA regular en f0):  factor (1 − θB)
**
-0.72  1              ← λ₂ (testigo Nyquist π): factor (1 − λ₂B) = (1 + .72B)
...
** Number and frequencies of regular MA(2) operators with fixed frequency:
1 1.000000            ← 1 factor a la frecuencia índice 1 (= π/2)
**
-0.68  1              ← λ₁ (testigo π/2): factor (1 − λ₁B²) = (1 + .68B²)
...
** Box-Cox lambda, regular differences and complete annual differences:
1.00 2 0              ← d=2, D=0   (¡NO d=1 D=1!  ver §3)
** Individual factors of the annual difference (from freq 0.0):
 0 1 1                ← [f0, π/2, π]: activa las raíces estacionales π/2 y π
```

**Convención de signo — se graba el valor de Tabla 4 VERBATIM.** Comprobado con EA:
ART re-estimó θ=0.4306, λ₂=−0.7209, λ₁=−0.6839 contra Tabla 4 (.43, −.72, −.68). Los tres
se almacenan **con su signo tal cual la Tabla 4**. El factor que resulta:
- f0: `(1 − θB)` con θ=+.43 → invertible, lejos de 1 (testigo de sobre-diferenciación regular OK).
- π (Nyquist): factor lineal `(1 − λ₂B)`; el testigo va como **2.º factor de la MA regular** (orden 1), no como MA(2).
- π/2 (par complejo): factor `(1 − λ₁B²)`; va como **MA(2) de frecuencia fija** a la frecuencia índice 1. A π/2, cos=0, así que solo hay coeficiente en B².

Para una serie **∇²** (P, EU): sin factores estacionales → MA regular de **1 factor** (θ),
sin MA(2) de frecuencia fija, `ifadf = 0 0 0`, `d=2 D=0`.

**Formato de `ifadf` (línea "Individual factors").** drtran reserva `freq/2 + 1` enteros y
los lee todos (`fue_pre_reader.c:544-547`). Para **s=4 son 3**: `[f0, π/2, π]`. `ornsop`
suma +1 por f0, **+2 por π/2**, +1 por π (líneas 583-587): ∇∇₄ = 2(d) + 2(π/2) + 1(π) = 5
raíces = grado de (1−B)²(1+B²)(1+B). ✓ (Los 7 ceros que escribía `build_m6.py` eran
sobre-relleno inofensivo *porque eran cero*; con ifadf activo hay que usar 3.)

---

## 3. ∇∇₄ es **d=2 + ifadf[π/2,π]**, no d=1 — el error que costó una vuelta

`∇∇₄ = (1−B)(1−B⁴) = (1−B)²(1+B)(1+B²)`. En la descomposición por frecuencias del MEG:

| frecuencia | factor | dónde va en el `.pre` |
|---|---|---|
| f0 (dos raíces) | (1−B)² | **d = 2** |
| π/2 | (1+B²) | ifadf[1]=1 |
| π (Nyquist) | (1+B) | ifadf[2]=1 |

El `(1−B)` interior de ∇₄ **se suma a la diferencia regular**: por eso son **d=2**, no d=1.
La notación «d=1 + D=1» de la identificación guiada es correcta como *narrativa* (∇ para la
tendencia, ∇₄ para lo estacional), pero al **codificar el MEG por factores** el f0 se cuenta
una sola vez y agrega: total d=2 a f0.

> **El síntoma del error (por si reaparece).** Reformular desde una base **d=1** (en vez de
> d=2) produce ∇₄ (una diferencia regular de menos): el ajuste se degrada (en EA λ₁ se fue a
> .94 vs .68, el MA regular y el Nyquist salieron colineales, y Q fallaba en el retardo 2).
> Con la base **d=2** correcta, EA **reproduce la Tabla 4** y la diagnosis APRUEBA. Si un
> MEG «casi ajusta pero no cuadra», sospecha el orden de f0.

---

## 4. Flujo para construir y **verificar** cada `.pre`

1. `build_m6.py` escribe el `.pre` con la estructura + valores de Tabla 4 (§1, §2).
2. **Verificación** (imprescindible — la Tabla 4 es *control*): estimar el `.pre` en
   solitario (ART/fue) y comprobar que σ̂ₐ y los θ/λ/incidentes reproducen la Tabla 4.
   La diagnosis debe APROBAR.
3. Solo entonces la serie entra en la diagonal del multivariante.

La verificación captura errores de formato (el gotcha de las **dos** líneas Nomega/Ndelta,
`build_m6.py:134`), de signo, o de orden de diferenciación (§3).

---

## 5. Estado por serie

| serie | `.pre` grabado | verificado vs Tabla 4 | notas |
|---|:--:|:--:|---|
| **P**  | **✓ build_m6** | **✓** | θ=.792 (T4 .82), σ=**17.9** (=T4), APROBADO |
| **EA** | **✓ build_m6** | **✓ (vía ART)** | θ=.431, λ₁=−.684, λ₂=−.721, σ=27.0, Q(15)=10.9, APROBADO |
| **EP** | **✓ build_m6** | **✓ (legacy)** | **∇² determinista** (§6.4, como legacy): θ=.72, σ=35.9, sen π/2=33.9, (−1)ᵗ=−3.6, II/87=195, APROBADO |
| **EC** | **✓ build_m6** | **✓ (∇² legacy)** | **∇² determinista** (§6.2): θ=.62, σ=18.0, cos/sen/alter=−2.4/19.7/2.3, I/92=−20.9, APROBADO |
| **EI** | **✓ build_m6** | **✓** | **∇² determinista** (§6.2): θ=.44, σ=18.8, cos/sen/alter=4.6/1.7/3.2, APROBADO |
| **EU** | **✓ build_m6** | **✓** | ∇² sin estacional; θ=.91 (T4 .88), σ=17.4; REVISAR solo por 1 outlier 1986:III (JB), Q pasa |

**Verificación (jul-2026)** — `build_m6.py` → `estimate_and_diagnose` sobre cada `.pre`:
los θ/λ/incidentes simples/σ̂ₐ reproducen la Tabla 4 (diferencias < redondeo; σ̂ₐ exacta o
a décimas). Confirma la codificación MEG (§2), la ∇² determinista de EI (§6.2) y las
intervenciones de Tabla 4. **Las seis series verificadas.** Los `.pre` se generan en scratch
para verificar; se vuelcan a `tests/data/m6/` cuando se cierre la tanda.

> **Caveat — intervenciones compuestas débilmente identificadas.** Las compuestas de fin de
> muestra (EU II/87 y IV/92, EI IV/92) se **graban** con los ω₀,ω₁ de Tabla 4 DIRECTOS, y la
> ganancia permanente `g = ω₀−ω₁` (convención BJR, la de Relloso) cuadra, pero la
> re-estimación en solitario las lleva a otro modo (p.ej. EU II/87 −60.8/+118.7 → −113.3/
> +28.3). Con solo 2-3 obs tras la fecha, el numerador de dos parámetros es casi no
> identificable univariantemente. No es un error de encoding: es el arranque correcto, y el
> multivariante (donde Relloso las estimó con información cruzada) es donde se anclan.

**EA — verificación (jul-2026).** Base determinista correcta `EA_d2_i5` (d=2, armónicos
cos/sin/alter + 5 escalones) → `meg_reformulate(freq=1)` → `meg_reformulate(freq=2)`:

| | Tabla 4 | ART (÷ refactor 100) |
|---|---|---|
| θ (f0) | .43 | 0.431 |
| λ₁ (π/2) | −.68 | −0.684 |
| λ₂ (π) | −.72 | −0.721 |
| σ̂ₐ | 27.3 | 27.0 |
| II/87, I/85 | 63.3, 86.5 | 63.3, 86.5 |
| I/89, IV/89, I/92 | −84.0, −70.9, 42.4 | −84.0, −71.0, 42.4 |

Artefactos en `examples/m6_EA/work/` (scratchpad de trabajo, no versionado como caso).

---

## 6. Puntos abiertos — resueltos

1. **Intervenciones compuestas** (EU II/87 y IV/92; EI IV/92) — **RESUELTO.** Numerador de
   dos parámetros. `g = ω₀ − ω₁` (Relloso escribe `(ω₀ − ω₁B)`); fue aplica **esa misma
   convención BJR** (`calcnu`, fue.c:4505: `ν[j] = Σδ·ν[j−i] − ω[j]`), así que se graban
   **los ω₀,ω₁ de Relloso DIRECTOS** (sin negar) y la ganancia `ω₀ − ω₁ = g` cuadra.
   `det_block` escribe Nomega=1 (dos ω por variable).
   *Caveat*: débilmente identificadas en solitario (§5). Correcto como arranque.
2. **EI y EC — ∇² determinista** (como el legacy) — **RESUELTO.** Sus testigos ∇∇₄ pinchan
   en −1.0 (EI ya en Tabla 4; EC en el diagonal conjunto): estacionalidad *de facto*
   determinista; grabar −1.0 sería un MA no invertible que cancela la raíz AR (degenerado).
   La Tabla 4 trae una **fila ∇²** para ambas (EI θ=.43; EC θ=.62, σ=18.0<19.0), que es la
   que usa el legacy m6-1 (`drv.c` x[45-47], x[58-60]). Verificado (§5).
4. **EP — ∇² determinista** (como el legacy) — **RESUELTO A FAVOR DEL LEGACY.** Aquí el
   univariante y el modelo final **discrepan**: la Tabla 4 deja π/2 **estocástica** en sus
   dos filas (∇∇₄ λ₁=−.78; ∇²(1+B²) λ₁=−.80), pero el legacy m6-1 (`drv.c` x[36-38]) la
   modela **∇² determinista** en todas las frecuencias (cos/sin/alter) — el paso 4 la
   simplificó. Decisión del usuario: «como legacy». Se graba ∇² determinista con los
   deterministas del legacy (Tabla 4 no da esta fila para EP), MA(1) e II/87 de Tabla 4.
   Verificado (§5): θ=.72, σ=35.9, sen π/2=33.9, APROBADO. *(Criterio: cuando univariante y
   modelo final discrepan en estocástico/determinista de una frecuencia, manda el legacy —
   es el modelo que reproducimos.)*
5. **build_m6.py** — **HECHO.** `write_pre`/`ma_block`/`det_block` graban: MA regular
   multi-factor, MA(2) de frecuencia fija, ifadf de 3 valores (s=4), e intervenciones con
   Nomega≥0 y valores de Tabla 4. Series controladas por el conjunto `READY`.

## 7. Paso 2 (diagonal) — el BUG de Nyquist, encontrado y CORREGIDO

Los seis `.pre` volcados a `tests/data/m6/`; `m6.cns` libera las 15 covarianzas
contemporáneas (Relloso §4: «sin restringir»). El primer intento del diagonal reveló que
las series **MEG ∇∇₄ (EA, EP, EC) salían mal estimadas** por drtran (σ_EA≈89 vs 27),
aunque su `.pre` era correcto (fue las reproduce). Y **hasta con la MA fija en Tabla 4**,
drtran daba σ_EA=224 → el fallo estaba en la **verosimilitud**, no en el optimizador.

**El bug (un carácter).** En `CalcNonsOp` (`fue_pre_reader.c`) — que construye el operador
de diferenciación `∇∇₄ = (1−B)²(1+B)(1+B²)` a partir de `ifadf` — la rama de la frecuencia
**Nyquist** (π) tenía `pol4[1] = +1.0`, que es el factor de la frecuencia **0** `(1−B)`. El
factor de Nyquist es `(1+B)` → `pol4[1] = −1.0`. Al portar de `fue.c` (legacy, que trae
`−1.0`, ver `atws/fue/fue-1.13/src/fue.c:4449`) se copiaron los valores de la rama de f=0.

**Efecto.** Con el signo mal, EA se diferenciaba como `(1−B)³(1+B²)` en vez de
`(1−B)²(1+B²)(1+B)`: sobre-diferenciaba en f=0 y **no** diferenciaba en Nyquist → dejaba la
estacionalidad de π en el ruido (σ ~8×) y el testigo MA se iba a la raíz unitaria. Solo
afectaba a series con `ifadf[Nyquist]=1` (EA/EP/EC); las ∇² (P/EI/EU) no lo tocaban — de
ahí el patrón. El fix: `pol4[1] = −1.0` en la rama Nyquist.

**Verificación (tras el fix).** `drtran -0` (diagonal puro, sin covarianzas) **CONVERGE** y
reproduce fue/Tabla 4 EXACTAMENTE en las seis:

| serie | θ (f0) | λ₂ (π) | λ₁ (π/2) | σ̂ₐ (drtran / T4) |
|---|---|---|---|---|
| EA | .4306 | −.7209 | −.6839 | 27.3 / 27.3 |
| EP | .575 | −.866 | −.776 | 35.0 / 35.0 |
| EC | .590 | −.936 | −.827 | 19.0 / 19.0 |
| EI | .435 | — | — | 18.0 / 18.5 |
| P / EU | .820 / .907 | — | — | 17.9 / 16.7 |

ℓ pasó de −1876 a **−1730** (diagonal) / −1715 (con covarianzas). Batería de tests: 249
PASS, 0 FAIL. Regresión añadida (`test_battery.sh`, homologación ∇∇₄ trimestral). Es el
6.º bug que el ejercicio m6 destapa (cf. `M6_EJERCICIO.md` §7).

*(Bug de display pendiente, menor: la diagnosis de residuos de `M6_diag.out` rotula
«seasonal period: 12» siendo trimestral.)*

*(La tabla de arriba es la homologación del momento del fix, con EP/EC aún ∇∇₄. Después se
pasaron a ∇² determinista siguiendo el legacy, ver §6.2/§6.4 — EA sigue siendo la única
estocástica.)*

## 8. Paso 2 (diagonal) — la lectura

Con la diagonal final (EA ∇∇₄; P/EU/EP/EI/EC ∇²) y las 15 covarianzas libres, el modelo
**CONVERGE** (334 iter, ℓ=−1709.5, 64 obs, 54 par) y **ningún testigo se clava** (EA π/2 en
−.59). `M6_diag.out`.

**Correlaciones contemporáneas (Σ)** — la estructura dominante, y coincide con las 3
covarianzas que el legacy libera:

|  | EA | EI | EU | EC |
|---|---|---|---|---|
| **EA** | 1 | **−.31** | −.00 | **−.41** |
| **EI** | −.31 | 1 | **+.35** | +.17 |

→ **EA·EC −.41, EI·EU +.35, EA·EI −.31** = `σ₆₂, σ₅₄, σ₄₂` del legacy. La estructura del
sistema es **sobre todo contemporánea** (lo que Relloso predijo: Σ primero, dinámica luego).

**CCF residuales — relaciones dinámicas** (banda ±.25; k>0 ⇒ i→j). Enlaces candidatos a
transferencia (moderados; ningún Q-cruce supera con holgura el crítico):

| enlace | k | r | lectura |
|---|---|---|---|
| **EI → EP** | +1 | +.30 | industria lidera servicios privados 1 trim |
| **EP → EC** | −2 | +.27 | servicios privados → construcción 2 trim |
| **P → EC** | −5 | +.38 | población → construcción (demográfico) |
| **EA → EP** | −5 | +.34 | agricultura → servicios privados (trasvase) |

Coherente con la red del legacy (EC→EU→EI→EP): EU–EI fuerte contemporáneo, EI→EP a lag 1.
Los `sector→P` (EU→P k=4) se descartan por la exogeneidad de P.

**Lo que queda:** pasos 3-4 (§9).

## 9. Pasos 3-4 — la red de transferencias, validada contra el legacy

**La red del legacy m6-1** (decodificada de `drv-source/m6-1/drv.c`, función `shootx`,
tensor `theta1[lag][salida][entrada]` — NO de `multshea.c`, que no está cableado). Es un
**VMA(4)** (p=0). Diagonal: P=(1−.82B), EA=∇∇₄ factorizado, **EP = identidad (sin MA
propia)**, EI=(1−.49B), EU=(1−.90B), EC=(1−.43B). DAG **EC→EU→EI→EP** + **EC→EP**, todas
finitas (r=0):

| transfer | b,s | numerador factorizado (legacy) | comparte |
|---|---|---|---|
| EP←EI | 1,1 | −x5(1−x6B) | **x6 = MA de EI** |
| EP←EC | 1,2 | (1−x14B)(x12+x13B) | — |
| EI←EU | 1,3 | −(1−B)(x8+x9B+x10B²) | — |
| EU←EC | 2,1 | −x15(1−x16B) | **x16 = MA de EU** |

Covarianzas: el legacy libera **solo 3** (`qq[4][2]`=EA·EI, `qq[6][2]`=EA·EC,
`qq[5][4]`=EI·EU).

**Estimación en drtran** (numeradores LIBRES; drtran no expresa los productos ni los
compartidos), con la red y las covarianzas versionadas en `tests/data/m6/m6_net.dag` y
`m6_net.cns` (orden EP EI EU EC EA P):
`drtran M6_EP M6_EI M6_EU M6_EC M6_EA M6_P -n m6_net.dag -c m6_net.cns`. Convergió
(55 par, **ℓ=−1697.6** vs −1709.5 del diagonal → los transfers ganan ~12). Ambas columnas
están en **convención Box-Jenkins** ω(B)=ω₀−ω₁B−…, la misma de fue (tras el audit de signo;
ver TODO §"signo del cast"). El − del cast empotrado es interno y ya **no** se filtra al ω
reportado; los ω₀ son idénticos y los ω_k (k≥1) llevan el signo BJR.

| transfer | drtran (red) | legacy (BJR) | veredicto |
|---|---|---|---|
| **EP←EI** | **(0.750, 0.300)** | **(0.78, 0.382)** | **✓ clavado** |
| EP←EC | (0.361, −0.529, 0.159) | (0.56, −0.74, 1.28) | dominantes ✓, ω₂ amortiguado |
| EI←EU | (0.154, −0.453, −0.292, 0.006) | (0.18, −0.24, 0.14, 0.28) | líder ✓, resto flojo |
| EU←EC | (0.170, 0.029) | (0.34, 0.31) | el más débil (mitad) |

Covarianzas recuperadas: EU·EI **+0.40**, EA·EC **−0.36**, EA·EI **−0.22** (legacy
~+.40/−.41/−.31). Transfer adecuado (p=0.95), exogeneidad ok.

**La carencia, ahora MEDIDA.** EP←EI (limpio, sin productos) **clava** el legacy —y su ω₁ se
corrige al añadir contexto: bivariante −0.10 → +EC +0.20 → red completa +0.30 ≈ +0.38 (BJR). Los
enlaces con **numerador factorizado + parámetro compartido** (EP←EC, EI←EU, EU←EC) aciertan
los términos dominantes pero **no clavan**: eso es exactamente lo que se pierde por estimar
con numeradores libres en vez de los **productos** del legacy (`M6_EJERCICIO.md` §6). El
ejercicio no solo confirma la carencia — la **cuantifica**.

## 10. Sobre las iteraciones — la regla de medida del legacy

El diagonal converge en ~334 iteraciones y el usuario preguntó por qué, si los `.pre`
arrancan en el óptimo univariante. Investigado:
- **drvarma** inicializa Q desde la **covarianza muestral de residuos** (`init_varma`,
  Hannan-Rissanen); drtran arranca las covarianzas en **0**. Porté ese init: acercó el
  arranque (F en el óptimo 0.24→0.29) pero **apenas cambió las iteraciones** (334→323) y
  alteró el aterrizaje en crestas planas (rompió un test) → **revertido**.
- **La prueba definitiva (regla de medida):** el **legacy m6-1 convergió en 258 iteraciones
  arrancando *en* la solución** (valores hardcodeados), con la función objetivo **plana**
  (F: 1.000000 → 0.999659, 0.03%). Arrancar en el óptimo **no** le ahorró iteraciones.

**Conclusión:** las ~250-330 iteraciones son **propiedad del optimizador** (`qnewtopt`, el
mismo en legacy y drtran) —parada por cambio de parámetros con direcciones casi planas—, **no
de la distancia de arranque**. drtran (334 desde mal arranque) ≈ legacy (258 desde el óptimo).
Reducirla de verdad exige tocar el motor (parada por valor de función, o Hessiano exacto vs
BFGS acumulado; ver `drtran-note.tex` §sec:hessian) — algo que el legacy tampoco hacía.

---

## Referencias
- Relloso, S. (1997). ICAE 9720 — Tabla 4, pp. 23-24. `drvarma_source/literature/9720.pdf`.
- `M6_EJERCICIO.md` (§4 metodología, §5 lo reconstruido, §8 lo que falta).
- `docs/CASO_EMPLEO.md` (criterios destilados). `docs/M6_EA_GUIADO.md` (identificación guiada de EA).
- Formato `.pre`: `src/fue_pre_reader.c:520-591`. Reconstrucción: `tests/build_m6.py`.

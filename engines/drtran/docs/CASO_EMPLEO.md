# El modelo del empleo por sectores: por qué transferencias y no un VAR

**Un caso de estudio de la escuela de Treadway.**
Material didáctico. Fuente: Relloso (1997); metodología: Muñoz Polo (2001).

---

## Para qué es este documento

Una universidad hace dos cosas: **crea conocimiento y lo preserva**. Lo primero deja
artículos. Lo segundo es más difícil, porque lo que se pierde no son los *resultados* —
esos están publicados— sino **el criterio**: por qué se eligió este modelo y no aquel,
qué se miró antes de decidir, qué se dio por supuesto y por qué, y qué señal hizo cambiar
de rumbo.

Ese criterio no está en las ecuaciones. Está en las decisiones. Y se transmite con
**ejemplos**, no con teoremas.

Este documento recoge un ejemplo completo de la escuela —el modelo del empleo español por
sectores— **con sus criterios y sus heurísticas explícitos**, para que se pueda enseñar y
para que drtran se pueda distribuir con él. No es un resumen del paper: es lo que un
analista necesitaría saber para *volver a hacerlo*.

---

## 1. El problema

Se quiere entender y prever el **empleo, el paro y la población activa** en España.
Datos: Encuesta de Población Activa (EPA), **trimestral**, 1976:III – 1993:II.
**69 observaciones.** Retén ese número: lo decide casi todo.

Las variables:

| símbolo | qué es |
|---|---|
| `E`  | Ocupados |
| `D`  | Parados |
| `A`  | Población Activa |
| `P`  | Población en edad de trabajar (menos la contada aparte) |
| `I`  | Población Inactiva |

**La tesis del trabajo:** no se puede entender `E` como un agregado. Hay que
**desagregarlo en cinco sectores productivos**, porque cada uno se comporta de forma
distinta y **las relaciones entre ellos son la clave**:

| | |
|---|---|
| `EA` | Agricultura, ganadería y pesca |
| `EI` | Industria |
| `EC` | Construcción |
| `EP` | Servicios privados |
| `EU` | Servicios públicos |

Que la desagregación importa se ve en los datos: entre 1976 y 1993, `EA` pierde 1.6
millones de empleos y `EI` casi 900.000, mientras `EP` gana 1.1 millones. **El agregado
apenas se mueve (−567.000) y esconde una transformación estructural completa.**

## 2. Las identidades contables

```
(1)   E = EA + EI + EC + EP + EU        ocupados = suma de los sectores
(2)   A = E + D                         activos  = ocupados + parados
(3)   P = A + I                         población = activos + inactivos
```

Son **exactas**. No son hipótesis: son definiciones. Y esto ya empieza a decidir el
método (§4.5).

## 3. El Modelo Conceptual Inicial: lo que se supone y lo que se contrasta

Aquí está, a mi juicio, **la lección metodológica central del trabajo**, y es una lección
de honestidad.

Relloso escribe primero un **modelo conceptual** —qué puede afectar a qué— y **separa
explícitamente dos clases de afirmaciones**:

### 3.1 Supuestos de identificación (NO contrastables)

> «(1) `P` no recibe influencias contemporáneas de `E`, de sus componentes ni de `A`, y
> (2) `E` y sus componentes no reciben influencias contemporáneas de `A`. **Estos dos
> supuestos facilitan la identificación exacta del sistema** en (`A`, `E`, `P`).»

Es decir: **`P` es exógena**, y el empleo no responde *en el mismo trimestre* a la
población activa. **No se pueden contrastar.** Sostienen el sistema, y se dicen.

### 3.2 Hipótesis contrastables

Otras seis, que sí se contrastan. Y **una se rechaza**:

> «(1) `A` no recibe influencias contemporáneas de `P`, de `E` ni de sus componentes,
> **hipótesis que se rechazan empíricamente**.»

**Ese rechazo es el hallazgo del trabajo.** Es lo que obliga a introducir una **relación
contemporánea de los sectores hacia la población activa** — la *ecuación de
participación*. Y tiene una lectura económica precisa:

> «Cuando `E` es mayor, dado `A`, la proporción de activos que trabajan es mayor. Si esta
> proporción se interpreta como la **probabilidad de trabajar de un activo**…»

Traducido: **más empleo → más gente se anima a incorporarse a la población activa.** Es
el *efecto del trabajador animado*, y sale de rechazar una hipótesis, no de imponerla.

> ### Heurística 1 — Di cuáles de tus supuestos no puedes contrastar
> Un modelo *siempre* supone algo que los datos no dicen. Lo honesto no es fingir que
> todo se contrasta: es **separar las dos listas** y decir cuál es cuál. Eso marca
> exactamente dónde está el contenido empírico y dónde el supuesto — y hace el resultado
> **más fuerte**, no más débil.

---

## 4. Por qué transferencias y no un VAR/VARMA

Cinco razones, y ninguna es la costumbre.

### 4.1 El sistema se identifica por ECONOMÍA, no por convención estadística

Un VAR estima **todo contra todo** y, para interpretar, necesita después una **ordenación
de Cholesky**: hay que decidir quién choca primero. **Esa ordenación la elige el
analista**, y la respuesta al impulso cambia con ella. Es una convención.

Relloso hace lo contrario. **Declara las exclusiones antes de estimar**, dice cuáles son
supuestos y cuáles hipótesis, y **contrasta las segundas**. Cuando una falla, el modelo
cambia — y ahí aparece la ecuación de participación.

> ### Heurística 2 — La ordenación no se elige: se hipotetiza y se contrasta
> Un modelo de transferencia **no escapa** del problema de identificación
> contemporánea: si hay un término en el retardo cero, es un sistema recursivo
> identificado por una ordenación (véase la nota técnica, §5). Lo que escapa es de la
> **arbitrariedad**: la ordenación es la exogeneidad, que se declara y se contrasta con
> la correlación cruzada en retardos negativos.

### 4.2 Los grados de libertad no dan

**69 observaciones.** Tras diferenciar, **64**.

Un VAR(4) de siete variables tendría **7 × 7 × 4 = 196 coeficientes AR**, más 28 de
covarianza. Con 64 observaciones **no hay nada que hacer**.

La estructura de transferencias impone **cientos de ceros a priori** — y los impone la
teoría, no un criterio de información. Aun así, el modelo final tiene unos **60
parámetros sobre 64 observaciones**: ya en el límite.

> ### Heurística 3 — Con muestras cortas, la estructura no es un lujo: es la única vía
> La pregunta no es «¿cuántos parámetros me deja meter el criterio de información?» sino
> «¿qué ceros puedo justificar antes de mirar los datos?».

### 4.3 Cada serie necesita SU dinámica

- `EA` (agricultura) lleva **∇∇₄**; las demás, **∇²**.
- Cada una tiene su MEG, con **factores de frecuencia fija** distintos.
- Y **17 escalones de intervención**, sobre todo el cambio metodológico de la EPA en
  1987:II, más las correcciones de los eventuales agrarios (1984) y de Ceuta y Melilla
  (1988).

**Un VAR impone la misma estructura de retardos a todas las ecuaciones.** No puede llevar
un ∇∇₄ en una serie y un ∇² en otra, ni un factor estacional estocástico en una y
determinista en otra.

> ### Heurística 4 — El modelo univariante de cada serie es sagrado
> El multivariante se construye **sobre** los univariantes, no **contra** ellos. Si el
> sistema te obliga a estropear el modelo de una serie, el sistema está mal planteado.
>
> *Comprobación independiente:* al reproducir el ejercicio de traslado del crudo al IPC
> (España, Francia, Alemania) con un VAR, el portmanteau **falla** en Francia (p = 0.006)
> y Alemania (p = 0.018) — exactamente porque el VAR no puede llevar el SAR(1)₁₂ que
> Francia necesita ni el AR(3)+SAR que necesita Alemania. **El mismo defecto, encontrado
> treinta años después y por otro camino.**

### 4.4 Las relaciones son unidireccionales y con retardos racionales

Una transferencia es `ν(B) = ω(B)·Bᵇ/δ(B)`: un **retardo racional**, con memoria
infinita si `δ ≠ 0`, escrito con dos o tres parámetros.

Un VAR solo puede aproximarlo con un polinomio **finito** → hay que subir `p` → y al
subir `p` aparecen coeficientes fuera de la diagonal en retardos altos que fabrican
**realimentación espuria**. (En el ejercicio del crudo, medido: con `p ≥ 2` aparece una
realimentación IPC → WTI que no existe.)

### 4.5 Las identidades contables son exactas, y un VAR las rompe

`E = ΣEᵢ` no es una regresión: es una definición. Un VAR estimaría siete ecuaciones
libres y produciría previsiones **que no cumplen la identidad**.

El enfoque correcto: **modelar los componentes, y derivar los agregados por la
identidad** — *después* de prever, con la varianza correcta `c′Vc`.

> ### Heurística 5 — Una identidad no se estima: se impone, y después se propaga
> La banda del agregado **no es** la suma de las bandas. Los errores de previsión de los
> componentes están **correlacionados** (comparten innovaciones a través del sistema), así
> que la varianza es `c′Vc` con `V` la matriz completa del error de previsión.
> Ignorarlo produce bandas demasiado estrechas o demasiado anchas según el signo de la
> correlación.

---

## 5. La escalera metodológica

Ésta es la parte que hay que enseñar, porque es **un procedimiento**, y sin él el modelo
final parece magia.

### Paso 1 — Los univariantes (US/UTI, y luego MEG)

Cada serie por separado: Box-Cox (aquí, ninguna la necesita), órdenes de diferenciación,
ARMA estacionario, anomalías, intervenciones, diagnosis. **Iterativo, y con gráficos**:

> «Los **informes gráficos** son una herramienta imprescindible en este proceso
> iterativo.»

Luego el **MEG** (Modelo Estacional Generalizado), que permite decidir, **frecuencia a
frecuencia**, si la estacionalidad es determinista o estocástica.

> ### Heurística 6 — Significativo dentro de muestra ≠ mejor previsión
> Un factor estacional estocástico puede mejorar el AIC/BIC dentro de muestra y **prever
> peor**. La alarma es la **cuasi-cancelación**: si el MA de una raíz unitaria estacional
> converge a `θ² → 1`, esa frecuencia es *de facto determinista* y el modelo estocástico
> solo añade varianza. **El árbitro es la previsión fuera de muestra.**

### Paso 2 — El conjunto, con dinámica DIAGONAL y covarianzas SIN RESTRINGIR

> «El **primer paso** con un conjunto de m variables consiste en estimar los m modelos
> US (ó UTI) **de forma conjunta**, como un modelo MS con **dinámica diagonal** —es decir,
> con matrices AR y MA diagonales— pero con **matriz de varianzas y covarianzas
> contemporáneas sin restringir**.»

¿Para qué?

1. **Despeja dudas** que el análisis univariante deja abiertas (p. ej. el orden de
   integración de alguna serie).
2. **Estima mejor** los parámetros de los univariantes (usa la información cruzada).
3. Da una **evaluación inicial de las correlaciones contemporáneas**.
4. Y, sobre todo: **las CCF residuales dicen qué relaciones dinámicas hay.**

> ### Heurística 7 — Empieza por la matriz de covarianzas, no por la dinámica
> Muñoz Polo (2001, §2.4) lo formula en general: *«la especificación de las relaciones
> bivariantes **puede comenzar con la modificación de la matriz Σ**»*.
>
> Y hay una razón profunda: **una correlación contemporánea y una transferencia
> contemporánea explican lo mismo en el retardo cero.** No se pueden estimar las dos. Así
> que primero se mira cuánta correlación hay, y *después* se decide si hay que
> estructurarla.

### Paso 3 — Leer las CCF residuales

Las correlaciones cruzadas entre los residuos del modelo diagonal dicen **dónde** están
las relaciones y **con qué retardo**. Es el preblanqueo de Box‑Jenkins, aplicado al
sistema.

> ### Heurística 8 — Arregla la RELACIÓN antes que el RUIDO
> La contaminación va en **un solo sentido**. Una relación mal especificada deja parte del
> input **dentro** del ruido, así que **sí** ensucia la ACF residual. Un ruido mal
> especificado **no puede** ensuciar la CCF.
>
> Luego: una ACF residual fea **no es evidencia contra el ruido** mientras la CCF siga
> hablando. Arreglar el ruido primero es perseguir un síntoma.
> *(Muñoz Polo 2001, §2.6.)*

### Paso 4 — Añadir la dinámica

Solo entonces se introducen las relaciones dinámicas (los términos fuera de la diagonal),
y se vuelve a diagnosticar. El modelo final del legacy (`m6-1`) es el resultado de este
paso, **no el punto de partida**.

---

## 6. El resultado

> «Reducción **(62%) de la varianza residual de Población Activa** en relación a la
> generada por el modelo univariante.»

Ése es el premio: **desagregar el empleo en sectores y modelar sus relaciones reduce en
un 62% la varianza del error de previsión de la población activa.**

Y la conclusión del trabajo:

> «Es evidente, por tanto, la importancia de **desagregar el total de Ocupados en sectores
> productivos** y analizar a fondo tanto las relaciones que existen entre los sectores como
> las de éstos con la Población Total y la Población Activa.»

---

## 7. Qué de todo esto ejecuta drtran hoy

| paso | ¿drtran? | cómo |
|---|---|---|
| 1. Univariantes | sí | es `fue` (los `.pre`) |
| 2. Conjunto diagonal + covarianzas libres | **sí** | `-0` con `q[i,j] = free` |
| 3. CCF residuales | **sí** | la diagnosis de adecuación y exogeneidad |
| 4. Dinámica fuera de la diagonal | **parcialmente** | la red (`-n`) sí; los **numeradores factorizados**, no (la tabla de slots necesita **productos**) |
| Identidades → agregados con `c′Vc` | **sí** | `-a` |
| Previsión recursiva fuera de muestra | **sí** | `-R` |

**Lo que falta está identificado y es acotado:** la tabla de restricciones expresa
LIBRE / FIJO / ALIAS (igualdad), y para reproducir el modelo final del legacy hace falta
que exprese **productos** — porque los numeradores de sus transferencias están
factorizados:

```
ω₃₄(B) = x5·B·(1 − x6·B)
ω₃₆(B) = B·(1 − x14·B)·(x12 + x13·B)
```

---

## 8. Y una constatación que cambia el marco

Las dos tesis de la escuela dicen, explícitamente, que estiman por **Máxima Verosimilitud
Exacta NO CONDICIONADA** con el algoritmo de Mauricio (1995, 1996, 1997) — **no** con la
aproximación condicional de Box‑Jenkins/TASTE:

> «Todos los modelos empíricos presentados se estiman por el criterio de Máxima
> Verosimilitud Exacta (MVE) **No Condicionada**, mediante el algoritmo de Mauricio.»
> *(Muñoz Polo 2001)*

> «El desarrollo de este trabajo **no habría sido factible** de no disponer de los
> algoritmos […] desarrollados por **Mauricio (1995, 1996 y 1997)**.»
> *(Relloso 1997)*

**La escuela ya usaba `elf`, y era consciente de lo que eso significaba.** La metodología
es de Box y Jenkins; el motor, no. Y esa combinación —el criterio de una escuela sobre un
motor exacto— es exactamente lo que drtran intenta preservar.

---

## Referencias

- **Relloso Pereda, S. (1997).** *Un modelo multivariante para el empleo por sectores,
  activos y parados en España.* Documento de trabajo ICAE **9720**, Universidad Complutense
  de Madrid. Tesis dirigida por A. B. Treadway.
- **Muñoz Polo, M. S. (2001).** *Estudios econométricos de las series temporales de la
  industria española.* Tesis doctoral, UCM. Director: A. B. Treadway. — §2.4 (variables
  ligadas por identidad), §2.6 (orden de reformulación).
- **Box, G. E. P., Jenkins, G. M. y Reinsel, G. C. (1994).** *Time Series Analysis:
  Forecasting and Control*, 3ª ed. — caps. 10–11.
- **Mauricio, J. A. (1995, 1996, 1997).** El algoritmo de verosimilitud exacta multivariante.
- **Gallego, A. y Treadway, A. B. (1996).** El Modelo Estacional Generalizado (MEG).
- **Tiao, G. C. y Box, G. E. P. (1981); Jenkins, G. M. y Alavi, A. (1981).** Las dos obras
  que inician la metodología multivariante que Relloso emplea.

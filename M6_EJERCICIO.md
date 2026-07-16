# El ejercicio m6: reproducir el modelo del empleo por sectores

Estado a 14 de julio de 2026. Documento de trabajo: qué se ha hecho, qué se ha
encontrado, y qué falta.

---

## 1. Qué es m6 y por qué importa

`m6` es el sistema multivariante del **mercado laboral español** (EPA trimestral):
población en edad de trabajar, ocupados por sectores productivos, activos y parados.
Es el modelo más complejo del legacy —seis o siete series, ~60 parámetros— y es la
razón por la que drtran existe: **todo lo que hemos construido (la red de
transferencias, los parámetros compartidos, las covarianzas liberadas, los agregados
con varianza `c′Vc`) se construyó *para* él.**

Y no lo habíamos corrido nunca.

## 2. La fuente (encontrada, y a tiempo)

Estuve a punto de reconstruir m6 a ciegas desde el `shootx` del legacy. **No hay que
hacer eso.** La teoría está publicada:

> **Relloso Pereda, S. (1997).** *Un modelo multivariante para el empleo por sectores,
> activos y parados en España.* Documento de trabajo ICAE **9720**, Universidad
> Complutense de Madrid. Tesis doctoral dirigida por **A. B. Treadway**.
> → `drvarma_source/literature/9720.pdf`

Complementada por:

> **Muñoz Polo, M. S. (2001).** *Estudios econométricos de las series temporales de la
> industria española.* Tesis doctoral, UCM. Director: A. B. Treadway.
> → `drvarma_source/literature/T24986.pdf` — §2.4 (variables ligadas por identidad).

Y el código: `drv-source/m6-1/` (`drv.c` + `m1.inp`).

## 3. LO PRIMERO QUE HAY QUE ENTENDER: la escuela usaba `elf`, y lo sabía

Esto zanja una duda que veníamos arrastrando.

**Relloso (1997), Introducción:**

> «Todos los modelos presentados están estimados por **Máxima Verosimilitud Exacta
> (MVE)** […] el desarrollo de este trabajo **no habría sido factible de no disponer de
> los algoritmos de evaluación y optimización de la función de verosimilitud de los
> modelos ARMA multivariantes desarrollados por Mauricio (1995, 1996 y 1997)**.»

**Muñoz Polo (2001), cap. 1**, con la palabra que lo decide:

> «Todos los modelos empíricos presentados se estiman por el criterio de Máxima
> Verosimilitud Exacta (MVE) **No Condicionada**, mediante el algoritmo de Mauricio
> (1995, 1996, 1997).»

**«No Condicionada».** Es exactamente la distinción entre la verosimilitud exacta y la
aproximación condicional de Box–Jenkins/TASTE, y es la que nos ha ocupado toda la
semana. La escuela **no** usaba la aproximación de TASTE para estos modelos, era
consciente de ello, y lo dejó escrito.

**Consecuencia para drtran.** El cast por resta —construir el ruido fuera del motor,
truncando el pasado del input— era **nuestro** invento, no el suyo. Al empotrar la
transferencia en el VARMA (opción `-V`, hoy el defecto) hemos devuelto a drtran al sitio
donde la escuela ya estaba en 1997.

## 4. LA METODOLOGÍA, tal como el documento la prescribe

Y aquí está el segundo hallazgo: **yo había tomado el modelo equivocado como punto de
partida.**

**Relloso, §4:**

> «En la metodología MS aquí empleada, **el primer paso** con un conjunto de m variables
> consiste en **estimar los m modelos US (ó UTI) de forma conjunta, como un modelo MS con
> dinámica diagonal**, es decir, con matrices AR y MA diagonales, pero con **matriz de
> varianzas y covarianzas contemporáneas sin restringir**. Esta estimación conjunta puede
> despejar a menudo las dudas que surgen en los análisis US y UTI sobre el grado de
> integración de las variables […] y en muchos casos constituye un buen modelo MS de
> partida desde el cual proceder a una reformulación. También permite obtener una
> **evaluación inicial de las correlaciones contemporáneas** y, consultando las
> **funciones de correlación cruzadas (ccf's) residuales**, adquirir una primera idea de
> cuáles son los rasgos más destacables de las relaciones entre las variables.»

Y más adelante:

> «Se detectan **fuertes correlaciones contemporáneas** entre algunos de los sectores, lo
> que conduce a la especificación de un **modelo pentavariante con dinámica diagonal** […]
> **En la diagonal principal de la matriz MA se recogen los modelos UTI de las variables.**»

Es decir, **la escalera es**:

| paso | modelo | ¿drtran lo hace? |
|------|--------|------------------|
| 1 | Univariantes (UTI / MEG) de cada serie | sí (es fue) |
| 2 | **Conjunto, dinámica DIAGONAL, covarianzas SIN RESTRINGIR** | **sí, hoy: `-0` + `q[i,j] = free`** |
| 3 | Leer las **CCF residuales** → qué relaciones hay | **sí, hoy** |
| 4 | Añadir la dinámica: MA fuera de la diagonal | **NO** (ver §6) |

`m6-1` —el VMA(4) con MA fuera de la diagonal que hay en el legacy— es el **paso 4**, el
modelo *final*. Yo lo había tomado por el punto de partida. Si hubiera seguido, habría
intentado reproducir el destino saltándome el camino.

**El paso 2 es exactamente lo que drtran sabe hacer y nunca hemos corrido.** Ése es el
ejercicio.

## 5. Lo reconstruido

`tests/build_m6.py` genera los seis `.pre` desde `drv-source/m6-1/m1.inp`:

| serie | qué es | diferencias | deterministas |
|-------|--------|-------------|---------------|
| P  | población en edad de trabajar | ∇² | — |
| EA | agricultura, ganadería y pesca | ∇∇₄ | 5 |
| EP | servicios privados | ∇² | 9 |
| EI | industria | ∇² | 9 |
| EU | servicios públicos | ∇² | 6 |
| EC | construcción | ∇² | 7 |

- **Datos**: EPA trimestral, 1976:III – 1993:II, **69 observaciones**. Están completos en
  `m1.inp`.
- **Sin Box-Cox** (λ = 1; se trabaja en niveles) y sin reescalado.
- **23 deterministas** generadas por `GenDet` en el legacy: 17 escalones (cambios
  metodológicos de la EPA, sobre todo el de 1987:II), 2 impulsos, 1 impulso compuesto,
  `cos`/`sin` de frecuencia 1, y el alternante. Todas son tipos que el lector de `.pre`
  de drtran ya soporta.

**Estado:** las seis series se leen y el sistema de 6 variables **corre**: 64
observaciones (69 − 5 por la diferenciación), 47 parámetros, 37 coeficientes
deterministas. **No converge**, y no debe extrañar: los ARMA univariantes que puse son un
MA(1) genérico **de relleno**. Los de verdad son los MEG de la Tabla 4 del documento.

### Un detalle del formato `.pre` que costó encontrar

El bloque determinista lleva **dos** líneas de enteros —`Nomega` y `Ndelta`—, no una.
Omitir la segunda desplaza todo el parseo y hace que la **última observación se lea como
cero**, con un error críptico de Box-Cox. Anotado porque volverá a morder.

## 6. LA CARENCIA QUE ESTE EJERCICIO DESTAPA

El paso 4 —la dinámica fuera de la diagonal— **drtran no lo puede expresar hoy**, y ahora
sabemos exactamente por qué.

Los coeficientes fuera de la diagonal del `shootx` de m6-1 son **productos de
parámetros** (`x5*x6`, `x12*x14 - x13`, `x2*x3*x4`). Al factorizarlos se ve qué son:

```
Theta_34(B) = x5 · B · (1 − x6·B)                    numerador FACTORIZADO
Theta_36(B) = B · (1 − x14·B) · (x12 + x13·B)        idem
Theta_22(B) = (1 − x2·B)(1 + x4·B)(1 + x3·B²)        MA(1) × MA(1) × frec. fija (f=1)
```

Los **diagonales** (como `Theta_22`) drtran los expresa perfectamente: son la estructura
factorizada del `.pre` de fue —regular × anual × frecuencia fija— y `expand_ma_factors`
los despliega.

Los **de fuera de la diagonal, no.** Una transferencia con **numerador factorizado**
exige que la tabla de slots exprese **productos**, y hoy solo expresa:

```
LIBRE          x
FIJO           x = 0.6
ALIAS          x = y          (igualdad)
```

Falta:

```
PRODUCTO       x = y * z      (y, en general, polinomios factorizados en omega(B))
```

**Ésa es la carencia real, y es concreta y acotada.** No es un problema de diseño del
cast ni del motor: es una limitación del lenguaje de restricciones.

## 7. Lo que este ejercicio ya ha dado (aunque no haya corrido todavía)

Cuatro cosas, ninguna esperada:

1. **La escuela usaba `elf`, y lo sabía** (§3). Valida retroactivamente el cast empotrado.
2. **El punto de partida era otro** (§4). Me habría estrellado reproduciendo el modelo
   final sin los tres pasos previos.
3. **La carencia de los productos** (§6). Objetivo concreto para la tabla de slots.
4. Y, por el camino, **cinco bugs vivos** que la formalización destapó y que ninguna
   sesión de programación habría encontrado: los residuos sin des-normalizar, el doble
   conteo en la previsión, las covarianzas liberadas ignoradas, la descomposición de
   varianza no disponible, y el **cuelgue infinito** del optimizador (que llevaba latente
   *todo el proyecto*).

La lección de fondo: **formalizar no fue un lujo académico. Fue el método por el que
descubrimos que el programa no era correcto.**

## 8. Lo que falta, y por dónde

**Los seis modelos univariantes (UTI/MEG).** No transcribirlos a mano de un OCR a dos
columnas —eso es exactamente la reconstrucción a ciegas que hay que evitar—, sino
**construirlos con ART**, que está en disco (`/home/david/Dropbox/SRC/ART/art-python`) y es
importable, y que es la herramienta de identificación de la casa.

**La Tabla 4 del documento sirve de CONTROL, no de fuente.** Lo que ART identifique debe
parecerse a lo que allí está; si no se parece, hay que entender por qué antes de seguir.

> **Actualización (jul-2026) — el plan se concretó: grabar la Tabla 4.** En vez de
> re-identificar cada serie, se **graba la especificación publicada** (Tabla 4, la
> representación más estocástica) como valores de arranque de los seis `.pre`, vía
> `build_m6.py`, con **las intervenciones de la Tabla 4**. El detalle completo —la Tabla 4
> transcrita, la codificación `.pre` del MEG ∇∇₄, y por serie— está en
> **`docs/M6_TABLA4_BASELINE.md`**, que es la línea de contexto de este trabajo.
>
> **EA ya está verificado** (reproduce Tabla 4: θ=.43, λ₁=−.68, λ₂=−.72, σ=27.0, APROBADO).
> Hallazgo clave: `∇∇₄` se codifica como **d=2 + ifadf[π/2,π]** (no d=1); ver el doc §3.
> Pendientes: P, EP, EC, EI, EU + extender `build_m6.py` (MEG, ifadf de 3 valores para s=4,
> intervenciones compuestas).

Luego, el **paso 2** de la metodología, que es lo que drtran ya sabe hacer:

```sh
drtran M6_P.pre M6_EA.pre M6_EP.pre M6_EI.pre M6_EU.pre M6_EC.pre \
       -0 -c m6.cns -m M6_diag
```

con `m6.cns` liberando las covarianzas contemporáneas. Y leer las **CCF residuales**,
que es lo que el documento manda: son las que dicen qué relaciones dinámicas hay.

Y sólo entonces, el **paso 4** — que necesita los productos en la tabla de slots.

---

## Apéndice: el orden de las series

En `m1.inp` (6 columnas), y por tanto en `build_m6.py`:

```
1  P    población en edad de trabajar menos la contada aparte
2  EA   agricultura, ganadería y pesca
3  EP   servicios privados
4  EI   industria
5  EU   servicios públicos
6  EC   construcción
```

Las identidades contables del Modelo Conceptual Inicial de Relloso son
`E = EA + EI + EC + EP + EU` (ocupados) y `A = E + D` (activos = ocupados + parados). Los
**agregados con varianza `c′Vc`** de drtran (`-a`) son justamente para eso, y tampoco se
han ejercitado.

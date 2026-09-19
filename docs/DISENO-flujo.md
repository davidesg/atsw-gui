# DISEÑO — el flujo, y qué página es cada etapa

`DISENO-mtram.md` dice **qué hace** cada pantalla. `DISENO-interfaz.md`, **cómo
se presenta**. Este documento contesta la que faltaba y es la primera:
**¿dónde estoy en el método, y qué estoy decidiendo aquí?**

Se escribe porque la interfaz no la contestaba. Con las siete páginas hechas
seguía sin verse **dónde se especifica un modelo y dónde se modifica**, y la
pregunta *«¿cómo se puebla la red?»* no tenía respuesta dentro del programa.

---

## 1. El ciclo

El de Box y Jenkins, sin adornos:

    identificación → especificación → estimación → diagnosis → ¿vale?
                                                                 │
                              ┌──────── no ───────────────────────┤
                              ▼                                   │ sí
                        reformulación                             ▼
                                                                 uso

Lo que mtram añade es que **la diagnosis dice a dónde volver**, y no es siempre
al mismo sitio.

---

## 2. El flujo, con las páginas

```
   ┌──────────────┐
   │   SERIES     │  los .pre, uno por serie ······· vienen de fue
   │              │  papeles · ventana común · ∇     (aquí no se tocan)
   └──────┬───────┘
          │ el conjunto, ordenado
          ▼
   ┌──────────────┐
   │IDENTIFICACIÓN│  CCF preblanqueada contra CADA candidata
   │              │  → propone (b, s) · descarta las no exógenas
   └──────┬───────┘
          │ los enlaces que merecen existir
          ▼
   ┌──────────────┐  ╮
   │     RED      │  │  .dag   qué alimenta a qué
   │              │  │         aciclicidad · orden de construcción
   └──────┬───────┘  │
          │          ├─ ESPECIFICACIÓN
   ┌──────▼───────┐  │
   │    MODELO    │  │  .cns   restricciones de ω, δ  ·  Σ
   └──────┬───────┘  ╯
          │ .dag + .cns
          ▼
   ┌──────────────┐
   │  ESTIMACIÓN  │  drtran → .out
   └──────┬───────┘
          │
          ▼
   ┌──────────────┐   k ≥ 0 falta estructura ──→ vuelve a IDENTIFICACIÓN
   │  DIAGNOSIS   │───k < 0 no es exógena ─────→ vuelve a RED
   └──────┬───────┘   residuos no blancos ────→ vuelve a FUE
          │ vale
          ▼
   ┌──────────────┐
   │  PREVISIÓN   │  uso
   └──────────────┘
```

**Red y Modelo son UNA sola etapa.** Están partidas en dos por el formato de
ficheros —`.dag` y `.cns`— no por el método. Eso explica por qué no se veía
dónde se especifica: la etapa no tiene nombre en la interfaz.

**La diagnosis tiene tres salidas distintas**, y ya sabe cuál es cuál:

| lo que falla | a dónde se vuelve | por qué |
|---|---|---|
| CCF del enlace en k ≥ 0 | **Identificación** | la forma (b, r, s) no agota la relación |
| CCF del enlace en k < 0 | **Red** | el enlace no debería existir |
| residuos de una ecuación no blancos | **fue** | el modelo univariante está mal |
| casi-colinealidad | **Modelo** | sobra una de las dos cosas |

---

## 3. Dónde nace cada cosa y dónde se toca

| | nace en | se modifica en |
|---|---|---|
| el `.pre` de una serie | **fue** | **fue** — aquí nunca |
| un enlace, con su `(b, r, s)` | Identificación *(propone)* | **Red** |
| las restricciones de ω y δ | — | **Modelo** |
| Σ, las covarianzas | — | **Modelo** |
| qué mantiene fijo el estimador | — | **Estimación** |
| el cast `-V` / `-S` | — | **Estimación** — y en Series NO se menciona |

Las dos últimas filas son la corrección que hizo falta: `-N/-X/-D/-E/-M` y el
cast dicen **cómo se estima**, no **qué es el modelo**. Estaban en Modelo y van
a Estimación.

---

## 4. Las restricciones de ω y δ: ¿confunden?

**No, si se escriben como lo que son.** Lo que confunde es enseñarlas en el
idioma del motor:

    omega1[1] = omega1[0] * theta_2[B^1]
    omega3[0] = omega3[1] + omega3[2] + omega3[3]

Eso es **cómo el motor las impone**, no **qué dicen**. Lo que dicen es:

    ω1(B) = ω1₀ (1 − θ_EI B)        el numerador se factoriza y COMPARTE
                                     la MA de EI: un solo grado de libertad

    ω3(1) = 0                        el numerador lleva un (1−B) FIJO: la
                                     ganancia a largo plazo es CERO, EU afecta
                                     a los cambios de EI y no a su nivel

La segunda es una afirmación económica fuerte y el analista **tiene que
verla**. Escondérsela sería peor que confundirle.

> **La regla: se enseña el enunciado del modelo, no la mecánica del motor.** El
> `omega1[1] = ...` vive en el `.cns` y ahí se queda; en pantalla va lo que
> significa.

### RECTIFICACIÓN: los PRODUCTOS son empotramiento, no especificación

Escribí aquí que el producto `ω1(B) = ω1₀(1 − θ_EI B)` había que enseñarlo
porque «ata la transferencia al modelo de ruido de EI». **Era un error**, y la
prueba está en este mismo repositorio.

`LEGACY_M6.md` §9 — *«la tabla de slots necesita PRODUCTOS»*:

> Los coeficientes **fuera de la diagonal** del `shootx` de m6-1 son
> **productos de parámetros**: `x5*x6`, `x12*x14 − x13`, `x2*x3*x4`. Al
> factorizarlos se ve lo que son: `Theta_34(B) = x5·B·(1 − x6 B)`.

`Θ₃₄` es un elemento **fuera de la diagonal de la matriz MA del VARMA**. El
mecanismo PRODUCTO se añadió para poder **reproducir el m6-1 como un VARMA
restringido** — es decir, para expresar la forma **empotrada**.

> **Un analista que especifica una FLT elige `(b, r, s)`. Nada más.** Los
> productos aparecen cuando esa FLT se escribe como VARMA restringido, y eso es
> cosa de la máquina.

Ponerle la etiqueta «atado al ruido de EI» era **vestir la máquina de modelo**,
que es exactamente lo que este documento dice que no se haga.

### Pero no todas las restricciones son lo mismo

Y esto sí hay que distinguirlo, porque las tres del `.cns` no tienen la misma
naturaleza:

| restricción | qué es |
|---|---|
| `omega3[0] = omega3[1]+…` | **modelo**: ω(1) = 0, ganancia a largo plazo cero |
| `delta1[1] = phi_2[B^1]` | **modelo**: el denominador ES el AR de la entrada — *«a rational transfer»*, lo dice el `-h` del motor |
| `omega1[1] = omega1[0]*theta_2` | **empotramiento**: reproduce un Θ fuera de la diagonal |

Las dos primeras son afirmaciones sobre ν(B) y se enuncian. La tercera es la
forma empotrada y **no se disfraza de especificación**.

### Lo que sí hay que ver del enunciado

Lo que se enseña es el **enunciado**, no la mecánica del `.cns`:

La combinación lineal se enuncia por lo que **afirma** y no por cómo se impone:

```
          ω3(1) = 0 · un (1−B) FIJO: ganancia a largo plazo CERO
```

Lo que **sí** hay que esconder es **cómo se empotra**: que la transferencia se
convierte en coeficientes fuera de la diagonal de Φ(B) del VARMA es asunto del
motor. El analista especifica ν(B); el cast es cosa de la máquina.

---

## 4bis. La forma de ECUACIONES no es la forma en que se estima

Es la corrección más importante del documento, y me la tuvieron que señalar.

**Un modelo de transferencia son DOS ecuaciones por serie**, no una:

```
   EP_t  =  ν1(B) EI_t  +  ν2(B) EC_t  +  N_EP,t         ← LOS NIVELES

   (1-B)² [ N_EP,t − D_EP,t ]  =  θ_EP(B) a_EP,t         ← el ruido, y AQUÍ
                                                           va la diferenciación
```

Y la primera va **en niveles**. Lo dice el motor en tres sitios
(`drtran.c:51`, `:3821`, y la batería en `test_battery.sh:2090`):

> El modelo dice que la transferencia relaciona los **NIVELES** y que la
> diferenciación la lleva el ruido.

No es una convención de escritura: la batería lo comprueba (BUG-8). Si el cast
empotrado ajustara ν·Δ con Δ(1) = 0, **la ganancia saldría aniquilada**.

Yo tenía escrito `EP_t = [ω1(B)]B¹ EI + … + N_EP,t` con `(1-B)²` colgando de la
misma línea, lo que sugiere que se diferencia la transferencia. Es justo lo
contrario.

### Y de ahí la regla general

> **No se mezclan la especificación y el empotramiento.** Una cosa es el
> modelo —dos ecuaciones, la transferencia en niveles y el ruido con su
> diferenciación— y otra cómo drtran lo mete en un VARMA para estimarlo.

El cast (`-V` / `-S`), que la transferencia acabe siendo coeficientes fuera de
la diagonal de Φ(B), y qué se mantiene fijo (`-N/-X/…`) **son máquina**. Viven
en Estimación. En Red y en Modelo no aparece ni la palabra.

Esto explica por qué el orden de los ∇ importa aunque el modelo sea de
niveles: si la salida va a ∇∇₁₂ y la entrada a ∇, el empotrado no puede
representar la relación de niveles y el motor **despacha al cast por resta**.
Es una consecuencia de la máquina sobre lo que la máquina puede, no un cambio
del modelo.

---

## 5. Cómo simplificar el GUI

Siete pestañas para cinco etapas. Lo que sigue, por orden de lo que más
simplifica:

### 5.1 Fundir Red y Modelo en una sola: **Especificación**

Son una etapa. Y hay una razón más fuerte que la simetría: **el objeto que se
especifica es el mismo** —el sistema de ecuaciones— y hoy está partido en dos
vistas que hablan de él por separado. Una página:

```
▾ EP_t = [ω1(B)]B¹ EI + [ω2(B)]B¹ EC + N_EP,t      N_EP: (1-B)²·MA 1·det 4
     ω1 ← EI   b=1 r=0 s=1        ω1(B) = ω1₀(1 − θ_EI B)
     ω2 ← EC   b=1 r=0 s=2        libre
▾ EI_t = [ω3(B)]B¹ EU + N_EI,t                     N_EI: (1-B)²·MA 1·det 9
     ω3 ← EU   b=1 r=0 s=3        ω3(1) = 0   (un (1−B) fijo)
▾ Σ    3 de 15 covarianzas libres

● Acíclica · orden EC → EU → EI → EP · 11 parámetros de transferencia
● 3 de 15 covarianzas libres
```

Con eso, **seis pestañas y seis etapas**, una por una.

### 5.2 Que cada página diga cuál es el paso siguiente

Una línea, al estilo del `Mes[]` de TASTE, pero sobre el **flujo** y no sobre
la opción: *«lo siguiente: define la red»*, *«lo siguiente: estima»*. Es lo que
convierte siete pantallas en un camino.

### 5.3 Que la diagnosis lleve de vuelta

El veredicto ya sabe a dónde hay que volver (§2). Que sea un **enlace**: pulsar
«falta estructura en EC» y aterrizar en Identificación con EC marcada.

### 5.4 Numerar las pestañas

`1 Series · 2 Identificación · 3 Red · 4 Modelo · …` El número no es adorno:
dice que hay un orden, y cuál.

### 5.5 Lo que NO simplifica y conviene no hacer

- **Un asistente que encadene los pasos.** La escuela no trabaja así: se vuelve
  atrás constantemente, y un asistente lineal estorba en cuanto la diagnosis
  manda a Identificación.
- **Esconder las páginas hasta que «toquen».** Es la tentación contraria a la
  regla de TASTE, que resultó ser lo mejor de su diseño: *nada se grisa; cada
  pantalla pide por su nombre lo que le falta.*

---

## 6. Lo que se implementó de aquí

1. **El orden de las pestañas**: Identificación **antes** que Red, porque es lo
   que la puebla.
2. **`Añadir a la red`** en Identificación: el gesto que faltaba, y sin el cual
   la pregunta «¿cómo se puebla la red?» no tenía respuesta dentro del
   programa.
3. **`-N/-X/-D/-E/-M` a Estimación**, junto al cast: dicen cómo se estima.
4. **El ruido sale del árbol de Modelo** y pasa a las columnas de la ecuación:
   una línea con su estructura y su cuenta, sin desglose, porque es un dato.
5. **Pestañas numeradas.**
6. **Las dos ecuaciones** (§4bis): la transferencia en niveles y el ruido con
   su diferenciación, cada una en su línea.

Queda propuesto y sin hacer: §5.1 (fundir Red y Modelo), §5.2 y §5.3.

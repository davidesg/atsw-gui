# DISEÑO — la página Diagnosis

Se escribe aparte porque no es una revisión de presentación como las otras:
**cambia qué se calcula**, y hay tres juicios de funcionalidad que conviene
dejar razonados antes de escribir una línea.

---

## 1. Las preguntas del analista, en su orden

1. ¿La red es acíclica, o esto debería ser un **VARMA**?
2. ¿La estructura es **adecuada** y está bien parametrizada?
3. ¿Cuál es la **bondad del ajuste**, y cuánto ha cambiado?
4. ¿Cómo han cambiado la **ACF/PACF** y la **CCF con la entrada**?
5. El **gráfico de residuos**.
6. El **modelo estimado**, con sus desviaciones típicas.

Ese orden no es caprichoso: **va de lo que invalida el modelo a lo que lo
matiza.** Si (1) falla, (2)…(6) no importan. La página lo respeta.

---

## 2. Tres juicios de funcionalidad

### 2.1 La aciclicidad de Red y la exogeneidad de Diagnosis NO son lo mismo

Es la distinción que la interfaz hoy no hace, y es la primera pregunta.

| | qué es | cuándo se sabe |
|---|---|---|
| **Red** dice «acíclica» | que el `.dag` **admite** orden de construcción | **antes** de estimar — es estructural |
| **Diagnosis** dice «exógena» | que **los datos** sostienen que la entrada no responde a la salida | **después** — es empírico |

Un `.dag` puede ser perfectamente acíclico y los datos decir que no lo es. Eso
es exactamente *«esto debería ser un VARMA»*, y se contesta **aquí**.

> La primera pestaña es la exogeneidad, y su veredicto nombra el escalón: si
> falla, **drvarma**.

### 2.2 R² **sí** — el de Brajín (A.28), sobre la serie estacionaria

> **Rectificación.** La primera versión de esta sección decía «R² no, sólo LR».
> Estaba mal, y el usuario tenía razón al insistir. Lo que no tiene sentido es
> un R² sobre el **nivel** de una I(1): sale cerca de 1 por construcción y no
> dice nada. El de la escuela no es ése.

`mtram` —el servidor MCP— **ya lo da**, y lo da porque es la tercera de las
tres cifras con que Brajín cierra cada caso (`school.py:r2_brajin`). Va sobre
la serie **estacionaria**:

    R² = 1 − Σ (a_t − ā)² / Σ (w_t − w̄)²,     w_t = ∇ᵈ ∇ₛᴰ z_t

> «La desviación típica residual estimada pasa de 0.53 % en el modelo
> univariante a 0.42 % en el Modelo rpu6.3. El R² en el modelo univariante de
> ru es 0.54, mientras que, en el Modelo rpu6.3, es 0.71.» (Brajín 6.4)

**Lo que hace comparables los dos R² es que el denominador no lleva
parámetros.** `w_t` es propiedad de los DATOS una vez fijados λ, d y D: es
idéntico en las dos estimaciones y sólo se mueve el residuo. Sacarlo en cambio
de la `W` del cast —que resta la parte determinista, y por tanto depende de los
parámetros estimados— es el error que `drtran-python` ya documenta: el R²
**bajaba** al añadir la transferencia mientras la desviación típica residual
bajaba también. Dos cifras del mismo ajuste apuntando en sentidos opuestos es
la señal de que el denominador se movió.

Deja de ser comparable entre **d distintas** — ahí `w_t` es otra variable. Por
eso se presenta como una **transición entre dos ajustes de una
especificación**, nunca como nota con la que ordenar modelos.

Así que la pestaña da **las tres**, por ecuación:

| | diagonal | con transferencia |
|---|---|---|
| desviación típica residual | `d.t.` | `d.t.` |
| R² (A.28) | `R²` | `R²` |

más la **reducción de varianza residual** en %, que es como la escuela lo dice
en voz alta («una reducción del 44 % en relación a su modelo univariante»), y
encima de la tabla el **LR** contra el diagonal, que es el que dice si esa
mejora se gana su sitio:

    LR = 2 ( logL_completo − logL_diagonal )  ~  χ²(k)

> Hace falta que la página **produzca el diagonal**, no sólo que lo recuerde:
> un botón `Calcular baseline` que ponga el modo, lance, guarde el `logL` **y
> los residuos** —las otras dos cifras salen de los `a_t`, no de la
> verosimilitud— y deje el modo como estaba. Ver §5.3.

#### El modo diagonal era inalcanzable desde la interfaz

Al verificarlo aparecieron dos cosas, y las dos impedían el baseline:

1. **Bug del motor.** `-0` fijaba `s_ord[j] = -1` sobre la red estrella por
   defecto, y **el `.dag` se leía después** y reponía todos los enlaces. Con
   `-n`, `-0` se ignoraba *en silencio*: misma verosimilitud, mismos residuos,
   LR = 0. Ahora `-0` gana —es un **modo**, no una opción— y lo anuncia:
   `Network file X IGNORED: -0 fits the diagonal model.`

2. **El diagonal no lleva `.cns`.** Un `.cns` que ata `omega1[0]` ni siquiera
   nombra slots que existan en un modelo sin enlaces. El baseline es: las
   series con sus univariantes del `.pre` y nada más — que es exactamente el
   ajuste cuya verosimilitud debe coincidir con la suma de las de fue, la
   puerta que certifica el puente (`mcp_server.py`: `build_cast_spec(specs,
   links=[])`).

Comprobado sobre el m6: el R² sube **donde están las transferencias** y se
queda quieto donde no las hay.

| | R² diagonal | R² con transferencia |
|---|---|---|
| EP | 0.7830 | **0.8610** |
| EI | 0.7707 | **0.8211** |
| EU | 0.7784 | 0.7743 |
| EC | 0.7633 | 0.7593 |
| EA | 0.5969 | 0.5844 |
| P | 0.3778 | 0.3778 |

(EP y EI son las dos que reciben entradas.)

### 2.3 Los gráficos necesitan los residuos **como números**

`lib/fugplot` ya sabe dibujar la batería de fue —serie, ACF/PACF, histograma,
media-desviación— y `lib/ccfplot` las cruzadas. **Todas toman un
`struct Tseries`**, así que lo único que falta son los valores.

Y están en el `.out`… **como columna derecha del gráfico ASCII**:

```
   1  1/   1 | :           :           |       *   :           : | 20.6490603115
```

**Sacarlos de ahí sería construir un gráfico a partir de otro gráfico.** Es
frágil —anchura fija, formato— y absurdo teniendo el motor los números.

> **Recomendación: que drtran los escriba.** Una opción que vuelque los
> residuos de cada ecuación a un fichero, y mtram los lee y los dibuja con la
> batería que ya existe. Es un añadido pequeño al motor y deja de depender de
> un formato de pantalla.

Mientras no esté, la pestaña de residuos da **los números** —que sí están
etiquetados en el `.out`— y los gráficos quedan pendientes.

---

## 3. La página

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Releer │ ◀ EP ▶ Gráficos… │ Exportar… │    Ir a…   Calcular baseline │
├───────────────────────────────────────────────────────────────────────────┤
│ Exogeneidad │ Adecuación │ Ajuste │ Residuos │ Modelo estimado │ Salida    │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│                    (la libreta se queda TODO el alto)                     │
│                                                                           │
├───────────────────────────────────────────────────────────────────────────┤
│ ● Hosking P(288) = 275.9, p = 0.6854 · los residuos son blancos           │
│ ● 2 enlaces sin estructura suficiente: EC, EU  → Identificación           │
└───────────────────────────────────────────────────────────────────────────┘
```

Una libreta, seis pestañas **en el orden de las preguntas**, y las dos líneas
de veredicto abajo, como en todas las páginas.

*(El usuario sugería el veredicto entre los tests y la salida. Lo pongo abajo
por consistencia —en las seis páginas está abajo— y porque la salida completa
es una pestaña más, así que no hay un «entre». Si se prefiere arriba, es un
cambio de dos líneas.)*

### `◀ EP ▶` — moverse por el grafo

La diagnosis es **por ecuación**, y las ecuaciones forman un grafo. Se empieza
en la salida y se navega **aguas arriba**, que es como se construye el modelo:

```
EP ◀── EI ◀── EU ◀── EC
```

No es una lista alfabética: el orden es el topológico, el mismo que el motor
usa para construir. Así la diagnosis se recorre en el orden en que el modelo
existe.

### Las seis pestañas

**1 · Exogeneidad** — por enlace: `Q(df)`, `p`, cuántos retardos negativos
significativos, y el veredicto. Y arriba, la frase que contesta la primera
pregunta:

> *Los cuatro enlaces pasan la exogeneidad: el modelo de transferencia se
> sostiene.* / *EC no es exógena (p = 0.03): esto no lo arregla (b,r,s) — el
> escalón que toca es drvarma.*

**2 · Adecuación** — por enlace: el contraste de k ≥ 0, y si falla, **dónde**
está el pico de la CCF residual, que es lo que dice cómo cambiar `(b, r, s)`.

**3 · Ajuste** — el LR contra el baseline diagonal, con sus grados de libertad
y su p; y por ecuación, la desviación típica residual y su cambio. Sin
baseline, dice qué hacer para tenerlo.

**4 · Residuos** — por ecuación: media, d.t., asimetría, curtosis,
**Jarque-Bera**, el Ljung-Box, y el histograma **observado contra esperado**
que el motor ya calcula. Los gráficos, desde `Gráficos…`.

> **Si se dan la asimetría y la curtosis, hay que dar el Jarque-Bera.** Es
> exactamente la función de esas dos —`n/6 (S² + K²/4)`— y es el que dice si
> apartarse de cero significa algo. Dejarlo fuera obliga al analista a hacer la
> cuenta a ojo teniendo los dos números delante.

**5 · Modelo estimado** — todos los parámetros con su **d.t. debajo**, en el
formato de la escuela:

```
   ω1(B) =   0.764711  −  0.295418 B
            (0.270770)    (atado)

   θ_EP(B) = 1 − 0.386313 B          ← DEL .pre, no se estimó (-X)
                 (0.106337)
```

Y lo que el usuario pidió: **si el ruido se mantuvo fijo (`-N`/`-X`/`-D`/`-E`),
no se presenta como estimado.** Se marca de dónde viene. Presentar como
resultado algo que no se estimó es la peor clase de mentira de una pantalla.

**6 · Salida** — el `.out` entero.

### `Gráficos…` — menú emergente

Sobre la ecuación en la que se esté:

```
   Serie y ACF / PACF
   Histograma
   ─────────────────────────
   CCF de los residuos con EI
   CCF de los residuos con EC
```

**No va el gráfico media – desviación típica.** Ése existe para decidir la
**transformación** de una serie: si la dispersión crece con el nivel, hay que
tomar logaritmos. Un residuo no tiene nivel con el que crecer —su media es cero
por construcción— así que el gráfico no puede decir nada. fue tampoco lo dibuja
sobre sus residuos.

Cada uno escribe su EPS con `lib/fugplot` / `lib/ccfplot` y lo abre en el
visor — el mismo fichero que iría al papel. **Depende de §2.3.**

---

## 4. Qué se puede hacer ya y qué no

| | estado |
|---|---|
| Exogeneidad, Adecuación | **hecho**: `lib/outdiag` lo lee |
| Residuos (números) | **hecho** |
| Salida | **hecho** |
| Ajuste (LR contra diagonal) | **hecho**, guardando el `logL` del baseline |
| Modelo estimado con d.t. | **hecho**: `od_params()` lee la tabla del `.out` |
| Gráficos | **hecho** — §2.3 se aprobó y el motor los escribe |

---

## 5. Lo que se hizo, y en qué se apartó del boceto

**Estado: implementado** (`gui/drtran/src/diagnosis.c`).

### 5.1 §2.3 se aprobó: el motor escribe los residuos

`drtran -e FICHERO` vuelca una columna por ecuación, con su fecha:

```
# drtran residuals: one column per equation
# n 64   m 6   freq 4
# obs date EP EI EU EC EA P
1 1/1977 22.1704193554 -10.3360838442 ...
```

Lo lee `od_residuos()`. mtram pone el `-e` **siempre** en la orden: es un
fichero pequeño y quitarle al analista un interruptor que nunca querría
apagado es lo correcto.

### 5.2 Los gráficos salen de `lib/fugplot`, la misma batería de fue

Para poder enlazarla hizo falta lo mínimo, y nada de ello es cálculo nuevo:

- `gui/drtran/include/plothost.h` — el contrato que fugplot pide al programa
  anfitrión. Casi todo ya estaba en `diagnose.c` del motor: `Acf`, `Pacf`,
  `ChiTest`, `Skew`, `Kurt`, `Stdev`, `Mean`. **La única costura es `Acf`**,
  que en drtran recibe `(data, nobs, lags, corr, mean, var)` y en fue
  `(ser, lags, corr)`: el cálculo es idéntico y las dos tienen que convivir en
  el mismo binario, así que la de fugplot se renombra en el shim.
- `plotsupport.c` pasó de `engines/fue/src/` a **`lib/fugplot/`**, que es donde
  dice su propia cabecera que vive: son las funciones que fugplot necesita, no
  las de fue. fue sigue pasando sus 109 pruebas.
- `save_eps()` de fugplot pone ahora el prefijo en el **nombre**, no delante de
  la ruta: con un `x11out` como `/tmp/mtram/res`, `hist_` delante daba
  `hist_/tmp/…`. Con un nombre a secas —como lo usan fue y fug— no cambia nada.

La CCF de los residuos **no es la de Identificación**: allí se preblanquea con
el modelo univariante para decidir `(b, r, s)`; aquí las dos series ya son
residuos —ya están blancas si el modelo vale— y lo que se mira es si quedó
algo que la transferencia no cogió. Se calcula con `Ccf()` del motor, en los
dos sentidos, y se dibuja con `lib/ccfplot`.

### 5.3 `Comparar…` no existe: la comparación **es** la pestaña Ajuste

El boceto ponía dos botones. Al escribirlo quedó claro que el segundo sobra:
en cuanto hay baseline, la pestaña 3 **ya** enseña las tres cifras y el LR. Un
diálogo aparte sería el mismo número escondido tras un clic.

Y el que queda no *fija*, **calcula**. `Fijar baseline` solo guardaba lo que
hubiera en pantalla, y conseguir que en pantalla hubiera un diagonal costaba
**ocho pasos**: ir a Estimación, marcar «Diagonal», estimar, volver, fijar, ir
otra vez, desmarcar, estimar. Ningún sitio decía que hubiera que hacer ese
viaje.

**`Calcular baseline`** lo hace entero: pone el modo diagonal —visiblemente, la
casilla de Estimación cambia delante del analista—, lanza, y al acabar se fija
solo y **deja el modo como estaba**. Lo que se pidió fue «dame el baseline», no
«pon el modo diagonal»: dejárselo puesto sería dejar una trampa armada.

Si el LR sale negativo tampoco se disimula: o el baseline no es el diagonal de
este sistema, o uno de los dos no convergió.

### 5.4 El veredicto no dice a dónde volver: **lleva**

La segunda línea elige el problema más grave —primero exogeneidad, luego
adecuación— lo nombra con sus enlaces, y aparece a su lado un botón
`Ir a Red` / `Ir a Identificación` que cambia de página. El veredicto deja de
ser una frase y pasa a ser el paso siguiente.

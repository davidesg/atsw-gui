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

### 2.2 R² no; **LR contra el modelo diagonal**

El motor no da R², y hacer uno sería un error:

- sobre series **diferenciadas** el R² es engañoso — mide contra una media que
  no significa nada;
- y no contesta la pregunta, que no es *«cuánta varianza explico»* sino
  ***«¿aportan algo las transferencias?»***.

Eso tiene respuesta exacta y el método ya la tiene definida. **El modelo
diagonal (`-0`) es el baseline**: las mismas series, el mismo ruido, las mismas
covarianzas, **menos las transferencias**. Y está **anidado** —el diagonal es
el modelo completo con todos los ω = 0— así que

    LR = 2 ( logL_completo − logL_diagonal )  ~  χ²(k)

con `k` = número de parámetros de transferencia libres. Es el contraste
correcto, es exacto, y no hay que inventar nada.

> Lo que hace falta es que mtram **recuerde el diagonal**, igual que Previsión
> recuerda una evaluación para comparar. Un botón `Fijar baseline` tras correr
> con `Diagonal (-0)` marcado.

Y como medida descriptiva, lo honesto es la **desviación típica residual por
ecuación**, comparada con la del baseline. Eso sí se interpreta.

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
│ Releer │  ◀  EP  ▶  │ Gráficos… │            Fijar baseline   Comparar…   │
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

**4 · Residuos** — por ecuación: media, d.t., asimetría, curtosis, el
Ljung-Box, y el histograma **observado contra esperado** que el motor ya
calcula. Los gráficos, desde `Gráficos…`.

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
   Serie de residuos
   ACF y PACF
   Histograma
   Media – desviación típica
   ─────────────────────────
   CCF con EI
   CCF con EC
```

Cada uno escribe su EPS con `lib/fugplot` / `lib/ccfplot` y lo abre en el
visor — el mismo fichero que iría al papel. **Depende de §2.3.**

---

## 4. Qué se puede hacer ya y qué no

| | estado |
|---|---|
| Exogeneidad, Adecuación | **ya**: `lib/outdiag` lo lee |
| Residuos (números) | **ya** |
| Salida | **ya** |
| Ajuste (LR contra diagonal) | **ya**, guardando el `logL` del baseline |
| Modelo estimado con d.t. | **ya**: la tabla del `.out` está etiquetada |
| Gráficos | **necesita** que el motor escriba los residuos (§2.3) |

---

**Estado: propuesto.** A la espera de revisión, y en particular de la decisión
sobre §2.3.

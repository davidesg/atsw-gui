# El módulo de diagnosis

> «Me interesa un módulo de resumen de la diagnosis. El `.out` tiene casi toda
> la diagnosis disponible. Necesitamos una salida que realice diagnosis
> estándar. Lo más básico es diagnosis de los residuos: media 0, estadístico
> para ausencia de autocorrelación y normalidad. A esto se añade situación de
> estimación y sobreparametrización.»

---

## 1. Dos módulos, porque **leer no es juzgar**

| | qué hace | dónde |
|---|---|---|
| `lib/outfile` | **lee** el `.out` de fue y devuelve hechos | ya existe, le faltan campos |
| `lib/dictamen` | convierte hechos en **veredictos** | nuevo |

No es una separación decorativa. Los umbrales —α, `|t| > 2`, `|r| ≥ 0.7`, el
|z| a partir del cual un residuo es noticia— **son método, no formato**. Si
viven pegados al parser, están en el sitio donde nadie los busca y no se pueden
probar sin un fichero delante. En un módulo aparte tienen nombre, se prueban
con números inventados, y el día que el analista quiera mover uno hay **un
sitio** donde moverlo.

Y al revés: si mañana el motor cambia una línea del informe, se arregla el
lector y los veredictos ni se enteran.

---

## 2. Los hechos que le faltan a `lib/outfile`

Todos están en el `.out`; hoy no se leen.

| hecho | dónde está | para qué |
|---|---|---|
| media de los residuos y **su error típico** | `Mean:` / `Standard error of mean:` | media 0 |
| si el modelo estima **μ** | `Mean parameter (mu):` | *ver §4.1* |
| la **escalera** completa del Ljung-Box | columna `L-B Q  DF` de la ACF | *ver §4.2* |
| el histograma: % observado contra esperado | `fuera de ±1, ±2` | normalidad |
| los **pares de parámetros** con \|r\| ≥ 0.7 | «Correlations greater than or equal to 0.7» | sobreparametrización |
| la tabla de parámetros: valor y error típico | `-0.001610 (0.000683) [ 1]` | `|t|` |
| los residuos extremos | `| 124  5/2012  -2.15 |` | atípicos |
| cómo acabó el optimizador | `convergence_of()` | **ya lo lee** |

---

## 3. La forma del dictamen

```c
typedef enum { DX_NO_CONSTA, DX_CUADRA, DX_MIRAR, DX_NO } DxEstado;

typedef struct {
   DxEstado estado;
   char     titulo[32];     /* "Autocorrelación"                        */
   char     dato[96];       /* "Q(39) = 117.74, 39 g.l., p = .000"      */
   char     dice[160];      /* "Los residuos NO son blancos."           */
} DxLinea;
```

**Cuatro estados y no dos**, y el cuarto es el que importa:

- `DX_NO_CONSTA` — **el `.out` no trae ese bloque.** No es aprobado: es que no
  consta. Es la regla de la huella vacía del guion, otra vez: *no consta* nunca
  significa *cuadra*.
- `DX_CUADRA` — no hay nada que objetar.
- `DX_MIRAR` — está en el filo. Un p de .04 y uno de .000 no son la misma
  noticia y no pueden salir iguales.
- `DX_NO` — lo rechaza.

Y **tres campos separados**: el dato es del motor, la frase es del front end.
El GUI la dirá en castellano y un informe en inglés si hace falta, sin que el
dictamen sepa de idiomas.

---

## 4. Los cinco bloques, y las dos trampas

### 4.1 Media — y la trampa

`t = media / e.t.` Trivial… salvo que **si el modelo estima μ, la media de los
residuos es cero por construcción** y el contraste no dice nada. Hoy el `.out`
de un modelo con μ da `Mean: -0.000001` con `t ≈ -0.006`, y presentarlo como
un aprobado brillante es presentar una tautología como evidencia.

Con μ estimado, el bloque dice **`DX_NO_CONSTA`: «el modelo estima la media, así
que ésta es cero por construcción»**. Sin μ, el contraste vale y se hace.

### 4.2 Autocorrelación — y por qué la escalera entera

El motor da el Ljung-Box **escalonado**: en 12, 24, 36 y el último retardo.
`lib/outfile` hoy se queda con el último.

Eso pierde información de verdad: *Q(12) significativo y Q(36) no* es un
problema **cerca**, casi siempre estacional o de forma del modelo; *Q(12) bien
y Q(36) mal* es arrastre lejano. Son dos diagnósticos distintos y hoy salen
iguales. El dictamen enseña la escalera y juzga por **el peor**.

### 4.3 Normalidad

Jarque-Bera con sus dos grados de libertad —el `.out` da el estadístico y el
p sale de `chisq_cola`, que ya está— más la asimetría, la curtosis y el
histograma que el motor ya calcula: **% observado fuera de ±1 y ±2 contra el
esperado**. Ese último es el contraste más barato que hay y está impreso.

### 4.4 Estimación

`convergence_of()` ya distingue **converger de rendirse**, y distingue las dos
formas de rendirse. Con una nota que ya está escrita en `lib/verdict` y vale
igual aquí: *«parado en un punto sin mejora»* suena a fracaso y es **lo que
sale cuando el `.pre` ya era el óptimo** — que es justamente el invariante del
contrato. Confundirlo con un fallo sería confundir el éxito con el fracaso.

### 4.5 Sobreparametrización — y la segunda trampa

Dos cosas, las dos leídas:

- **pares con \|r\| ≥ 0.7**: el motor ya los lista, calculados por él;
- **parámetros con \|t\| < 2**: de la tabla de parámetros.

Y la trampa: una correlación alta **entre un AR y un MA puede ser
estructural** —una función de transferencia racional, un AR(2) con φ₂ < 0— no
un exceso de parámetros. Por eso el dictamen dice *«φ₂ y θ₁ correlacionados
0.93»* y **no** dice «sobra un parámetro». Enunciar el hecho es del módulo;
decidir si sobra es del analista.

---

## 5. Lo que el módulo **no** hace

**No dice qué hacer.** La escalera de alternativas —qué quitar, qué intervenir,
qué reformular— es una **decisión**, y las decisiones son del analista (o del
LLM en la otra encarnación). El dictamen llega hasta *«los residuos no son
blancos, y el peor es Q(12)»* y se para ahí.

Es la misma línea que ya separa el manifiesto —que guarda decisiones— de los
`.out` —que guardan resultados.

---

## 6. Dónde sale

1. **Una ventana**, desde el menú del modelo: «Diagnosis…». Cinco bloques, uno
   por línea, cada uno con su estado a la vista. Es el hermano pequeño de la
   página de diagnosis de `drtran_gui`, que ya tiene seis pestañas para el caso
   multivariante — misma forma, menos cosas.
2. **Una línea en la rejilla.** El peor de los cinco estados, en el globo de la
   fila o como columna si el uso lo pide. La rejilla ya dice `Q` y `p`; el
   dictamen añade *qué significan juntos*.
3. **Exportable con `lib/tabla`** — CSV, TXT y TeX, que es lo que hace que una
   diagnosis acabe en un papel sin volver a teclearla.

---

## 7. Cómo se prueba

Las dos mitades, por separado, que es la ventaja de haberlas separado:

- **el lector**, contra `.out` de verdad de `engines/fue/tests/golden/`, como
  ya hacen `lib/outfile` y `lib/outdiag`. Si el motor cambia una línea, cambia
  el golden y la prueba se entera el mismo día.
- **los veredictos**, con números inventados y sin fichero: un p de .04 da
  `MIRAR`, uno de .000 da `NO`, un bloque ausente da `NO_CONSTA` y **no**
  `CUADRA`, y un modelo con μ no presenta el contraste de la media.

---

## 8. Orden de trabajo

| | qué | coste |
|---|---|---|
| 1 | los hechos que faltan en `lib/outfile`, con su prueba contra golden | 1 día |
| 2 | `lib/dictamen`: los cinco bloques y los cuatro estados, con su prueba | 1 día |
| 3 | la ventana «Diagnosis…» en la madre | 1 día |
| 4 | el resumen en la rejilla y el exportar con `lib/tabla` | medio día |

Nada de esto toca los motores: los ocho hechos de §2 están ya escritos en el
`.out`, y cinco de ellos los calcula el propio motor.

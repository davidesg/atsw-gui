# La previsión: dónde vive y de quién es

> «fue genera un `.inp` para fuf y fuf genera previsiones. Dónde se guardan
> estos archivos de previsiones, no tenemos linaje para las previsiones.»

---

## 1. Lo que había, y la mentira que traía dentro

```
fue <modelo> -f          escribe forecast_<modelo>.inp
fuf forecast_<modelo>    escribe .out, .pdf, .tex y
                         forecast_<modelo>_prev.1.2022.tex
                         prevforecast_<modelo>.12022.eps
```

**El origen de la previsión estaba en el nombre del fichero.** Era el único
rastro de linaje que tenía, escrito justo donde este proyecto dice que no se
escribe nada: *si hay que parsear el nombre para saberlo, no está registrado.*

Y los ficheros iban «al área de trabajo», que no es el sitio de nadie: un
directorio elegido en un selector, sin relación con el modelo que los produjo.

---

## 2. Una previsión NO es un nodo del linaje

Y no es una laguna: es una **diferencia de categoría**.

El manifiesto registra **decisiones del analista**. Un modelo es una decisión;
los datos son la raíz de la que salen. Una previsión no es ninguna de las dos:
es una **salida**, determinada del todo por (modelo, origen, horizonte). Corre
`fuf` otra vez sobre el mismo `.pre` y los números no se mueven — el mismo
invariante que hace del `.pre` un óptimo.

Darle un nodo metería una salida en un registro de decisiones, y `pr_camino`
pasaría por cosas que nadie eligió.

### 2.1 Pero hay un hecho que NO es regenerable

*Lo que dijiste, cuándo lo dijiste, antes de saberlo.* En cuanto se extiende la
muestra y se reestima, la previsión de enero no se reproduce nunca más. Eso no
es una salida: es un **acta**, y perderla es perder lo único que juzga a un
modelo.

Son dos objetos distintos, y conviene no confundirlos:

| | qué es | dónde va |
|---|---|---|
| **la corrida** | salida regenerable | al lado de su modelo, sin nodo |
| **el acta** | una afirmación hecha en una fecha | un apunte del modelo, no un nodo |

**El acta no está construida, y es deliberado.** Sólo se gana cuando alguien
prevé de verdad todos los meses. Hoy la evaluación ex-post sobre una submuestra
declarada —`docs/DISENO-muestras.md`— cubre la necesidad real: la ventana está
declarada, el modelo cuelga de ella, y los valores realizados están en el
`datos.csv`.

---

## 3. Dónde se guardan: donde está lo que las produjo

Los dos motores corren **en el directorio del modelo**:

```
IPC_DE/[muestra/]work/
    IPC_DE_m02.inp  .pre  .out          el modelo
    forecast_IPC_DE_m02.inp  .out  .pdf  su previsión
    prevforecast_IPC_DE_m02.12021.eps
```

No hace falta moverlos ni inventarles un sitio: basta con correr los motores
donde vive el modelo. Eso es toda la respuesta a «dónde se guardan».

**El nombre del EPS sigue llevando el origen, y no se toca.** Es la convención
del motor y los nombres de sus ficheros son su interfaz — no se cambian sin
aprobación. Lo que sí cambia es que **nadie lo parsea**: el gráfico se
*encuentra* en el directorio, y el origen se lee del `.out`, que lo dice en
claro:

```
FORECAST REPORT:
VARIABLE NAME: CPI_USA
FORECAST ORIGIN : 12/2021
LEAD TIME FOR FORECASTING: 24
```

---

## 4. Un GUI por motor, y la madre orquesta

`fuf` era una **pestaña de fue_gui**. Ahí estaba estrecha —una previsión tiene
origen, horizonte, bandas, tabla y gráfico— y, sobre todo, fuf **es otro
motor**. El taller tiene un GUI por motor:

| motor | ventana | qué hace |
|---|---|---|
| fug | `gtk_fmg` | identificar |
| fue | `fue_gui` | estimar |
| **fuf** | **`fuf_gui`** | **prever** |
| drtran | `drtran_gui` | transferencias |

Y **la decisión no se va con la ventana**: *qué* prever lo dice la madre,
porque es ella la que sabe cuál es el modelo elegido de cada serie y qué
ventanas hay declaradas. `fuf_gui` prevé lo que le mandan, igual que `fue_gui`
estima lo que le mandan.

### 4.1 Se manda el `.inp`, y se exige el `.pre`

Parece al revés y no lo es. El ciclo empieza con `fue <modelo> -f`, y **fue lee
el `.inp`**: da el mismo óptimo —eso es justo lo que el `.pre` afirma— y de ahí
escribe el `forecast_<modelo>.inp` con los parámetros estimados.

Pero se **exige que el `.pre` exista**, porque es la prueba de que el modelo se
estimó alguna vez. Prever con un `.inp` cuyas semillas nadie ha ajustado sería
prever con un modelo inventado, y saldría sin avisar.

### 4.2 El horizonte se escribe en el fichero

`fue -f` genera el `.inp` de la previsión con un horizonte dentro, y fuf lo lee
**de ahí**. Cambiarlo por la línea de órdenes sería pisar por comando la
especificación que el fichero declara — la regla que ya costó la opción `-B` de
fug. Así que `fuf_gui` **edita esa línea del fichero** y después corre el motor
sin decirle nada más.

### 4.3 Un solo lector de previsiones

`lib/outfcst` leía las de drtran. Ahora lee también las de fuf, y la única
diferencia era el rótulo del horizonte:

```
drtran:  FORECAST ORIGIN : 12/2021   LEAD TIME: 24
fuf:     LEAD TIME FOR FORECASTING: 24
```

Se lee por los dos puntos, no por la frase entera. La etiqueta es del motor y
no vamos a pedirle que la cambie; lo que no puede es obligarnos a tener dos
lectores de lo mismo. Es el mismo reparto que `lib/outfile` (fue) y
`lib/outdiag` (drtran), pero al revés: allí eran dos formatos de verdad, aquí
es uno.

---

## 5. Lo que queda

- **El acta**, cuando el uso la pida (§2.1).
- **La evaluación ex-post automática**: prever desde una submuestra y comparar
  contra el `datos.csv`. Todas las piezas están; falta juntarlas.
- **Las bandas son TEÓRICAS** y `drtran_gui` ya lo dice en su pantalla.
  `fuf_gui` lo dice en el globo de la tabla; cuando exista la evaluación
  ex-post habrá con qué contrastarlas.

# `datos` — la puerta de los datos. Una, no cuatro.

El inventario encontró **cuatro lectores del mismo formato** —columnas de
números separadas por blancos— y que **discrepan**:

| | dónde | un fichero de 2 columnas |
|---|---|---|
| fug | `gui/fug/src/data_load.c:117` | **aplana** todo en un vector |
| fue | `gui/fue/src/data_handling.c:11` | toma la **columna 1** |
| drvarma | `engines/drvarma/gui/drvarma_gui.c:198` | multivariante, con `setlocale` |
| drvarma viejo | `engines/drvarma/old/…:148` | la copia anterior, sin `setlocale` |

**El mismo fichero cargado en fue y en fug daba dos series distintas.**

## No es el caso de `fugdraw`

Aquellas cuatro copias eran **idénticas** y se factorizaron sin riesgo. Éstas
discrepan, así que unificarlas **obliga a elegir** — y elegir es una decisión
del método, no de ingeniería. Las decisiones, con su razón:

| | decisión | por qué |
|---|---|---|
| columnas | **la 1 es la serie**; las demás, regresores | aplanar dos columnas fabrica una serie que no existe |
| cabecera | se acepta **y se usa** para nombrar | fug ya la saltaba; usarla es gratis y quita un tecleo |
| separador | espacio, TAB, `;` **y coma** | el filtro de fug anunciaba `*.csv` y un CSV de verdad fallaba |
| decimal | coma **si el separador no es la coma** | es la regla europea, y es decidible |
| frecuencia | **del fichero si está**; del diálogo si no | ver abajo |

## La frecuencia y la fecha tienen que viajar con la serie

Es el requisito 2 del inventario y el que más daño hace hoy: **no viajan**.
Salen de un combo de la ventana, con defaults distintos por programa — fug
`freq=1`; fue y drvarma `freq=12` y año 2000. Una serie mal fechada al cargarla
envenena todo lo que venga después y **no hay nada que lo detecte**.

Se leen de dos sitios, los dos convenio de la casa:

```
# freq 4              la cabecera de '#', la que escribe "drtran -e"
# start 1/1977
```
```
fecha,IPC             una columna de fechas, que es lo que trae un CSV real
1/1977,20.6
```

Con la columna de fechas, la **frecuencia se deduce del salto de año**: si tras
`4/1977` viene `1/1978`, la frecuencia es 4.

> **Y si el fichero no lo dice, sale 0** — no un default. Que el que llama
> tenga que ponerlo es el punto: así sabe que lo está poniendo él.

## Lo que refusa, y está bien que refuse

Una tabla de **resultados** no es una serie. `recursive_compare.csv` tiene una
columna de texto repetida en cada fila (el nombre de la serie): eso es formato
largo y habría que pivotarlo. Se rechaza, diciendo exactamente dónde:

```
Línea 2, campo 1: «IPC_ES» no es un número.
```

## El error es un hecho; cada idioma lo redacta

Es la lección que costó una prueba de la batería en `lib/netfile`: la
biblioteca devuelve **qué pasó** —código, línea, campo, el texto que falló— y
cada front end lo dice en su idioma. El motor habla inglés porque **es una
propiedad declarada del puerto** y hay pruebas que lo comprueban verbatim; el
GUI, castellano.

```c
dt_error_es( &e, b, n );   /* Línea 2, campo 2: «hola» no es un número.  */
dt_error_en( &e, b, n );   /* line 2, field 2: hola is not a number      */
```

## Quién la usa

fue, fug, y la madre al dar de alta una serie.

**`gui/drtran` no**, y es deliberado (`src/main.c:9-11`): no lee datos crudos
porque un CSV ahí se saltaría el escalón univariante. Es la única puerta que
está cerrada a propósito, y sigue cerrada.

## Pendiente

**Adoptarla en fue y en fug.** Es un cambio de comportamiento en dos programas
que funcionan y tienen bancos de pruebas —el de fue son 109 corridas contra
ficheros de oro—, así que va en su propio paso y no de propina.

Sin dependencias, como `lib/rutas` y `lib/tabla`.

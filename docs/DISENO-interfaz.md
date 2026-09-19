# DISEÑO — la interfaz de mtram, pantalla por pantalla

Las siete pantallas existen y hacen lo que tienen que hacer
(`DISENO-mtram.md`). Este documento es lo otro: **cómo se presentan**, y cómo
se llaman los botones.

Se escribe página a página. Empieza por **Series**.

---

## 0. El fallo que lo motiva, medido

La página Series tiene hoy tres cosas apiladas en vertical: la lista, el marco
«Ventana muestral común» y el marco «Operadores no estacionarios». Los dos
marcos de abajo **crecen con los datos**:

| | cuánto ocupa | con 6 series |
|---|---|---|
| Ventana común | `n + 1` renglones | 7 |
| Operadores | `n(n−1)/2 + 2` renglones — **una pareja por línea** | 17 |
| | **total** | **≈ 26 líneas ≈ 470 px** |

La ventana abre a 620 px de alto. Descontando la barra de botones, la de estado
y los bordes, **a la lista le quedan unos 70 px: dos filas**. Justo cuando hay
seis series es cuando la lista deja de servir.

> No es que los marcos sean grandes. Es que **su altura es O(n) y O(n²) y
> compiten por el mismo espacio vertical que la lista**, que es la que hay que
> poder leer.

---

## 1. Cuatro reglas que salen de ahí

**R1 — Nada cuyo tamaño crezca con los datos comparte el eje vertical con la
lista.** Si crece con `n`, va *dentro* de la lista; si crece con `n²`, no va en
la pantalla: va a un panel que se abre.

**R2 — Un hecho POR SERIE es una columna. Un hecho POR PAREJA no cabe en una
lista de series.** «Cuántas observaciones pierde EI con la ventana común» es de
EI: es una columna. «EI y EU tienen el mismo ∇» no es de ninguna de las dos: es
un veredicto del conjunto, y su detalle se pide.

**R3 — El veredicto siempre visible; el detalle, a un clic.** Una línea fija
que diga *qué pasa*, y el desglose sólo cuando se pregunte. Es la idea del
`Mes[]` de TASTE —una frase por opción, siempre en la fila 25
(`MENTEC.PAS:37-39`)— aplicada al resultado en vez de a la opción.

**R4 — La altura de la ventana se la queda la lista.** Todo lo demás es de
altura fija. Si algo no cabe en su altura fija, se resume y se abre aparte.

---

## 2. Cómo se llaman los botones

La tradición de las interfaces gráficas es que el botón sea **escueto**: una
palabra, dos a lo sumo, y que la explicación viva en el *tooltip*, no en la
chapa. Las reglas que se siguen aquí:

1. **Una palabra.** `Abrir`, `Quitar`, `Salida`, `Estrella`. Si hace falta una
   frase, es que el botón hace dos cosas.
2. **Sin extensiones de fichero en la chapa.** `Abrir…`, no `Abrir .pre…`: el
   diálogo ya dice qué filtra, y el nombre del botón no es el sitio para
   enseñar el formato.
3. **`…` significa que abre algo.** Un diálogo, un panel, un selector. Sin
   puntos suspensivos, el botón actúa en el acto.
4. **El verbo, sólo si hay ambigüedad.** `Salida` basta: está claro que hace
   salida a la marcada. `Hacer salida` es una frase donde cabía una palabra.
5. **Lo destructivo, separado.** `Quitar` no va pegado a `Abrir`.
6. **El porqué va en el tooltip, y es largo si hace falta.** Es donde este
   proyecto pone lo que no se puede perder: *«el orden es el índice de q[i,j]
   en el .cns»* no cabe en una chapa y no puede faltar.

Lo que hay hoy, contra lo que queda:

| hoy | queda | por qué |
|---|---|---|
| `Añadir .pre…` | `Abrir…` | la extensión la dice el diálogo |
| `Subir` / `Bajar` | `↑` / `↓` | con arrastre, son el atajo fino; el icono basta |
| `Hacer salida` | `Salida` | una palabra |
| `Quitar` | `Quitar` | ya estaba bien |
| — | `Ventana…` | nuevo: abre el detalle |
| — | `Operadores…` | nuevo: abre la matriz |

---

## 3. La página Series

### El boceto

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Abrir…  Quitar  │  Salida  ↑  ↓                      Ventana…  Operadores… │
├───┬────────┬─────┬─────────┬─────────┬────────┬───┬───┬───┬──────────────┤
│ # │ Serie  │ Obs │ Desde   │ Hasta   │ Pierde │ d │ D │ f │ Fichero      │
├───┼────────┼─────┼─────────┼─────────┼────────┼───┼───┼───┼──────────────┤
│1 Y│ EP     │  69 │ 03/1976 │ 03/1993 │      0 │ 2 │ 0 │ — │ M6_EP.pre    │
│ 2 │ EI     │  69 │ 03/1976 │ 03/1993 │      0 │ 2 │ 0 │ — │ M6_EI.pre    │
│ 3 │ EU     │  69 │ 03/1976 │ 03/1993 │      0 │ 2 │ 0 │ — │ M6_EU.pre    │
│ 4 │ EC     │  69 │ 03/1976 │ 03/1993 │      0 │ 2 │ 0 │ — │ M6_EC.pre    │
│ 5 │ EA     │  69 │ 03/1976 │ 03/1993 │      0 │ 1 │ 1 │ — │ M6_EA.pre    │
│ 6 │ P      │  69 │ 03/1976 │ 03/1993 │      0 │ 2 │ 0 │ — │ M6_P.pre     │
│   │                                                                        │
│   │              (la lista se queda TODO el alto que sobre)                 │
│   │                                                                        │
├───┴────────┴─────┴─────────┴─────────┴────────┴───┴───┴───┴──────────────┤
│ ● 03/1976 – 03/1993 · 69 obs · todas completas                             │
│ ● 15 pares iguales · cast −V empotrado (verosimilitud exacta)              │
├───────────────────────────────────────────────────────────────────────────┤
│ 6 series. La salida es «EP».                                               │
└───────────────────────────────────────────────────────────────────────────┘
```

Dos líneas de veredicto de altura **fija**, pase lo que pase. Todo lo demás del
alto es de la lista.

### La columna `#`

Es la posición, `1..n`, y **es exactamente el índice al que se refiere
`q[i,j]` en el `.cns`**. Eso no se cuenta en ningún sitio hoy y es la causa de
los dos fallos que aparecieron al implementar el arrastre. Ponerlo como columna
lo hace visible sin decir una palabra.

La salida lleva además una `Y` — la letra con la que aparece en la ecuación y
en el informe del motor.

### Las columnas `d`, `D`, `f` — y por qué NO se leen del fichero

El operador no estacionario se enseñaba como una cadena, `(1-B)(1-B)[f=1][f=2]`.
Se parte en tres columnas cortas:

| columna | qué es | valores típicos |
|---|---|---|
| `d` | cuántas veces ∇ = (1−B) | 0, 1, 2 |
| `D` | cuántas veces ∇ₛ = (1−Bˢ) | 0, 1 |
| `f` | las frecuencias irreducibles que sobran | casi siempre `—` |

**Y aquí está la trampa, que está medida.** `d` y `D` **no** son `nrdiff` y
`nadiff`: la escuela escribe ∇∇₄ como `nrdiff=2` con `ifadf={1,2}`, que es el
**mismo operador** que `nrdiff=1, nadiff=1`. El propio `serie_operador` lo deja
escrito, y por eso `conjunto_compat` compara el polinomio y no los enteros.

En el m6:

| serie | `nrdiff` | `nadiff` | el polinomio | `d` | `D` |
|---|---|---|---|---|---|
| EP, EI, EU, EC, P | 2 | 0 | (1−B)² | 2 | 0 |
| **EA** | **2** | **0** | **(1−B)(1−B⁴)** | **1** | **1** |

Las seis tienen `nrdiff = 2`. Cinco son ∇² y la sexta es ∇∇₄. **Una columna que
leyera `nrdiff` pondría un 2 en las seis y diría que son iguales cuando no lo
son** — y EA es precisamente la que está anidada con las otras cinco.

Así que las tres columnas se calculan **del polinomio** `rnsop`, por división:

    1. dividir por (1−Bˢ) mientras se pueda   -> D
    2. dividir el resto por (1−B) mientras se pueda  -> d
    3. lo que sobre son los factores irreducibles   -> f

Es la forma canónica de la escuela, ∇^d ∇ₛ^D, y tiene la propiedad que hacía
falta: **dos escrituras del mismo operador dan las mismas tres columnas.**

`D` se extrae **al máximo** (decidido): `(1−B)(1−B⁴)²` da `d=1, D=2`.

Vive en `lib/nsop`, con pruebas. La que justifica el módulo no comprueba un
número: comprueba que **las dos escrituras de ∇∇₄ dan lo mismo**, que es lo
único que la columna promete.

Ganancia de lectura, con los datos reales: `EA` con `d=1 D=1` frente a `d=2 D=0`
**salta a la vista**. La cadena de hoy no lo consigue, porque
`(1-B)(1-B)[f=1][f=2]` *empieza igual* que `(1-B)(1-B)` y hay que leerla entera
para ver que no es lo mismo.

Si el paso 3 dejara algo que no se sabe nombrar, la columna `f` pone `?` y el
polinomio entero está en el panel. Un `?` es mejor que un número que miente.

### La columna `Pierde`

Las observaciones que esa serie pierde con la ventana común. Hoy eso está en el
marco de abajo, con un renglón por serie: es **un hecho por serie** y por tanto
una columna (R2). Ahí se lee de un golpe cuál es la que recorta a las demás.

### Las dos líneas de veredicto

Siempre visibles, de una línea, con un punto de color:

```
● verde   todo en orden
● ámbar   hay algo que mirar, pero se puede estimar
● rojo    esto impide estimar
```

| línea | verde | ámbar | rojo |
|---|---|---|---|
| ventana | todas completas | alguna recorta | no hay tramo común |
| operadores | todos iguales | hay anidados (Δ(B), sigue exacto) | incompatibles: cast por resta |

Son **clicables**: abren el mismo panel que los botones `Ventana…` y
`Operadores…`. El botón está para que se vea que se puede; la línea, para que
esté a mano.

### Los dos paneles

**`Ventana…`** — el desglose por serie que hoy ocupa el marco, más el aviso que
no se puede perder:

> el motor **no recorta por fecha**, sólo compara cuántas observaciones hay.
> Dos series de la misma longitud y distinta fecha de inicio se estiman
> desalineadas, en silencio. Está medido: catorce años de desfase mueven la
> verosimilitud 31 unidades y drtran sale con 0.

**`Operadores…`** — un panel emergente (*popover*) anclado al botón. Dos
bloques, y ninguno crece con `n²` en la página porque la página no lo tiene.

Arriba, **el polinomio entero de cada serie**, que es lo que las tres columnas
resumen. Ahí es donde vive la cadena larga que se va de la lista:

```
   EP   (1-B)^2                     d=2  D=0
   EI   (1-B)^2                     d=2  D=0
   EU   (1-B)^2                     d=2  D=0
   EC   (1-B)^2                     d=2  D=0
   EA   (1-B)(1-B^4)                d=1  D=1    [escrito como nrdiff=2, f={1,2}]
   P    (1-B)^2                     d=2  D=0
```

La coletilla entre corchetes sólo aparece cuando el fichero lo escribe de una
forma distinta de la canónica. Es el sitio donde decirlo: en la lista sería
ruido, y callarlo del todo dejaría al analista sin entender por qué su
`nrdiff = 2` sale como `d = 1`.

Abajo, **la matriz de compatibilidad**, que es `n×n` y por eso **nunca** puede
estar en la página (R1). Como matriz y no como lista de parejas: con 6 series
son 15 casillas en una rejilla, no 15 renglones.

```
        EP   EI   EU   EC   EA   P
   EP    ·   =    =    =    ⊃    =
   EI         ·   =    =    ⊃    =
   EU              ·   =    ⊃    =
   EC                   ·   ⊃    =
   EA                        ·   ⊂
   P                              ·

   =  el mismo ∇          cast empotrado, verosimilitud exacta
   ⊂  uno divide al otro  hay Δ(B), sigue siendo exacta
   ✗  incompatibles       el motor despacha al cast por resta
```

Léase por filas: *«EP ⊃ EA»* = el operador de EA contiene al de EP. Los diez
`=` y los cinco `⊃` son los «10 pares iguales» que hoy dice la barra: la matriz
enseña además **cuál** es la distinta de un vistazo, que la lista de parejas no
hace.

### Qué se va de la página

| se va | a dónde |
|---|---|
| el marco «Ventana muestral común» entero | a la línea de veredicto + panel `Ventana…` |
| el marco «Operadores no estacionarios» entero | a la línea de veredicto + panel `Operadores…` |
| la columna `Papel` con las palabras `SALIDA Y` / `entrada` | a la columna `#` |
| la columna `Operador ∇` con la cadena entera | a las tres columnas `d` `D` `f`; la cadena, al panel |

Y entra: la columna `Pierde`.

---

## 4. La página Red

### El mismo mal, y uno nuevo

El marco «¿Se puede estimar?» escribe **de 8 a 12 líneas** según el caso — el
del ciclo es el más largo porque es el que más hay que explicar. No es tan
grave como en Series, pero tiene algo peor: **cambia de alto según lo que
pase**, así que la lista da saltos mientras se edita la red. Un sitio donde se
trabaja no puede moverse bajo la mano.

Y hay algo que hoy **no se ve y hace falta**: cuántos enlaces entran y salen de
cada serie. Eso dice de un golpe quién es salida pura, quién entrada pura y
**quién es las dos cosas** — que es lo único que distingue una RED de una
estrella. Hoy está en prosa, al final del marco.

### La decisión de fondo: una lista o dos

La página tiene dos objetos: los **enlaces** y las **series**. La tentación es
poner dos listas, y es un error: volvería a repartir el alto entre dos cosas
que crecen (R1, R4).

**Manda la lista de enlaces**, porque es lo que se edita aquí. Lo de las series
es un *resumen* —tres números por serie— y por tanto veredicto y panel.

### El boceto

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Nuevo…  Editar…  Quitar  │  Estrella  │  Abrir…  Guardar…    Orden…  Series… │
├─────────┬───┬─────────┬───┬───┬───┬─────┬─────────────────────────────────┤
│ Salida  │ ← │ Entrada │ b │ r │ s │ par │                                 │
├─────────┼───┼─────────┼───┼───┼───┼─────┼─────────────────────────────────┤
│ 1 EP    │ ← │ 2 EI    │ 1 │ 0 │ 1 │  2  │                                 │
│ 1 EP    │ ← │ 4 EC    │ 1 │ 0 │ 2 │  3  │                                 │
│ 2 EI    │ ← │ 3 EU    │ 1 │ 0 │ 3 │  4  │                                 │
│ 3 EU    │ ← │ 4 EC    │ 2 │ 0 │ 1 │  2  │                                 │
│                                                                           │
│                  (la lista se queda TODO el alto que sobre)                │
│                                                                           │
├─────────┴───┴─────────┴───┴───┴───┴─────┴─────────────────────────────────┤
│ ● Acíclica · orden  EC → EU → EI → EP · 4 enlaces, 11 parámetros           │
│ ● 2 series son salida Y entrada: es una RED, no una estrella · 0 sueltas   │
├───────────────────────────────────────────────────────────────────────────┤
│ 6 series. La salida es «EP».                                              │
└───────────────────────────────────────────────────────────────────────────┘
```

Y con un ciclo, la primera línea en rojo:

```
│ ● CICLO:  EU → EI → EP → EU · el sistema es simultáneo — esto es drvarma   │
```

### `b`, `r`, `s` en tres columnas

Es la misma decisión que `d`, `D`, `f` en Series, y por la misma razón: tres
números cortos se comparan de un vistazo entre filas, una cadena no. Además
deja las dos páginas hablando igual.

Aquí **sí** son los números del fichero y no hay forma canónica que buscar: el
`.dag` los escribe tal cual y el motor los usa tal cual.

### La columna `par`

`s + 1 + r`: los parámetros que ese enlace mete en el modelo. Es un hecho por
enlace (R2) y es lo que hace crecer la estimación. Verlo por fila explica de
dónde salen los 11 del total, y cuál es el enlace caro.

### La columna `#` delante del nombre

`1 EP`, `2 EI`. El mismo número que la página Series, que es el de `q[i,j]`. No
se pone el número solo —`1 ← 2` sería ilegible— ni el nombre solo, porque
entonces las dos páginas hablarían idiomas distintos.

### Los dos veredictos

| línea | verde | ámbar | rojo |
|---|---|---|---|
| topología | acíclica, con su orden | — | **ciclo**, nombrado |
| forma | es una red | es una estrella, o hay series sueltas | — |

La segunda línea dice tres cosas en una: cuántas series son salida **y**
entrada (la red de verdad), cuántas están sueltas (el motor las estima pero no
pintan nada), y si la topología es una estrella.

### Los dos paneles

**`Orden…`** — el orden topológico y por qué importa: el motor construye cada
serie por recursión, después de todas las que la alimentan. Cuando hay ciclo,
es aquí donde va la explicación larga que hoy ocupa el marco: qué es un sistema
simultáneo, por qué no se puede triangularizar, y que el escalón que toca es
drvarma. También el consejo: *si crees que el ciclo no debería estar, mira la
CCF del enlace que lo cierra; si sus retardos negativos están dentro de la
banda, ese enlace sobra.*

**`Series…`** — los tres números por serie, que es lo que no se ve hoy:

```
            entra   sale   papel
   1 EP       2      0     salida final
   2 EI       1      1     INTERMEDIA
   3 EU       1      1     INTERMEDIA
   4 EC       0      2     entrada pura
   5 EA       0      0     suelta
   6 P        0      0     suelta
```

Una serie con `entra > 0` **y** `sale > 0` es lo que hace que esto sea una red:
su ecuación se estima **y** alimenta a otra.

### Qué se va de la página

| se va | a dónde |
|---|---|
| el marco «¿Se puede estimar?» entero | dos líneas de veredicto + `Orden…` |
| la columna de notas («cierra el ciclo») | se queda, pero como marca corta |

Y entran: `par`, el `#` delante de los nombres, y el panel `Series…`.

### Una idea que dejo abierta

`lib/fugdraw` dibuja vectores y `lib/preview` los enseña en pantalla. **Se
podría dibujar el grafo** —cajas y flechas, con los `(b,r,s)` en las aristas— y
enseñarlo en el panel `Orden…`, que es donde la topología se mira. Sería el
mismo fichero que iría al papel, como todo lo demás.

No entra en esta pasada. Lo apunto porque un grafo de seis nodos se lee mejor
dibujado que en cualquier tabla, y porque la maquinaria ya está.

---

## 5. Lo que queda por diseñar

`Identificación`, `Modelo`, `Estimación`, `Diagnosis` y `Previsión`, en ese
orden. Las cuatro reglas de §1 y el criterio de nombres de §2 valen para
todas; lo que cambia es qué es el hecho por fila y qué es el veredicto.

Dos ya se sabe que tienen el mismo problema de §0:

- **Modelo** enseña 67 renglones de parámetros y debajo un marco de texto que
  crece con los avisos.
- **Diagnosis** enseña dos filas por enlace más dos por serie, y debajo otro
  marco que crece.

**Estado: Series IMPLEMENTADA** (`lib/nsop`, `gui/drtran/src/main_window.c`).
**Red propuesta**, sin implementar. El resto, sin diseñar.

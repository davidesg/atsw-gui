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

**R5 — Todo lo que va en un panel es de ancho fijo.** Casi todo lo que se
enseña ahí son tablas hechas con espacios; con fuente proporcional se
descuadran y dejan de leerse. Y lo que sea una matriz, rejilla de verdad, que
además se puede colorear. *(Salió de usarlo: §3.)*

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

### Una corrección que salió de usarlo: las tablas van a ancho fijo

La matriz se descuadraba. Era una tabla hecha con **espacios** pintada con una
fuente **proporcional**: las columnas no caían donde debían y la información
dejaba de reconocerse.

Y no era sólo la matriz — el desglose de la ventana, los papeles de cada serie
y el orden de construcción son tablas igual. De ahí una regla más:

**R5 — Todo lo que va en un panel es de ancho fijo.** Es lo que toca: el `.out`
del motor también lo es.

Y la matriz, además, **como rejilla de verdad** y no como texto. Gana dos
cosas: la alineación deja de depender de la fuente, y **se puede colorear** —
con los mismos tres colores de los veredictos, así que la lectura es la misma
en toda la interfaz y la casilla distinta salta a la vista.

De paso gana precisión: el anidamiento ya no es un símbolo neutro. Se compara
el orden de los dos polinomios y se dice **cuál contiene a cuál**:

```
            EP    EI    EU    EC    EA    P
   1 EP     ·     =     =     =     ⊂     =
   2 EI     =     ·     =     =     ⊂     =
   3 EU     =     =     ·     =     ⊂     =
   4 EC     =     =     =     ·     ⊂     =
   5 EA     ⊃     ⊃     ⊃     ⊃     ·     ⊃
   6 P      =     =     =     =     ⊂     ·

   =  el mismo ∇            ⊂  el mío divide al suyo
   ✗  incompatibles         ⊃  el suyo divide al mío
```

La fila de **EA** con seis símbolos apuntando al revés que las demás es
exactamente lo que pasa: su operador contiene al de las otras cinco.

---

## 5. La página Identificación

### El mal aquí es otro: no había lista

El marco de la lectura escribe de 6 a 12 líneas, como en Red. Pero lo grave es
lo otro: **la página no tenía lista**, sólo un combo. Para comparar qué entrada
tiene la CCF más limpia había que ir **una por una**, recordando de memoria lo
que decía la anterior — **y esa comparación es la decisión que se toma aquí**.

### Una fila por entrada CANDIDATA, no por enlace

Identificar es decidir **cuáles** merecen estar en la red, así que se calcula
la CCF preblanqueada de la salida contra **todas** las series cargadas, estén o
no en el `.dag`. Son *n−1* preblanqueos en vez de uno; milisegundos, y cambia
el sentido de la pantalla.

```
┌───────────────────────────────────────────────────────────────────────────┐
│ CCF…   Ecuación…                                    Retardos  [ 0 ]▾      │
├──────────┬───┬───┬───────┬─────┬──────────┬──────────────────────────────┤
│ Entrada  │ b │ s │ k máx │ neg │ P (df)   │                              │
├──────────┼───┼───┼───────┼─────┼──────────┼──────────────────────────────┤
│ 2 EI     │ 1 │ 1 │   2   │  0  │ 138 (96) │ transferencia                │
│ 3 EU     │ 0 │ 3 │   3   │  0  │ 214 (96) │ transferencia                │
│ 4 EC     │ — │ — │   —   │  2  │ 301 (96) │ sin transferencia · OJO       │
│ 5 EA     │ 2 │ 0 │   2   │  0  │  97 (96) │ transferencia, un solo ω     │
│                                                                           │
│                   (la lista se queda TODO el alto)                        │
├──────────┴───┴───┴───────┴─────┴──────────┴──────────────────────────────┤
│ ● EI · b=1  s=1 · pico en k=2 · banda ±0.140 sobre 203 obs estacionarias  │
│ ● Exogeneidad: ningún retardo negativo fuera · EI puede tratarse como     │
│   exógena                                                                 │
└───────────────────────────────────────────────────────────────────────────┘
```

### La columna que manda es `neg`

Los retardos **negativos** fuera de banda son el contraste de exogeneidad, y
son los que deciden si ese enlace **puede existir siquiera**. Va en columna y
no escondido tras una selección, porque es lo que hace que una fila merezca
mirarse.

Y es un diagnóstico que **no se arregla con (b, s)**: si la salida antecede a
la entrada, no hay orden que lo salve — o el enlace sobra, o el escalón es
drvarma. El veredicto lo dice con esas palabras.

### Las demás columnas

| | qué es |
|---|---|
| `b` `s` | lo que propone la CCF: primer y último retardo significativo en k ≥ 0 |
| `k máx` | dónde está el **pico** — no sólo cuánta señal hay, sino dónde |
| `P (df)` | Hosking sobre los preblanqueados |

`b` y `s` en columnas cortas, como `d D f` en Series y `b r s` en Red.

### `Retardos` es un control de verdad

`prewhiten_ccf` recibe `nlags` y hasta ahora se calculaba solo. GraphMaker
dejaba elegir de 8 a 39. `0` significa *los que elige el motor*: `n/4`, con
tope 24 y suelo 10.

Es la diferencia con el criterio de parada de la Estimación (§ `DISENO-mtram`),
que **no** se ofrece porque el motor no lo expone: aquí el parámetro existe de
verdad.

### Dónde va lo demás

**`CCF…`** sigue abriendo la ventana de `lib/preview` — con zoom, guardar e
imprimir, y siendo el mismo fichero que va al papel. Es lo que se mira despacio,
y no cabe dentro de una página que además lleva la lista.

**`Ecuación…`** a panel: crece con el número de enlaces (R1). Enseña la
ecuación con los órdenes que propone la CCF, con los ω a 1 — porque es la
**especificación**, no una estimación, y se enseña antes de estimar justamente
para poder mirarla antes.

---

## 6. La página Modelo

### El problema, y uno que no es de sitio sino de cantidad

El marco de abajo escribe **6 líneas de base**, más una por enlace
casi-colineal, más seis de explicación cuando las hay. Lo de siempre.

Pero aquí hay algo más: **la lista tiene 67 filas** en el m6, y son
heterogéneas —ω de transferencia, φ y θ del ruido, deterministas, medias,
varianzas, covarianzas—. Una lista plana de 67 renglones no se recorre: se
sufre. Y lo que el analista toca de verdad son **seis**.

### Las dos cosas que lo arreglan

**Un árbol por grupos, no una lista plana.** Los grupos ya existen en el código
(`grupo_de`), pero hoy son una *columna*; deberían ser la **estructura**:

```
▾ transferencia (11)
     omega1[0]       libre
     omega1[1]       producto      = omega1[0] * theta_2[B^1]
     omega2[0]       libre
     ...
▸ ARMA del ruido (9)
▸ deterministas (28)
▸ varianzas (5)
▾ covarianzas (15)
     q[3,2]          libre
     q[5,2]          libre
     q[5,4]          libre
     ...
```

Seis grupos plegados caben en la pantalla; se abre el que interesa.

**Y una casilla «sólo lo restringido».** Lo que el `.cns` contiene son los
slots que **dicen algo** — en el m6, **6 de 67**. Es la vista que casi siempre
se quiere, porque es el fichero que se va a escribir.

### El boceto

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Libre  Fijar…  Compartir… │ Abrir…  Guardar… │ ☐ Sólo lo restringido       │
│                                                    Covarianzas…   Avisos… │
├────────────────────────┬──────────────┬──────────────────────────────────┤
│ Parámetro              │ Es           │ Dice                             │
├────────────────────────┼──────────────┼──────────────────────────────────┤
│ ▾ transferencia (11)   │              │                                  │
│      omega1[0]         │ libre        │                                  │
│      omega1[1]         │ producto     │ = omega1[0] * theta_2[B^1]       │
│      omega3[0]         │ comb. lineal │ = omega3[1] + omega3[2] + omega…  │
│ ▸ ARMA del ruido (9)   │              │                                  │
│ ▸ deterministas (28)   │              │                                  │
│ ▸ varianzas (5)        │              │                                  │
│ ▾ covarianzas (15)     │ 3 libres     │                                  │
│      q[3,2]            │ libre        │                                  │
│                                                                           │
├────────────────────────┴──────────────┴──────────────────────────────────┤
│ ● 67 parámetros · 52 libres · 15 fijos o atados                           │
│ ● 3 de 15 covarianzas liberadas · nacen FIJAS en cero                     │
└───────────────────────────────────────────────────────────────────────────┘
```

Y cuando hay casi-colinealidad, la segunda línea pasa a rojo y dice eso, que es
lo que hay que mirar:

```
│ ● OJO — EP ← EI es contemporáneo (b=0) y su covarianza está libre           │
```

### La segunda línea es «lo que hay que mirar»

No es una línea fija de contenido, es una de **prioridad**:

| | cuándo | color |
|---|---|---|
| casi-colinealidad | si la hay | rojo |
| restricciones perdidas al rehacer | si las hay | ámbar |
| estado de las covarianzas | si no hay nada peor | verde |

Es la primera página donde la línea de veredicto tiene que **elegir qué
contar**, y la regla es la evidente: lo que impide o compromete la estimación
va antes que lo que sólo informa.

### El panel `Covarianzas…`: una matriz, y **editable**

Σ es una matriz, y enseñarla como matriz es ver la estructura de covarianzas de
un golpe — igual que los operadores en Series:

```
            EP    EI    EU    EC    EA    P
   1 EP     ·
   2 EI     0     ·
   3 EU     0    libre  ·
   4 EC     0     0     0     ·
   5 EA     0    libre  0    libre  ·
   6 P      0     0     0     0     0     ·
```

Y aquí propongo un paso más que en Operadores: **que se pulse para liberar y
fijar**. Liberar covarianzas es el uso más común del `.cns` —el m6-1 libera
tres de quince— y hacerlo en una matriz es infinitamente más claro que buscar
`q[5,4]` entre 67 renglones.

La matriz de Operadores es de sólo lectura porque el operador viene del `.pre`
y no se decide aquí. Ésta **sí** es una decisión del analista, así que se
decide donde se ve.

### El panel `Avisos…`

La casi-colinealidad con su explicación entera: por qué un enlace
contemporáneo y su covarianza libre explican lo mismo en k = 0, cómo se separan
sólo por el decaimiento en k > 0, y qué pasa cuando los dos AR se parecen —la
cresta plana, ω por las nubes, t-ratios de 2424—. Y la doctrina: **usar una de
las dos, no las dos**.

Hoy eso está en el marco y ocupa seis líneas fijas aunque no haya ningún aviso.

### Qué se va de la página

| se va | a dónde |
|---|---|
| el marco entero | dos líneas + `Covarianzas…` + `Avisos…` |
| la columna `De` | pasa a ser el **grupo** del árbol |
| el párrafo de las covarianzas | a la matriz |

Y entran: el árbol por grupos, la casilla «sólo lo restringido», y la matriz de
Σ editable.

---

## 7. Lo que queda por diseñar

`Estimación`, `Diagnosis` y `Previsión`. Las dos primeras son distintas de
todas las anteriores: no tienen una lista como protagonista sino **una orden
que lanzar y un resultado que leer**, así que puede que las reglas haya que
aplicarlas de otra forma. Las cuatro reglas de §1 y el criterio de nombres de §2 valen para
todas; lo que cambia es qué es el hecho por fila y qué es el veredicto.

**Diagnosis** ya se sabe que tiene el mismo problema de §0: dos filas por
enlace más dos por serie, y debajo un marco que crece con los enlaces que
fallan.

**Estado: Series, Red, Identificación y Modelo IMPLEMENTADAS.** El resto, sin
diseñar.

De Red salió además el sitio donde viven los dos ayudantes que comparten las
páginas: `mtram_verdicto()` —una línea con su punto de color— y
`mtram_popover()`. Están en `gui.h` porque la regla R3 es de todas las páginas,
no de una.

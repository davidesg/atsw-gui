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
│ Abrir…  Quitar  │  Salida  ↑  ↓                        Ventana…  Operadores… │
├───┬────────┬─────┬─────────┬─────────┬────────┬──────────────┬────────────┤
│ # │ Serie  │ Obs │ Desde   │ Hasta   │ Pierde │ Operador ∇   │ Fichero    │
├───┼────────┼─────┼─────────┼─────────┼────────┼──────────────┼────────────┤
│1 Y│ EP     │  69 │ 03/1976 │ 03/1993 │      0 │ (1-B)(1-B)   │ M6_EP.pre  │
│ 2 │ EI     │  69 │ 03/1976 │ 03/1993 │      0 │ (1-B)(1-B)   │ M6_EI.pre  │
│ 3 │ EU     │  69 │ 03/1976 │ 03/1993 │      0 │ (1-B)(1-B)   │ M6_EU.pre  │
│ 4 │ EC     │  69 │ 03/1976 │ 03/1993 │      0 │ (1-B)(1-B)   │ M6_EC.pre  │
│ 5 │ EA     │  69 │ 03/1976 │ 03/1993 │      0 │ (1-B)(1-B)[f]│ M6_EA.pre  │
│ 6 │ P      │  69 │ 03/1976 │ 03/1993 │      0 │ (1-B)(1-B)   │ M6_P.pre   │
│   │                                                                        │
│   │              (la lista se queda TODO el alto que sobre)                 │
│   │                                                                        │
├───┴────────┴─────┴─────────┴─────────┴────────┴──────────────┴────────────┤
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

**`Operadores…`** — la matriz de compatibilidad, que es `n×n` y por tanto
**nunca** debe estar en la página (R1). Como matriz y no como lista de parejas:
con 6 series son 15 casillas en una rejilla, no 15 renglones.

```
        EP   EI   EU   EC   EA   P
   EP    ·   =    =    =    ⊂    =
   EI         ·   =    =    ⊂    =
   EU              ·   =    ⊂    =
   EC                   ·   ⊂    =
   EA                        ·   ⊃
   P                              ·

   =  el mismo ∇          cast empotrado, verosimilitud exacta
   ⊂  uno divide al otro  hay Δ(B), sigue siendo exacta
   ✗  incompatibles       el motor despacha al cast por resta
```

### Qué se va de la página

| se va | a dónde |
|---|---|
| el marco «Ventana muestral común» entero | a la línea de veredicto + panel `Ventana…` |
| el marco «Operadores no estacionarios» entero | a la línea de veredicto + panel `Operadores…` |
| la columna `Papel` con las palabras `SALIDA Y` / `entrada` | a la columna `#` |

Y entra: la columna `Pierde`.

---

## 4. Lo que queda por diseñar

`Red`, `Identificación`, `Modelo`, `Estimación`, `Diagnosis` y `Previsión`, en
ese orden. Las cuatro reglas de §1 y el criterio de nombres de §2 valen para
todas; lo que cambia es qué es el hecho por fila y qué es el veredicto.

Dos ya se sabe que tienen el mismo problema de §0:

- **Modelo** enseña 67 renglones de parámetros y debajo un marco de texto que
  crece con los avisos.
- **Diagnosis** enseña dos filas por enlace más dos por serie, y debajo otro
  marco que crece.

**Estado: propuesto, sin implementar.** A la espera de revisión.

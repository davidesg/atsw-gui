# La serie: qué es, de dónde viene, y hasta dónde llega

Propuesta para revisar. Sale de tres peticiones que llegaron juntas y que
resultaron ser **dos problemas distintos**:

> «En la gestión de Series necesitamos un Editar. Donde por ejemplo se pueda
> cambiar el nombre de la serie. Además necesitamos que la serie guarde
> información sobre su procedencia. Si alguien utiliza mnemotécnico, que pueda
> leer un descriptivo completo de la serie. Quizás también incluir de dónde se
> ha bajado.»

> «Necesitamos una aplicación para truncar las series. Yo quiero truncar la
> muestra hasta 12/2019 y realizar los análisis hasta esta fecha. Aquí no
> podemos cambiar el sample de los modelos estimados porque dañaría el
> linaje.»

Lo primero son **metadatos**: texto sobre la serie, que no cambia ningún
número. Lo segundo **no es un metadato**: la muestra está dentro del `.inp`, y
eso lo cambia todo.

---

## 1. El mnemotécnico es una CLAVE; el nombre largo, un campo

`UEM`, `Alemania`, `IPC_ES` son cortos por una razón: **tienen que valer como
nombre de fichero y como nombre de serie para el motor**, que la imprime en
cada informe y la usa para componer el EPS. Eso los hace crípticos, y de ahí
la petición.

La respuesta no es alargarlos, es **separar las dos cosas** — la misma regla
que ya rige los modelos:

> El nombre del fichero es cortesía. La identidad está en el manifiesto.

```yaml
series:
  IPC_DE:
    elegido: m02
    descripcion: >
      Índice de precios de consumo armonizado, Alemania.
      Índice general, base 2015 = 100.
    fuente: Eurostat, tabla prc_hicp_midx
    url: https://ec.europa.eu/eurostat/databrowser/view/prc_hicp_midx
    bajada: 2026-09-20
    unidades: índice 2015 = 100
    notas: >
      Serie encadenada; el salto de 1/2015 es de base, no de precios.
```

Todos opcionales y **todos vacíos por defecto**: «no consta» tiene que verse
como no consta, igual que la razón de una iteración.

Y una consecuencia práctica: con `descripcion` puesta, la lista de series
puede enseñarla en un globo, y la clave deja de necesitar ser legible.

### 1.1 Renombrar la clave es otra cosa, y cuesta

Cambiar `IPC_DE` por `IPC_ALE` **no es editar un campo**: mueve el directorio,
reescribe las claves del manifiesto y deja a los `.inp` ya estimados diciendo
el nombre viejo dentro. Se puede hacer, pero es una **operación**, no una
casilla de un formulario, y va con su aviso.

Sobre el nombre que queda dentro de los `.inp` ya estimados, mi criterio es
**no tocarlos**: un `.pre` y un `.out` son el registro de una corrida, y el
nombre con que se corrió es parte de ese registro. Los modelos nuevos llevan
el nombre nuevo.

Con `descripcion` disponible, renombrar debería hacer falta muy pocas veces —
que es justamente el objetivo del reparto.

---

## 2. La muestra es parte de la ESPECIFICACIÓN

Aquí está el criterio que manda, y de él sale todo lo demás:

> El `.inp` lleva el número de observaciones y la fecha de comienzo. **La
> muestra no es una vista de la serie: es parte de lo que se estimó.**

Tres consecuencias, y ninguna es negociable:

1. **Truncar un modelo estimado es imposible sin mentir.** Su `.out` describe
   una estimación sobre 367 observaciones; cambiarle la muestra deja un
   informe que ya no corresponde. Es exactamente lo que dices: dañaría el
   linaje.
2. **Truncar es DERIVAR, no editar.** Una muestra distinta es un modelo
   distinto aunque la especificación sea idéntica.
3. **Dos modelos con muestras distintas no se comparan.** La d.t. residual de
   uno hasta 2019 y la de otro hasta 2026 no miden lo mismo, y la rejilla los
   pone en la misma columna. Eso hay que decirlo, no confiar en que se
   recuerde.

### 2.1 Muestra total y submuestras declaradas

**La muestra total es lo que entró, y no se toca nunca.** Vive en el nodo de
datos, que ya es intocable por la misma razón.

Una **submuestra** es una ventana **declarada, con nombre y con razón**:

```yaml
muestras:
  pre-covid:
    hasta: 12/2019
    razon: 2020-2022 es otro proceso; se estima antes y se predice encima
  completa:              # implícita, no hace falta escribirla
```

Van en el proyecto y no en cada serie porque una submuestra es una **decisión
del análisis** —«hasta donde acaba el régimen anterior»— y normalmente vale
para todas las series a la vez. Una serie más corta que la ventana simplemente
da lo que tiene; eso es un hecho, no un error.

### 2.2 Cada modelo declara en qué muestra nació

```yaml
modelos:
  IPC_DE/m04:
    version: 4
    padre: m02
    muestra: pre-covid
    razon: el mismo modelo, antes del salto
```

**Declarado, no deducido.** Se podría mirar el `nobs` del `.inp` y restar,
pero eso es volver a parsear para saber algo que nadie escribió — la regla que
ya nos costó el `rol: datos`.

### 2.3 Las muestras, en pestañas abajo — como las hojas de un cálculo

> «¿Qué tal generar un notebook con los samples? Donde está la lista de los
> modelos, un botón que pueda navegar entre muestras. Los tabs de las
> subsamples irían abajo, como las *sheets* de una hoja de cálculo.»

**Mejor que lo que yo había propuesto**, y conviene decir por qué, porque
contradice el §5 de esta misma propuesta.

Yo había dicho una columna «Muestra». Una columna **avisa**; la pestaña
**impide**. Si los modelos de cada muestra viven en su hoja, la comparación
inválida —la d.t. residual de uno hasta 2019 contra otro hasta 2026— deja de
ser un descuido posible: no se pueden ver los dos a la vez. El invariante pasa
de advertencia a estructura, que es siempre el cambio bueno.

Y la columna desaparece: la rejilla se queda en las siete que acabamos de
fijar, sin gastar ancho en algo que la pestaña ya dice.

**¿No es esto el «modo» que yo rechazaba?** No, y la diferencia importa. Lo que
rechazo en §5 es una muestra **activa del proyecto**, en un sitio lejano, que
cambie en silencio lo que hacen todos los botones. Una hoja de cálculo no se
siente como un modo porque **la pestaña está pegada a los datos que gobierna y
es donde acabas de pulsar**. Es una selección, no un ajuste. Ahí es donde
estaba mal mi objeción.

#### Lo que la pestaña obliga a decidir, y está bien que obligue

**El nodo de datos vive en la muestra total, y sólo aparece ahí.** Un modelo de
`pre-covid` sigue colgando de `m00`: el linaje es «estos datos, esta ventana,
esta especificación», y la ventana es un **campo del modelo**, no un dato
distinto. Una muestra no tiene datos propios — si los tuviera habría tres
copias de la serie y volveríamos a tener tres dueños.

**Las muestras son del proyecto, y ahora hay una razón más.** Si fueran de cada
serie, la fila de pestañas bailaría al cambiar de serie. Siendo del proyecto,
la fila es estable: siempre las mismas hojas, y una vacía significa «aquí no
has estimado nada todavía», que es una invitación y no un hueco.

**La hoja «+».** Definir una submuestra es exactamente el botón de hoja nueva.
Mejor sitio que cualquier menú, y con la misma pregunta al crearla: hasta
dónde, y por qué.

#### Las dos condiciones

1. **Los veredictos de abajo cuentan el proyecto entero, no la hoja visible.**
   Si contaran sólo lo que se ve, las pestañas mentirían por omisión — y «lo
   que hay que mirar» es justo lo que no puede ir filtrado. Un proyecto con
   tres muestras tiene el triple de modelos y dos tercios invisibles en cada
   momento; el veredicto es lo que impide que se olviden.

2. **Hay que poder cruzar cuando el cruce es legítimo.** Las pestañas impiden
   comparar los *números*, que es lo que había que impedir. Pero comparar la
   *estructura* sí vale —«¿sale el mismo (0,1,1)(0,1,1)₁₂ antes y después del
   salto?»— y eso la pestaña lo esconde. Se recupera barato: en el globo de la
   fila, «la misma especificación está en: completa (m02)», y una entrada de
   menú «Ver este modelo en las otras muestras».

#### Factibilidad: alta, con un punto de cuidado

`gtk_notebook_set_tab_pos(GTK_POS_BOTTOM)` es una llamada. Lo demás es que
**cada hoja tiene su propia rejilla**: un widget no puede tener dos padres, así
que o hay N vistas o las pestañas son un selector de mentira con la vista
debajo. Lo honesto es N vistas — la constructora de la rejilla ya existe y se
llama una vez por hoja.

A cambio, todo lo que hoy hace `marcada(a->l_modelos, …)` pasa por un
`vista_actual(a)`. Es un accesor y unas treinta líneas repartidas.

El punto de cuidado es **el repintado y la marca**: ya nos mordió una vez
—`clear` dispara `changed` con nada marcado— y con N vistas hay N sitios donde
puede volver a morder. El guardia `recolocando` que ya existe tiene que cubrir
el cambio de hoja también.

### 2.4 Lo que se ve

```
    ┌─ Modelo nuevo ─ Iterar ─ Elegir ─ Razón… ─│─ → fug ─ → fue ─┐
    │  ★  Modelo  Viene de  Estructura   d.t.res.  Q (g.l.)   p   │
    │     m01     m00       (0,1,0) log   0.3759   117.7(39) .000 │
    │  ★  m02     m01       (1,1,0) log   0.3744   105.9(38) .000 │
    │                                                             │
    ├─────────────────────────────────────────────────────────────┤
    │  Completa │ pre-covid │  +                                  │
    └─────────────────────────────────────────────────────────────┘
      5 series, 7 modelos, 6 estimados · 2 con el elegido declarado
      2 iteraciones sin razón (IPC_DE/m04, …). El linaje está; el porqué, no.
```

Los dos veredictos de abajo del todo siguen contando **todo**, no la hoja.

### 2.5 El gesto

En el menú de la serie, junto a «Modelo nuevo»:

    Modelo nuevo, desde los datos          →  muestra completa
    Modelo nuevo, en otra muestra…         →  elige o define una submuestra

Y en el de un modelo:

    Reestimar en otra muestra…             →  MISMA especificación, otra
                                              ventana. Deriva; no pisa.

Ese último es el que pides: coges `m02`, lo llevas a `pre-covid`, y sale `m05`
con el mismo modelo y otra muestra, colgado de `m02`. Las dos estimaciones
quedan, y el linaje dice de dónde salió cada una.

---

## 3. El `.csv`: el dato, con un solo dueño

> «Probablemente la madre y el proyecto deberían guardar un `.csv` con los
> datos en total sample.»

Sí, y por una razón más fuerte que la comodidad: **hoy los números sólo
existen dentro de `m00.inp`**, en el formato del motor. Eso tiene tres costes
concretos.

1. **Nadie más los puede leer.** ATSW Python, una hoja de cálculo o R tienen
   que entender el `.inp` para ver un dato que no es del motor.
2. **No se puede extender la muestra.** Cuando llegue el dato de 8/2026 hay
   que volver a importar el fichero entero y perder lo que se hizo.
3. **Y sobre todo: no se puede generar una submuestra.** Para escribir el
   `.inp` de una ventana hacen falta los números de esa ventana, y hoy hay que
   sacarlos del `.inp` de la ventana completa.

La propuesta:

```
IPC_DE/
  datos.csv            <- LA MUESTRA TOTAL. El dato.
  work/
    IPC_DE_m00.inp     <- generado del csv, ventana completa (para fug)
    IPC_DE_m04.inp     <- generado del csv, ventana pre-covid + su modelo
```

Y la regla que lo hace sano, que es la de siempre:

> **El `.csv` es el dato; los `.inp` se generan de él.** No hay dos dueños:
> hay un dueño y una derivación.

Es la misma disciplina que `.pre → .inp`, y da la misma prueba comprobable:
leer el `.csv` con `lib/datos` y volver a escribirlo tiene que dar lo mismo.
`lib/datos` ya lee ese formato — cabecera con la frecuencia y fechas en la
primera columna — así que el ida y vuelta se prueba el primer día.

### 3.1 Lo que esto abre, y no hay que construir ahora

Truncar a 12/2019 **y predecir lo que sigue** es la evaluación ex-post de toda
la vida, y con las submuestras declaradas el proyecto sabe lo que viene
después de la ventana: tiene los valores realizados. `fuf` ya predice. Juntar
las dos cosas —predecir sobre `pre-covid` y comparar contra lo que pasó— sale
casi gratis una vez que esto esté, pero **no es parte de esta propuesta**.

---

## 4. Orden que propongo

| | qué | por qué en ese sitio |
|---|---|---|
| 1 | Metadatos de la serie + «Editar…» | No toca ningún número. Utilidad inmediata, riesgo cero. |
| 2 | `datos.csv` al importar, y `m00.inp` generado de él | Es el cambio de dueño. Todo lo demás lo necesita. |
| 3 | `muestras` en el manifiesto y `muestra` en cada modelo | Registrar antes de usar. |
| 4 | «Modelo nuevo en otra muestra» y «Reestimar en otra muestra» | El gesto, cuando ya hay dónde apuntarlo. |
| 5 | Las pestañas abajo, una por muestra, con su hoja «+» | Lo que evita el daño de verdad: impedir en vez de avisar. |
| 6 | Renombrar la clave | Lo menos urgente si 1 funciona. |

---

## 5. Lo que NO propongo, y por qué

- **Una muestra «activa» del proyecto**, que cambie lo que se ve en todas
  partes. Es un MODO, y la fase 4 midió que los modos son peores que las
  dependencias.

  Cuidado con confundir esto con las pestañas del §2.3, que sí van: la
  diferencia es que la pestaña **está pegada a lo que gobierna y es donde
  acabas de pulsar**, y que lo de fuera de ella —los veredictos— sigue
  contando el proyecto entero. Un modo es malo cuando es invisible y está
  lejos de la acción; una hoja de cálculo no lo es.
- **Truncar el `.csv`.** La muestra total es lo que entró. Si se recorta, se
  pierde la única copia de lo que había, y la submuestra deja de ser una
  ventana para pasar a ser una amputación.
- **Deducir la muestra del `nobs`.** Ya sabemos cómo acaba eso.

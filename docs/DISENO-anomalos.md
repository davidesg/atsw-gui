# Los anómalos: calibrar la distorsión

> «Ahora vamos con la calibración de los anómalos. Art tiene varias funciones
> para evaluar y calibrar distorsiones.»

Son **tres cosas distintas** y conviene no mezclarlas, porque ocurren en
momentos distintos y sólo una de ellas está ya calculada.

---

## 1. Lo que hay hoy, y el hueco

| momento | la pregunta | qué hay |
|---|---|---|
| **antes del modelo** — identificación | ¿cuánto de lo que veo en el correlograma **es el anómalo**? | **nada** |
| **después** — residuos | ¿qué fechas distorsionan cada r(k)? | el motor **ya lo calcula** |
| en ambos | ¿estos extremos son un suceso o varios? | nada |

El `.out` de **fue** trae un bloque entero, *Calibration of distortions of the
ACF*, que por cada autocorrelación da los tramos de fechas que más contribuyen
y cuánto:

```
  r(2) = -0.095       3/2018 -  5/2018       -0.026
                      5/2012 -  7/2012       -0.022
```

**Y `fug` no lo trae.** Comprobado: el bloque existe sólo en el informe de fue
y sólo sobre los **residuos** de un modelo ya estimado. Justo donde la
calibración decide de verdad —eligiendo los órdenes, antes de que haya
modelo— no hay nada que leer.

---

## 2. La pregunta que importa, y por qué son **dos** funciones

De `art.calibracion`, y es el argumento que manda:

> La **PACF** decide el orden **AR**. La **ACF** decide el orden **MA**.
> Calibrar sólo la ACF deja media identificación a ciegas — y no porque la ACF
> prediga a la PACF: **porque no la predice.** La PACF es una transformación
> **no lineal** de la ACF (Durbin-Levinson), así que las dos pueden moverse en
> direcciones opuestas en el mismo retardo.

Con el caso medido que trae art (∇ln PGAS, n = 83, banda ±0,2151, omitiendo
|z| > 2,5):

| | con el anómalo | sin él | veredicto |
|---|---|---|---|
| ACF(2) | +0,1321 | +0,3143 | **enmascarada** — la tapaba: *falta* MA |
| PACF(2) | −0,2964 | −0,1967 | **fabricada** — no existe sin él: *sobra* AR |

El mismo anómalo **escondía una señal MA y fabricaba una AR a la vez**. Quien
calibrase sólo la ACF concluiría «hay más MA de la que creía» y no se enteraría
de que el AR(2) que iba a estimar **era el anómalo**.

### Y sirve en los dos sentidos — esto es lo que evita sobre-intervenir

Si al quitar el anómalo **ningún retardo cambia de veredicto** dentro/fuera de
banda, intervenirlo **no compra nada para la identificación**. Y añadir una
intervención que no hace falta es gastar un parámetro y tocar la serie sin
motivo.

Un módulo que sólo sabe decir «hay un atípico» empuja a intervenir siempre.
Éste sabe decir **«no hace falta»**, que es la mitad que suele faltar.

---

## 3. Cómo se calcula, y por qué es barato

Con `I` el conjunto de índices señalados:

```
μ̂  sobre las observaciones RETENIDAS
z̃ₜ = (xₜ − μ̂)   si t ∉ I
     0           si t ∈ I
r(k) = Σ z̃ₜ z̃ₜ₊ₖ / Σ z̃ₜ²
φ(k) por Durbin-Levinson sobre ese r(k)
```

**La PACF sale de la ACF**, así que *una sola omisión da las dos funciones*.
Ésa es la propiedad que hace esto barato — y en C es aritmética elemental: sin
GSL, sin álgebra matricial, sin distribuciones.

Y la desviación a cero en vez de eliminar por pares es una decisión declarada,
no una comodidad: eliminar por pares cambia el número de sumandos de cada
retardo y las r(k) dejan de ser comparables entre sí.

---

## 4. Los episodios: un suceso no son tres atípicos

De `art.episodes`, con su coste medido:

> Un suceso que dura tres períodos eran **tres atípicos sueltos**, cada uno con
> su forma decidida por una comprobación de adyacencia. Dos formas elegidas por
> una regla, sin estimar nunca la alternativa: la pregunta del análisis de
> intervención —¿permanente o transitorio?— se contestaba con una **etiqueta**
> en vez de con un contraste. Costaba −16,24 AIC en la réplica de Bolivia, y
> **1 de 8** corridas encontraba la segunda intervención del episodio 2008-09.

Así que: agrupar los extremos separados por un hueco ≤ `ventana` en un
**episodio**, con su extensión. Y la ventana es un **parámetro declarado**, no
un número mágico enterrado — por defecto 2, que admite un período tranquilo
dentro del suceso.

De la extensión sale la **forma general** que le corresponde; cuál de ellas se
estima es otra cosa, y es del analista.

---

## 5. El diseño

Tres módulos, y la misma separación de siempre: **leer no es calcular, y
calcular no es decidir.**

### `lib/outfile` — los hechos que ya están

- la lista de residuos extremos con su fecha y su |z|;
- el bloque de calibración del motor: por cada r(k), los tramos y su
  contribución.

*Coste: horas. No toca el motor: está impreso.*

### `lib/anomalos` — agrupar y calibrar

```c
/* Los extremos, agrupados en episodios. ventana DECLARADA. */
int an_episodios( const double *z, int n, double umbral, int ventana,
                  AnEpisodio *out, int max );

/* La ACF y la PACF con y sin los índices señalados, y el veredicto por
   retardo. UNA omisión da las DOS funciones.                          */
int an_calibra( const double *z, int n, const int *omitir, int nomitir,
                int lags, AnCalibra *out );
```

Y el veredicto por retardo, que es lo que se enseña:

```c
typedef enum { AN_IGUAL, AN_ENMASCARADA, AN_FABRICADA } AnVeredicto;
```

más el resumen que evita sobre-intervenir: **¿cambia algún retardo de
veredicto?** Si no, la respuesta es *«intervenir esto no compra nada»*.

*El umbral del |z| depende de n* —bajo especificación correcta el máximo de n
normales crece con n— así que va con nombre en un sitio, como los de
`lib/dictamen`.

### La ventana: el interruptor **redibuja**

> «No quiero barras una al lado de otra, quiero que las acf/pacf cambien
> con/sin contribuciones. También cómo cambia el Q.»

Eso, y las dos alternativas que se descartaron merecen quedar escritas porque
las dos parecían mejores:

**Barras una al lado de otra: no.** Lo que hay que ver es **la misma figura
moviéndose**. Lo que se compara no son dos números: son dos **lecturas** del
correlograma — *«¿corto el AR en el 2?»* se responde mirando **un** dibujo, y
con dos barras por retardo la lectura deja de ser la de siempre.

**Un degradé con la parte que aporta el anómalo: tampoco.** `r(k)` con y sin
son **dos cocientes distintos** —cambia el denominador y cambia *n*— así que

```
r_con  =  r_sin  +  aportación_del_anómalo      ← NO se cumple
```

Una barra apilada afirmaría una descomposición que **no existe**. Es la misma
clase de mentira callada que el módulo existe para no contar.

**Lo que sí se dibuja son las dos bandas** cuando difieren: la del estado
visible entera, la del otro punteada. Quitar observaciones **ensancha** la
banda, y comparar contra una sola haría parecer que algo sale de banda cuando
lo que pasó fue que la banda se movió.

**Y el color es del veredicto, no del estado**: rojo lo que el anómalo
**fabrica**, verde lo que **enmascara**, gris lo que da igual. Así el dibujo
dice, sin tocar el interruptor, *qué* retardos están en juego.

**La escala la fija el mayor de los dos estados**, no el visible: si cambiara
al pulsar, las barras se moverían por el dibujo y no por los datos.

**Y el Q, con y sin**, con la misma fórmula y cada uno con **su** *n*. No se
compara contra el que imprime el motor: ése sale de su propio estimador, y
mezclarlos sería restar peras de manzanas.

### Se calibra lo que se marca, y marcar no es calibrar

> «Me gustaría utilizar el mismo gráfico de residuos + acf/pacf para calibrar
> las distorsiones, con check marks para marcar los anómalos que se quiere
> calibrar. Puede ser uno o dos o todos. Se puede calibrar 3 episodios a la
> vez. Para calibrar se usa un push button.»

El interruptor binario —«con todos» / «sin ninguno»— contestaba una pregunta
que nadie hace. **La pregunta de verdad no es «¿y si no hubiera anómalos?»
sino «¿y si no estuviera ÉSTE?»**, porque *éste* es el que se acaba
interviniendo, con su parámetro y su forma.

Así que:

- **el gráfico de residuos de siempre**, arriba, con los extremos en rojo y
  **los episodios marcados sombreados encima**: se ve sobre qué se va a
  calibrar *antes* de calibrar, que es la mitad de la pregunta;
- **una casilla por episodio**, con sus fechas, su extensión y su z máximo;
- **un botón «Calibrar»**, y ahí está la separación que importa: **marcar dice
  QUÉ se quiere probar; probarlo es pulsar.** Sin esa separación no se pueden
  marcar tres y ver el efecto **de los tres juntos** — que no es lo mismo que
  el de cada uno por separado, y es justamente la pregunta que no se podía
  hacer.

Al calibrar se enseña el resultado, que es lo que se acaba de pedir; y el
interruptor queda para **volver al original y comparar**, que es la misma
figura moviéndose.

---

### El gráfico es el de fue, y el círculo dice cuál

La primera versión de la ventana dibujaba con cairo un gráfico de residuos y
dos correlogramas *parecidos* a los de fue. Parecidos no sirve: el analista
compararía dos lienzos distintos creyendo que compara dos calibraciones, y las
diferencias de escala, de bandas y de margen se leerían como diferencias de
los datos.

Así que se dibuja **el de fue** — `fp_PlotSer_CorrSer`, la opción `-c` — sobre
los residuos que trae el `.out`. Tres cosas lo hacen posible sin tocar el
motor:

1. **`fp_PlotSer_CorrSer_marks`**, nueva en `lib/fugplot`: el mismo `-c` con un
   círculo alrededor de las observaciones marcadas. La de siempre pasa a ser
   un envoltorio con `marks == NULL`, así que **el dibujo de fue no se mueve un
   punto**. Marcar un episodio en una lista y verlo sombreado no dice *cuál*
   es; el círculo va sobre el punto, donde está la fecha.

2. **«Sin los anómalos» es la serie con la media de los retenidos en el
   hueco.** El estimador declarado de `lib/anomalos` es la desviación a cero:
   μ sobre las retenidas, z̃ = z − μ en ellas y 0 en las omitidas. Poner μ en
   las omitidas da exactamente esa serie — su media *es* μ — de modo que la
   `Acf` del motor devuelve nuestra r(k) sin tener que pasarle correlaciones
   ya calculadas.

3. **La escala se fija con el máximo de los dos estados** y se le pasa por
   `cbands`, que es la opción con la que el motor admite una escala dada. Si
   la eligiera sola en cada estado, al accionar el interruptor las barras se
   moverían por el dibujo y no por los datos.

Los mandos van **al pie del gráfico** (`preview_set_footer`), como λ, d y D en
el vistazo: una casilla por episodio con su fecha, `Calibrar` y «ver
calibrado». No hay una ventana de anómalos aparte mirando a otra: el analista
trabaja al pie de la figura. Cambiar una marca **deshace** la calibración
anterior — lo que se ve tiene que ser lo que está marcado.

Lo que el dibujo de fue no dice, porque no es su dibujo, se dice en el pie con
palabras: qué retardos cambian de lado y en qué sentido (`ACF 2 enmascarada ·
PACF 2 fabricada`), los dos Q con su n y las dos bandas. Antes era color; así
se puede copiar a un informe.

El precio, dicho: la madre enlaza `lib/fugplot` con su propio `plothost.h` y un
`plotstats.c` que **copia** las fórmulas de `diagnose.c` y `nlatools.c` del
motor. Copiadas, no reescritas — el gráfico tiene que ser el de fue, y eso se
consigue copiando el cálculo. Si el cálculo cambia en el motor, hay que
cambiarlo aquí.

---

## 6. Lo que **no** hace

**No interviene.** Ni elige la forma, ni escribe la intervención en el `.inp`,
ni estima la alternativa. Eso es `suggest_intervention_form`, y es el paso
siguiente — y una **decisión**.

Este módulo llega hasta *«el episodio de 5/2012 a 7/2012 fabrica el AR(2):
quitarlo cambia el veredicto del retardo 2»* y se para ahí. La misma línea que
en `lib/dictamen`.

---

## 7. Orden

| | qué | coste |
|---|---|---|
| 1 | los extremos y la calibración del motor, en `lib/outfile` | horas |
| 2 | `lib/anomalos`: episodios y calibración ACF/PACF, con su prueba | 1-2 días |
| 3 | la ventana «Anómalos…»: episodios, los dos correlogramas y el Q | hecho |
| 3b | el gráfico de fue (`-c`) con círculo en lo marcado, y los mandos al pie | hecho |
| 3c | «Sugerir intervención…» sobre los sucesos marcados — ver `DISENO-intervencion.md` | hecho |
| 4 | el mismo interruptor en el vistazo, sobre la serie sin modelo | pendiente |

Nada toca los motores: lo de después está impreso, y lo de antes se calcula
sobre la serie que ya tenemos en `datos.csv`.

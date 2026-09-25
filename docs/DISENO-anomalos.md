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

### La ventana

El sitio natural **ya existe**: el vistazo de «Serie y ACF/PACF», que tiene λ,
d y D al pie y se redibuja al tocarlas. Ahí, un interruptor **«sin los
anómalos»** enseña los dos correlogramas — que es exactamente la comparación de
§2, vista en vez de leída.

Y una ventana «Anómalos…» con la lista de episodios, su extensión, su forma
general, y el veredicto por retardo. Desde la serie (identificación) y desde el
modelo (residuos).

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
| 3 | el interruptor «sin los anómalos» en el vistazo | 1 día |
| 4 | la ventana «Anómalos…» con la lista y los veredictos | 1 día |

Nada toca los motores: lo de después está impreso, y lo de antes se calcula
sobre la serie que ya tenemos en `datos.csv`.

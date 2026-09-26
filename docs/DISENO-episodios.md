# Cuándo un tramo de residuos es un incidente

**Implementado** en `lib/anomalos` con K = 1,5 y escala muestral, decisión
del analista. La medición de más abajo es la de la regla tal como quedó.

Lo plantea el analista así: *no es lo mismo un anómalo aislado de 3σ, cuya
probabilidad es muy pequeña, que uno de 2σ, que pasa el 5 % de las veces. Pero
un 3σ **con** un 2σ antes y otro después es, en un gaussiano, prácticamente
imposible. La probabilidad de un incidente no es la del punto aislado.*

Es correcto, y tiene consecuencias sobre la regla.

---

## 1. Lo que hay hoy, y por qué se queda corto

Un umbral para **declarar** —`u = √(2 ln n)`, con suelo 2,5— y otro fijo de
**2,0** para **extender** a los vecinos. El segundo se puso con dos
justificaciones buenas: es el umbral con el que el motor marca `@` en el
`.out`, y art midió que subirlo a 3,0 cuesta la mitad de la potencia.

Lo que está mal no es la idea de dos umbrales: es que **el segundo sea una
constante**. Lo que hace improbable un tramo no es que cada punto pase un
listón, es **cuántos lo pasan a la vez**.

---

## 2. Los números, bajo la nula

Con residuos gaussianos iid tipificados, `p₁(c) = P(|z| > c)`:

| c | p₁(c) | esperados en n = 261 |
|---|---|---|
| 2,0 | 0,0455 | **11,9** |
| 2,5 | 0,0124 | 3,2 |
| 3,0 | 0,0027 | 0,70 |
| 3,34 = √(2 ln 261) | 0,00085 | **0,22** |

Doce falsos a 2σ por serie: por eso 2σ suelto no es noticia. Pero el tramo
`(3,01 · −3,50 · 2,36)` del caso real —m04 de IPC\_ES, 1–3/2021— tiene
probabilidad conjunta, **por posición**, de ~10⁻⁸; en toda la serie, ~10⁻⁶.
Es **136 veces más raro** que el extremo aislado que hoy sí se declara.

La conclusión del analista se sostiene: **hay que puntuar el tramo, no los
puntos.**

---

## 3. La regla propuesta

Para un tramo de L períodos consecutivos con residuos tipificados z₁…z_L:

```
        S = Σ zᵢ²        y        bajo la nula   S ~ χ²(L)
```

**Es un episodio si el tramo es al menos tan improbable como un extremo
aislado en el umbral de declarar:**

```
        P( χ²_L > S )  ≤  p₁(u) / K
```

Tres propiedades que la hacen la regla correcta y no una más:

1. **Con L = 1 es exactamente la regla de siempre.** `P(χ²₁ > z²) ≤ p₁(u)` es
   `|z| ≥ u`. No hay discontinuidad ni caso especial.
2. **No introduce ninguna constante nueva.** `u` ya estaba declarado y depende
   de n, que es lo que gobierna las comparaciones múltiples.
3. **Usa las magnitudes, no sólo si cruzan un listón.** `(3,5 · 2,0)` y
   `(2,0 · 2,0)` dejan de ser el mismo caso.

### El escalón que implica

Si todos los puntos del tramo valen lo mismo, el listón por punto sale de la
fórmula — **derivado, no elegido**:

| n | L=1 | L=2 | L=3 | L=4 | L=5 | L=6 |
|---|---|---|---|---|---|---|
| 80 | 2,96 | 2,41 | 2,15 | 2,00 | 1,89 | 1,81 |
| 261 | 3,34 | 2,66 | 2,35 | 2,17 | 2,04 | 1,95 |
| 600 | 3,58 | 2,82 | 2,48 | 2,28 | 2,14 | 2,04 |

*(con K = 1; con K = 1,5 sube ~0,1 en toda la fila)*

El 2,0 fijo de hoy era, sin saberlo, **el valor correcto para L ≈ 4–5 en una
serie de 261**: demasiado laxo para pares y demasiado estricto para tramos
largos.

---

## 4. La calibración, medida

4 000 series gaussianas iid de n = 261, escaneando L = 1…8 y quedándose con
los tramos no solapados más significativos:

| regla | episodios falsos por serie |
|---|---|
| dos umbrales (la de hoy) | 0,212 |
| tramo conjunto, K = 1 | 0,338 |
| tramo conjunto, **K = 1,5** | **0,226** |
| tramo conjunto, K = 2 | 0,168 |
| *referencia: lo que hoy cuesta un extremo aislado* | *0,222* |

Escanear ocho longitudes **sí** infla los falsos positivos: con K = 1 el
analista vería un 50 % más de episodios falsos. Con **K = 1,5** la carga de
falsas alarmas queda **exactamente donde está hoy**, y a cambio los tramos se
detectan con muchísima más potencia.

El precio de K = 1,5: un extremo aislado necesita 3,45 en vez de 3,34. Es
poco —de 0,22 a 0,16 falsos aislados esperados— pero **es un cambio de
comportamiento en el caso más común**, y por eso se dice.

### Cómo queda el caso real y sus vecinos (n = 261, K = 1,5)

| tramo | L | p conjunto | veredicto |
|---|---|---|---|
| **3,01 · −3,50 · 2,36** (el real) | 3 | 6,2·10⁻⁶ | **episodio** |
| sólo el del medio (−3,50) | 1 | 4,7·10⁻⁴ | episodio |
| 2,8 · 2,8 · 2,8 | 3 | 3,2·10⁻⁵ | episodio |
| seis seguidos de 2,0 | 6 | 5,2·10⁻⁴ | episodio |
| 3,0 con un 2,0 al lado | 2 | 1,5·10⁻³ | no |
| tres seguidos de 2,0 | 3 | 7,4·10⁻³ | no |
| uno de 2,0 | 1 | 4,6·10⁻² | no |

---

## 5. Lo que la regla NO arregla, y hay que decirlo

**La desviación típica está inflada por los propios anómalos.** Los z se
tipifican con la sd muestral, que los extremos que buscamos hacen mayor: el
contraste es **conservador**, y tanto más cuanto peor es el caso. Una escala
robusta —MAD × 1,4826— lo corregiría, pero cambia todos los z y por tanto
todos los veredictos: es **otra decisión**, no un detalle de ésta.

**Los residuos no son exactamente iid.** Vienen de un modelo ajustado: hay
error de estimación y autocorrelación residual leve. La χ² es una
aproximación y la simulación de arriba usa iid exacto.

**El solapamiento se resuelve con avaricia**: gana el tramo más significativo
y los demás no pueden pisarlo. Es una convención, no un teorema.

---

## 6. Lo decidido, y lo que la batería encontró

**K = 1,5 y escala muestral.** Con la regla completa, la medición final sobre
4 000 series bajo la nula da **0,192 episodios falsos por serie**, algo por
debajo de los 0,21 de la regla anterior: la nueva **no es más laxa**, que era
la preocupación.

### Un período tranquilo separa dos sucesos

La primera versión confiaba en que el contraste rechazara solo las ventanas
con huecos —un período callado cuesta un grado de libertad y no aporta suma—.
**La batería enseñó que no**: dos picos de 5σ separados por dos ceros dan una
ventana de cuatro con p = 4·10⁻¹⁰, más improbable que cualquiera de los dos
solo. Y lo es — pero **lo es porque contiene dos sucesos**, no porque sea uno.
*Improbabilidad de la ventana no es unicidad del suceso.*

Así que el tramo tiene que ser **sólido**: todos sus períodos con |z| ≥
`AN_ACTIVO`. Un período que no es anómalo no forma parte de un suceso: lo
separa de otro.

#### Y el suelo es 2σ, no 1σ

Empezó en 1σ —«por debajo de una desviación típica no hay nada que
explicar»— y sobre un caso real se vio que no basta: **|z| > 1 pasa un tercio
de las veces**, así que en un tramo revuelto encadena todo. En IPC\_ES m02, la
inflación de 2022 salía como **un** suceso de siete períodos:

```
   12/2021 +2,54   1/2022 +1,05   2/2022 +1,69   3/2022 +5,68
    4/2022 −2,63   5/2022 +1,34   6/2022 +3,72
```

Son varios choques distintos, y leerlos juntos pediría **ocho escalones**. Con
2σ salen los dos que son: el par **3–4/2022** —un impulso de nivel— y
**6/2022** aparte.

El 2,0 no es un número nuevo: es con el que **el motor** marca los residuos
con `@`, y el que art usa para «este vecino es anómalo».

**Las dos caras del intercambio, medidas** (4 000 series bajo la nula):

| suelo | falsos/serie | tramos falsos de L≥4 | L máximo visto |
|---|---|---|---|
| 1,0 | 0,192 | 0,018 | **7** |
| 1,5 | 0,184 | 0,008 | 5 |
| **2,0** | **0,171** | **0,001** | 5 |

Y lo que cuesta en potencia sobre un incidente de verdad `(3,0 · −3,5 · 2,4)`:
detectado el **86,3 %** en vez del 88,5 %, con el tramo exacto el 42,5 % en vez
del 49,2 %. Dos puntos de detección a cambio de que los tramos largos falsos
prácticamente desaparezcan.

**Y la asimetría decide:** tragarse un tramo largo sobreparametriza —ocho
escalones, y una trayectoria determinista equivocada metida en la previsión
para siempre— y la sobreparametrización **no se detiene sola**. Cortar una
cola floja deja un residuo de 1,7σ, que es ruido.

Eso sustituye al parámetro de hueco que había, y es mejor: el hueco era un
número de períodos —una convención— y esto es una condición sobre el dato. El
caso `4,0 · −3,5 · ⟨nada⟩ · 3,2` pasa de leerse como un suceso de cuatro
—cuatro escalones— a leerse como lo que es: un par compensado y, tres períodos
después, otra cosa.

### Y quién decide cuántos parámetros

**K no.** K decide dónde hay un suceso y cuántos períodos abarca. Cuántos ω se
gastan lo decide la **escalera**, y el guardia contra sobreparametrizar es
**Treadway**: si la forma de abajo deja un anómalo al lado, no cubre el suceso.
Sobre el tramo real de m04:

| forma | ω | R² | mayor resto |
|---|---|---|---|
| escalón | 1 | 0,32 | **−3,50** |
| impulso | 1 | 0,75 | **+2,36** |
| episodio, 3 escalones | 3 | 0,95 | −0,78 |

Las dos formas escalares dejan un anómalo vivo. Eso es evidencia del dato, no
del umbral.

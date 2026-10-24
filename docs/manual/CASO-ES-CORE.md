# El IPC subyacente: un univariante con intervenciones

Este caso va **antes** del de la función de transferencia (el IPC y el
petróleo del [manual](MANUAL.md)). Es un modelo univariante de principio a
fin, con dos cosas que allí se dan por sabidas: **cómo se trata la
estacionalidad** y **cómo se mide un suceso económico con una
intervención**. El vídeo `video/ATSW-es-core.mp4` lo hace en los programas;
aquí está lo mismo por escrito, con los números y las fuentes.

---

## 1. La serie

El **IPC subyacente** de España es el índice general **sin alimentos no
elaborados ni productos energéticos**: lo que queda al quitar los precios
más volátiles. Dentro están los alimentos elaborados (pan, leche, lácteos…),
los bienes industriales no energéticos y los servicios.

- Serie del INE **IPC292511**: «Nacional. Subyacente: General sin alimentos
  no elaborados ni productos energéticos. Índice». Mensual, sin
  desestacionalizar, índice enlazado.
- Fichero: [datos/es_core.csv](datos/es_core.csv) (y
  [datos/es_core.xlsx](datos/es_core.xlsx)), de 2002-01 a 2026-09.
- **Muestra de estimación: 2002-01 a 2019-12**, n = 216. La pandemia y la
  crisis energética quedan fuera.

Fuente: Sitio web del INE, www.ine.es. Los datos del INE se reutilizan con la
licencia CC BY 4.0, citando la fuente ([aviso legal del INE][ine-aviso]).

## 2. Mirar antes de decidir

- **λ = 0, logaritmos.** Es un índice de precios: en logaritmos una
  diferencia es una tasa de variación, y un efecto se lee en porcentaje.
- **d = 1.** En logaritmos la ACF decae muy despacio. Con una diferencia
  regular lo que se ve es la inflación mensual.
- **Estacionalidad.** La ACF de la inflación mensual repite picos en 12, 24
  y 36 que apenas se apagan.

## 3. Dos maneras de tratar la estacionalidad

| | **Estocástica (D = 1)** | **Determinista (D = 0)** |
|---|---|---|
| Qué supone | el patrón del año **cambia poco a poco** | el patrón del año es **fijo**, un calendario |
| Cómo se recoge | diferencia estacional ∇₁₂ y un MA estacional (Box-Jenkins) | armónicos: 11 senos y cosenos de periodo 12, 6, 4, 3, 12/5 y 2 |
| Si el patrón de un mes cambia | el modelo lo va siguiendo | el cambio queda en los residuos |

La diferencia estacional compara cada mes con el mismo mes del año
anterior; el MA estacional, (1 − ΘB¹²), dice cuánto pesa el pasado. Con Θ
cerca de 1 el MA cancela la diferencia, y la estacionalidad era determinista;
con Θ lejos de 1 la diferencia estacional no sobra.

El caso se hace **dos veces**, primero con D = 1 y después con D = 0, para
ver qué recoge cada ruta y qué deja.

## 4. La ruta D = 1

**Identificar.** Con λ = 0, d = 1 y D = 1, art propone AR(1) × SMA(1). La
PACF tiene fuera de las bandas los retardos 1 y 3, y **AR(3) × SMA(1)**
comparte la similitud más alta (0.873). Es el modelo **m01**:
ARIMA(3,1,0)(0,1,1)₁₂.

**m01.** Residuos blancos, pero **no normales**: Jarque-Bera = 127.5,
curtosis 3.75. En **Anómalos…** un residuo se sale de todo: **septiembre de
2012**, +5.2 desviaciones típicas. Calibrado (sin ese mes), Jarque-Bera cae a
4.8.

**¿Por qué un escalón?** Una subida de impuestos sube el **nivel** de los
precios para siempre. En ln Pₜ eso es un escalón permanente; en la inflación,
∇ ln Pₜ, es un impulso de un solo mes. La firma en los residuos es justo esa:
un extremo aislado que no revierte. **Sugerir intervención…** propone un
escalón en 9/2012: **m02**. Con él, ningún residuo pasa del umbral y la
normalidad se acepta (JB = 5.3, p = 0.07).

**Dos puntos más.** En los residuos de m02 quedan dos cerca de 3σ, por debajo
del umbral: **octubre de 2012** (+2.7σ) y **octubre de 2007** (+3.1σ). Los dos
tienen una causa económica que se puede citar (§6), y por eso se prueban: el
umbral es una ayuda, no la ley.

**El IVA, una función de transferencia.** Octubre de 2012 no es otro suceso:
es la misma subida del IVA, que llegó en dos meses. Así que no se le pone un
segundo escalón, sino que el escalón de septiembre pasa a llevar un
numerador de orden 1, ω(B) = ω₀ − ω₁B (el convenio de fue: el primero suma y
los retardos restan). En fue_gui es «Edit» sobre el escalón y orden 1 en el
numerador. El de octubre de 2007 entra con un solo ω. Es **m03** (de
**Iterar**).

**m03**, el modelo de la ruta D = 1:

(1 − 0.206 B − 0.086 B² − 0.143 B³) ∇∇₁₂ [ln Pₜ − (0.00793 + 0.00497 B) S⁹ᐟ²⁰¹²ₜ − 0.00389 S¹⁰ᐟ²⁰⁰⁷ₜ] = (1 − 0.583 B¹²) aₜ

| intervención | ω(B) | ganancia ω(1) | e.t. | t | sobre el nivel |
|---|---|---|---|---|---|
| IVA, escalón 9/2012 | 0.00793 + 0.00497 B (e.t. 0.0011 cada uno) | **0.01290** | 0.00164 | 7.8 (W = 61.6) | **+1.3 %** |
| alimentos, escalón 10/2007 | 0.00389 | 0.00389 | 0.00106 | 3.7 | +0.39 % |

- σ̂ₐ = 0.123 % (m02: 0.132 %; m01: 0.144 %). 7 parámetros.
- Media de los residuos: −0.00014, t = −1.6: no se distingue de cero (m01:
  t = −1.5).
- Q de Ljung-Box: Q(8) = 6.5, Q(20) = 15.8, Q(32) = 21.4: residuos blancos.
- Θ = 0.583 (e.t. 0.056): lejos de 1, la diferencia estacional no sobra.
- El AR(3) tiene φ₂ = 0.086 con t = 1.2. Se deja: es el orden que pide la
  PACF.
- **La normalidad empeora**: JB = 12.1 (p = 0.002). Con menos ruido destacan
  los **eneros de 2009 y 2010** (−3.4σ cada uno). Se explica en el §7.

**La ganancia es la cifra económica.** **Diagnosticar ▸ Ganancia…** da
ω(1) = ω₀ − ω₁ = 0.0079 + 0.0050 = **0.0129**, con su error típico (0.0016) y
el contraste de Wald de que sea cero (W = 61.6, p = 0.000; la t es 7.8). Es el
efecto **permanente** del IVA sobre el nivel del subyacente: cuánto más caros
quedaron los precios. ω₀ y ω₁ sólo dicen cómo se repartió en el tiempo: un
0.79 % en el impacto, en septiembre, y un 0.50 % más con un mes de retraso. La
fila «Ganancia» de la Diagnosis lo resume: la intervención deja una ganancia
distinta de cero, el efecto es permanente y gobierna la previsión.

**¿Traslado en dos meses o gradual?** La otra forma natural es
ω₀/(1 − δB): un traslado que se va apagando, con ganancia ω₀/(1 − δ). Con los
mismos parámetros (ω₀, δ) ajusta peor: la log-verosimilitud baja de 135.6 a
131.3 (ω₀ = 0.0082, δ = 0.35, ganancia 0.0126). Con las dos cosas a la vez,
(ω₀ − ω₁B)/(1 − δB), δ sale negativo y no significativo (δ = −0.30, t = −1.8;
LR = 2.5). El traslado se hizo en dos meses y paró: ω₀ − ω₁B.

**Cómo se lee ω.** Con λ = 0, la intervención suma ω a ln Pₜ: el nivel queda
multiplicado por exp(ω), una subida de exp(ω) − 1, y para ω pequeño eso es
ω × 100 por ciento. (Los modelos que deriva la madre llevan el factor de
escala 1; con el factor 100 de fue, ω sale ya en porcentaje: 0.793.)

**¿Y dos escalones separados?** Escalones en 9/2012 y en 10/2012, cada uno
con un ω, dan exactamente la misma verosimilitud: es el mismo modelo escrito
de otra forma. Pero la función de transferencia dice lo que pasó, un solo
suceso con su dinámica, y da directamente la ganancia con su error típico.
Con dos escalones habría que sumar a mano y calcular el error típico de la
suma.

## 5. La ruta D = 0

**Identificar con D = 0.** art quita la estacionalidad con los 11 armónicos
antes de identificar. La ACF que queda conserva picos en 12 y 24: el patrón
fijo no lo explica todo y hace falta un **AR estacional**, una persistencia
estacional estacionaria. art le da casi todo el peso Akaike (0.999) a
**AR(1)+MA(1) × SAR(1)**: **m04**.

**La media.** Con d = 1 y D = 0 hace falta **μ**, la inflación mensual media
(la deriva del índice), y la madre la deriva ya libre: con d + D = 1 la deja
libre cuando la serie diferenciada tiene una media significativa, y aquí la
tiene (la inflación mensual media, t ≈ 2.6). En m03,
con d = 1 y D = 1, va fija en cero: con dos diferencias, una media sería una
tendencia en la inflación. En fue_gui se añaden las dos intervenciones, como en
m03: un suceso económico es el mismo en las dos rutas.

**m04:**

(1 − 0.902 B)(1 − 0.356 B¹²)(∇[ln Pₜ − armónicos − intervenciones] − 0.00131) = (1 − 0.737 B) aₜ

- IVA: ω(B) = 0.00759 + 0.00487 B; ganancia **0.01246** (e.t. 0.00167, t = 7.5),
  un +1.25 %. Alimentos 10/2007: ω = 0.00414 (t = 3.7). Casi lo mismo que en
  m03. También aquí el traslado gradual, ω₀/(1 − δB), ajusta peor (log-
  verosimilitud 144.1 frente a 147.7).
- μ = 0.00131 (t = 4.0): un 0.13 % al mes.
- σ̂ₐ = 0.121 %. **18 parámetros** (11 armónicos, μ, 3 del ARMA, 3 ω).
- Q(9) = 7.7, Q(21) = 24.1, Q(33) = 36.6: blancos. JB = 12.2 (p = 0.002).
- El par de armónicos de f = 5 (periodo 12/5) no es significativo; se deja el
  juego completo, como lo pone art.
- Con un AR estacional de orden 2, SAR(2), el ajuste mejora algo más; con
  SAR(1) los residuos ya son blancos, y se queda el más sencillo.

## 6. La lectura económica de las intervenciones

**Septiembre de 2012: la subida del IVA.** El Real Decreto-ley 20/2012, de 13
de julio, artículo 23, subió el tipo general del IVA del 18 % al 21 % y el
reducido del 8 % al 10 %, **con efectos desde el 1 de septiembre de 2012**; el
superreducido se quedó en el 4 %. Además, algunos bienes y servicios pasaron
del reducido al general (flores y plantas, entradas de teatros, circos y
espectáculos, peluquería…) ([BOE-A-2012-9364][boe]). Ese mes la tasa anual del
IPC pasó del 2.7 % al 3.4 % y la del subyacente subió siete décimas, hasta el
2.1 %; la del IPCA a impuestos constantes fue del 1.4 %, «más de dos puntos por
debajo de la del IPCA, debido al aumento del Impuesto sobre el Valor Añadido»
([INE, IPC septiembre 2012][ine-0912]).

**Octubre de 2012: el resto.** No todos los precios se cambian el mismo día:
parte de la subida llegó al mes siguiente. Ese mes la tasa del subyacente
subió otras cuatro décimas, hasta el 2.5 % ([INE, IPC octubre 2012][ine-1012]).
En octubre también entraron precios regulados, como las tasas universitarias,
que según el Banco de España «se trasladaron plenamente» ([Banco de España,
Informe Anual 2012, cap. 5][bde]). El término de octubre, ω₁, recoge las dos
cosas; el modelo no las separa.

**El tamaño.** La ganancia de la función de transferencia es un 1.3 % sobre el
nivel del subyacente. El Banco de España estimó que la traslación de la subida de la
imposición indirecta fue de «en torno al 40 % del impacto potencial total»
([Banco de España, Informe Anual 2012, cap. 5][bde]). Una subida del 18 % al
21 % encarece un 2.5 % lo que paga el tipo general (1.21/1.18); que el índice
suba un 1.3 % es compatible con una traslación parcial y con que no todo el
subyacente paga el tipo general.

**Octubre de 2007: los alimentos.** En la segunda mitad de 2007 subieron con
fuerza los precios mundiales de los cereales y de los lácteos: el índice de
precios de los alimentos de la FAO estaba en septiembre de 2007 un 37 % por
encima de un año antes ([FAO, Food Outlook, noviembre 2007][fao]). En España,
en octubre de 2007 la tasa anual del subyacente subió cinco décimas, hasta el
3.1 %, y el INE destacó «la leche, el pan, otros productos lácteos, la carne de
ave y los quesos»; la leche pasó del 11.4 % al 24.8 % anual en un mes ([INE,
IPC octubre 2007][ine-1007]). Son alimentos elaborados: están dentro del
subyacente. El modelo lo mide como un escalón de +0.39 %, con un solo ω: no
hay nada en los residuos que pida otra forma.

## 7. Cuándo no intervenir: los eneros

Las dos rutas dejan en los residuos los **eneros**, pero de forma distinta:

| enero | 2003 | 2007 | 2009 | 2010 | 2012 | 2014 |
|---|---|---|---|---|---|---|
| m03 (D = 1) | — | +2.0σ | −3.4σ | −3.4σ | −2.5σ | −0.7σ |
| m04 (D = 0) | +3.3σ | +3.6σ | −1.6σ | −1.4σ | −2.6σ | −1.9σ |

Con D = 0, que fija el patrón, los eneros de los primeros años quedan por
encima y los de 2009 a 2014 por debajo: el **patrón de enero cambió** a lo
largo de la muestra. Con D = 1 ese cambio se va absorbiendo, y sólo deja
residuos grandes en los años en que fue más brusco (2009 y 2010).

No es un suceso de un mes con una causa propia: es la **estacionalidad que
evoluciona**. Ponerle escalones en enero taparía los residuos, pero serían
parches sin causa económica, y el modelo diría algo falso: que en 2003 y en
2007 pasó algo. Si alguna vez se hace, se dice. Aquí no se interviene, y la
normalidad que no llega a aceptarse se acepta **diciendo por qué**.

(Los precios rebajados están en el IPC desde enero de 2002, cuando entró el
sistema de base 2001 ([INE][ine-rebajas]); la muestra empieza ahí.)

## 8. Elegir

| | m03 (D = 1) | m04 (D = 0) |
|---|---|---|
| σ̂ₐ | 0.123 % | 0.121 % |
| parámetros | 7 | 18 |
| Q | blancos | blancos |
| Jarque-Bera | 12.1 | 12.2 |
| los eneros | los absorbe, salvo 2009-2010 | todos fuera, como anómalos |
| ganancia del IVA | +1.3 % (t = 7.8) | +1.25 % (t = 7.5) |

Las dos ajustan igual, y las dos miden lo mismo del IVA. **m03** lo consigue
con 7 parámetros en lugar de 18, sigue la estacionalidad que evoluciona sin
parches, y su Θ = 0.58 dice que la diferencia estacional no sobra. Para
**describir y prever esta serie sola**, se elige **m03**, la ruta D = 1.

Si el modelo fuera a una **función de transferencia** o a un sistema, la
escuela tomaría la determinista, D = 0: es la especificación de elección para
los multivariantes, como en el caso del IPC y el petróleo del manual.

## 9. Rehacerlo

```sh
make
VOZ_AZURE=es-ES-AlvaroNeural bash docs/manual/video/video.sh es-core
```

Sin voz (sin `VOZ_AZURE` ni `VOZ_MODELO`) sale mudo, con subtítulos. Los
guiones están en `video/escenas-es-core/`; el proyecto se crea en
`/tmp/curso-es-core/es-core`. Los números de este texto son los que da fue
hoy: si cambia un motor, hay que revisarlos.

[ine-aviso]: https://ine.es/dyngs/AYU/es/index.htm?cid=125
[boe]: https://www.boe.es/buscar/act.php?id=BOE-A-2012-9364
[ine-0912]: https://www.ine.es/daco/daco42/daco421/ipc0912.pdf
[ine-1012]: https://www.ine.es/daco/daco42/daco421/ipc1012.pdf
[bde]: https://www.bde.es/f/webbde/SES/Secciones/Publicaciones/PublicacionesAnuales/InformesAnuales/12/Fich/cap5.pdf
[fao]: https://www.fao.org/4/ah876e/ah876e00.htm
[ine-1007]: https://www.ine.es/daco/daco42/daco421/ipc1007.pdf
[ine-rebajas]: https://ine.es/ipc01/rebajas02.htm

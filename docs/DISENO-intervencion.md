# Sugerir la intervención: de calibrar a intervenir

Calibrar contesta *«¿cambia la identificación si quito esto?»*. Si la
respuesta es que sí, queda la otra pregunta, que es la que se acaba
estimando: **qué se le pone.** Este documento es la segunda.

---

## 1. Por qué no lo decide el ajuste

Las dos lecturas escalares de un suceso son:

| | en el nivel | efecto |
|---|---|---|
| **1a** | escalón ω en T | **permanente** |
| **1b** | impulso ω en T | **transitorio**, un período |

Cuestan **un parámetro cada una** y **no están anidadas**. El AIC compara
*dentro* de un peldaño o confirma una subida ya justificada por otra cosa;
entre dos modelos del mismo coste que no se contienen, elegir por ajuste es
leer ruido (art, BUG-0086).

Lo decide **la firma que el suceso deja en los residuos**, por el diccionario
de la función de transferencia:

```
   en el NIVEL          en ∇                        suma en ∇
   escalón ω en T   →   UN impulso ω en T           ω
   impulso ω en T   →   DOS impulsos +ω, −ω         0
```

De ahí sale la regla, **y sale sin umbral nuevo**: se lee sobre los EXTREMOS
del episodio, que son los que la intervención tiene que explicar.

- con **d ≥ 1** los residuos viven en ∇:
  - dos extremos contiguos de signo opuesto y magnitud comparable → **1b**;
  - cualquier otra cosa → **1a**.
- con **d = 0** los residuos viven en el nivel y el diccionario se invierte:
  - un extremo solo → **1b**;
  - una racha del mismo signo → **1a**.

### Y el diccionario vale porque el motor lo cumple

No es una suposición de libro: `fue.c` resta el efecto determinista **en el
nivel** —`vtmp1[i] = DataMat[0][i] − Σν·x`— y aplica el operador no
estacionario **después**. Es decir, `step 10 2008` es un escalón en la serie,
no en la serie diferenciada. Comprobado en el motor antes de escribir la
primera línea del módulo.

### Lo único que la regla no mira

La lectura usa la diferencia **regular**. Con `D = 1`, ∇∇ₛ de un escalón deja
`+1` en T y `−1` en T+s: dos extremos **no contiguos**, que la lectura escalar
ve como dos sucesos. art tiene el mismo límite. Aquí no se calla: la ventana
lo **avisa** cuando `D > 0`, porque «no consta» no es «cuadra».

---

## 2. Los módulos

Tres piezas, y la separación de siempre.

### `lib/intervencion` — la lectura

Puerto de `art.escalera.lectura_escalar`. Aritmética pura sobre los extremos
y la `d`: ni estima, ni escribe, ni decide. Devuelve la forma, **la frase que
la justifica** —que va al informe: un dictamen que dice «escalón» sin decir
por qué no se puede revisar— y en qué peldaño de la escalera estás.

La batería prueba **las cuatro casillas**: la misma firma leída al revés según
la `d`. Es todo el contenido del módulo.

### `lib/inpdet` — la escritura

Añadir un determinista **no es añadir una línea**. Es tocar cinco sitios: el
número de deterministas, el bloque de nombres, la cuenta de omegas, su bloque
de valores y la cuenta de deltas. Y el `.inp` es **posicional**: `fue` reabre
el fichero y se posiciona contando `fgets` a pelo, y hace 115 `fscanf` sin
comprobar el retorno ni una vez. Una línea de más no da error: da un `.pre`
con nombres de determinista basura.

La regla del módulo: **lo de antes y lo de después del bloque sale byte a byte
igual**, y dentro del bloque lo que ya había tampoco se toca. De ahí la prueba
que lo vigila —añadir **cero** deterministas devuelve el fichero idéntico— que
no comprueba la copia (es trivial por construcción) sino **lo único que puede
fallar**: que los límites del bloque estén bien encontrados. Se barren los 114
ficheros del corpus del motor; los 86 que son `.inp` de fue salen idénticos.

El juez de lo que se escribe es `inp_check_fue`, la puerta del **propio
motor**: lo que el motor acepta es lo que esto acepta, por construcción y no
por parecido. Es el mismo convenio que el editor.

### `gui/atsw` — la ventana

Se abre desde el pie del gráfico de anómalos, con los sucesos ya marcados: es
donde están los extremos **con su signo**, que es de donde sale la forma. En
cualquier otro sitio habría que volver a buscarlos.

Por cada suceso: su fecha, la forma que dice el dato, la línea tal como va a
quedar en el `.inp`, y la razón. La forma **se puede cambiar**, y ése no es un
adorno: hay una razón que la ventana no puede mirar —si existe un suceso
conocido que explique otra forma— y es el único nodo cuya evidencia no está en
los datos.

### El dibujo: cómo capta la forma el suceso

Al lado de cada sugerencia va la **superposición**, que es la otra mitad de la
respuesta: la razón dice qué forma pide la firma, y el dibujo dice si esa
forma la **capta**.

```
   en gris     lo observado en el entorno del suceso
   en azul     la HUELLA que la forma dejaría en los residuos, ya escalada
   en oscuro   lo que QUEDA al quitarla — en rojo si pasa del umbral
```

La huella no es una tabla: es `(1−B)^d (1−B^s)^D` aplicado al regresor de
nivel, **y de ahí sale el diccionario**. La batería lo comprueba al revés —
calcula la huella y verifica que un escalón con d=1 deja un pico que suma 1, y
un impulso deja dos que suman 0. La regla de §1 no es una convención: es esta
cuenta.

Y tres números, que se leen **sin mirar la figura** (así sirven igual para un
informe):

| | qué dice |
|---|---|
| **escala** | cuánto hay que multiplicar la forma para que encaje |
| **R²** | qué fracción del entorno explica la forma ya escalada. Bajo con escala razonable ⇒ el problema no es la amplitud, es el **perfil** |
| **mayor resto** | el pico que **sobrevive** a quitarla. Si tras ajustar sigue habiendo un 4, la hipótesis no cubre lo que hay |

El tercero es el que decide, y es el **criterio de Treadway** para subir de
peldaño — visto aquí antes de gastar una estimación.

**Dónde no llega**, dicho en la propia ventana: la superposición **no**
distingue una forma correcta de otra que deja una cola permanente pequeña. El
R² apenas se mueve, porque la diferencia está en la ganancia a largo plazo, que
es del comportamiento futuro y no del perfil local. Eso lo dirime el contraste
ω(1)=0, que exige estimar. El dibujo descarta lo incompatible barato; el
contraste ve lo que el dibujo no puede.

Y con **d ≥ 2** el diccionario tiene otra fila —un impulso en la serie
transformada es una rampa en el nivel—, así que la ventana lo avisa.

---

## 2 bis. El peldaño 2: cuando una escalar no da

Esto faltaba, y se vio en un caso real: el incidente de **3/2022** deja **dos
extremos consecutivos**, y la sugerencia era un `step 3 2022` solo — que
explica el primero y **deja el segundo intacto**.

El error de diseño era mío y conceptual: confundí `lectura_escalar` con la
escalera. `lectura_escalar` sólo elige **entre 1a y 1b**. Quien decide **subir
de peldaño** es la escalera, y lo decide por otras razones.

### Qué es el peldaño 2

La forma general de un episodio de **L** períodos son **L+1 escalones en el
nivel**. Y en el `.inp` eso es **UNA** intervención `step` con **L+1
coeficientes ω** —la familia anidada ω(B) sobre un escalón—, **no L+1
intervenciones sueltas**. Con ganancia ω(1)=0 equivale a L impulsos de nivel.

La cuenta del fichero es uno menos que los coeficientes: `nomega = L`, y el
bloque lleva L+1 valores. Confundir las dos escribe un `.inp` que el motor lee
mal sin quejarse.

### Y L no es lo que se ve

```
L = max( duración_en_residuos − d, 1 )
```

Los residuos están diferenciados: **L impulsos en el nivel se ven como L+d
extremos**. Contar sobre los residuos pide un escalón de más por cada orden de
diferenciación.

### Qué justifica subir — y no es el AIC

**El AIC no arbitra la subida.** Compara *dentro* de un peldaño, o confirma una
subida ya justificada por otra cosa. Una escalera que se quedara con el mejor
AIC subiría siempre, porque el modelo más sofisticado casi siempre ajusta
mejor: tiene más parámetros. Eso es exactamente lo contrario de la navaja.

Lo que justifica subir son cuatro cosas, y **dos se ven sin estimar**:

| | razón | ¿la ve el GUI? |
|---|---|---|
| 1 | **Treadway**: la forma de abajo deja un vecino anómalo | **sí** — es el «mayor resto» de la superposición |
| 2 | **El episodio dura L > 1** períodos en el nivel | **sí** — es aritmética |
| 3 | **Inadecuación**: la forma de abajo no deja ruido blanco | no: exige estimar |
| 4 | **Dominio**: la lectura simple es implausible para esa clase de serie | no: exige saber la clase |

Las dos primeras son las que la ventana mira, y bastan para el caso que lo
destapó. En **3/2022** con d=1 los dos extremos contiguos dan L=1 —o sea, la
duración *no* manda—, pero al ajustar el escalón solo **sobrevive el segundo
extremo entero**: eso es Treadway, y la ventana ya lo estaba dibujando en rojo
sin sacar la conclusión.

Ahora la saca: el desplegable trae una quinta opción, «episodio — N
escalones», y **viene marcada** cuando la escalera dice que hay que subir. El
dibujo se rehace con las L+1 columnas ajustadas a la vez, así que se ve que lo
que quedaba rojo desaparece.

### Lo que sigue sin verse aquí

Las razones 3 y 4, y el contraste de ganancia ω(1)=0 que distingue un episodio
transitorio de uno permanente. Todo eso exige **estimar**, y estimar es el
editor. La ventana llega hasta donde llega la aritmética, y lo dice.

---

## 3. Derivar, nunca pisar

La intervención va en un modelo **nuevo, hijo** del que se estaba mirando. El
padre sigue estimado y con su `.out`, que es **la mitad «sin» de la
comparación**: pisarlo borraría la mitad de la pregunta.

Y se parte del **`.pre` si lo hay**, no del `.inp`: el `.pre` es ese mismo
modelo con sus estimaciones como valores iniciales, un óptimo en forma
reejecutable. Empezar desde el óptimo del padre es lo que hace que **lo que se
mueva sea la intervención y no el punto de partida**.

La razón va al manifiesto con la frase del diccionario:

> *escalón en 10/2008: dos extremos contiguos de signo opuesto que NO se
> cancelan (+3,21 y −1,02, suma +2,19 = 68 % del pico, por encima del 35 %):
> lo que no revierte es un ESCALÓN. Derivado de m03 (de su .pre, el óptimo).*

El linaje lo escribe `pr_deriva` solo. La razón es lo que dentro de seis meses
explica **por qué esta iteración existe**, y aquí se sabe.

### Dos intervenciones sobre el mismo suceso

No dan error. Dan un ω no significativo **por síntoma**, que se lee como «no
hacía falta» cuando lo que pasa es que el suceso está contado dos veces (art,
BUG-0162). Así que antes de escribir se mira qué intervenciones trae ya el
fichero, y si alguna cae en la misma fecha **se rechaza y se dice cuál**.

---

## 4. Lo que **no** hace

**No estima.** El `.inp` queda escrito y se abre en el editor, que es quien
corre `fue` y enseña la diagnosis. Un botón que escribiera *y* estimara
escondería el fichero, que es justo lo que en esta escuela no se esconde.

**No sube de peldaño.** Esto es el peldaño 1, la lectura escalar, UNA
intervención por suceso. La escalera de Ockham sigue con el peldaño 2 —el
episodio entero, L+1 escalones— y el 3 —la FLT con denominador, cuando la
respuesta decae—. Y **subir no lo justifica el AIC**: lo justifica que la
forma de abajo deje un vecino anómalo o no deje ruido blanco. Eso se ve
**reestimando**, que es otro sitio.

Lo obvio primero.

---

## 5. Orden

| | qué | coste |
|---|---|---|
| 1 | `lib/intervencion`: la lectura escalar, con su batería | hecho |
| 2 | `lib/inpdet`: el bloque de deterministas, barrido sobre el corpus | hecho |
| 3 | la ventana «Sugerir intervención…» y el modelo derivado | hecho |
| 3b | la superposición: la huella de la forma sobre lo observado, y los tres números | hecho |
| 4 | el peldaño 2: el episodio entero como L+1 escalones | hecho |
| 5 | la comprobación de Treadway —¿deja la forma un vecino anómalo?— sobre el `.out` del hijo | pendiente |

El 5 es el que cierra el círculo: es leer el `.out` del modelo derivado y
decir si la forma elegida **resolvió** el suceso o lo dejó a medias. Y eso ya
es `lib/outfile` más `lib/anomalos`, que están.

# Relevo: drvarma en C tras el plan de noviembre de 2026

Para quien retome el desarrollo de atsw-gui (sim_gui, atsw-cli, el asistente)
sobre el motor de drvarma. Qué ofrece hoy el motor, qué cambió de lo que la
GUI ya usaba y qué hay de nuevo en su salida.

- Las reglas para tocar un motor: `docs/MOTORES.md`.
- El diseño del motor: `engines/drvarma/docs/DESIGN-v5-ladder.md`.
- Las opciones, una a una: `engines/drvarma/docs/USER_GUIDE.md`.
- El porte (drvarma-python) hace lo mismo y está homologado contra el commit
  `1bc1862edf` de este repositorio (su `docs/HOMOLOGATION.md`).

## 1. El principio que ordena todo

**Los univariantes siempre primero, en las dos rutas.** Dan la vara de medir
y las condiciones iniciales; la puerta de la escalera es eso. Pasada la
puerta, las rutas solo difieren en qué es lo propio de cada serie y cómo se
siembra:

- **ruta U** (por defecto): cada serie conserva su ARMA univariante y se
  añade la relación;
- **ruta V** (`-route v`, el vector primero, Tiao y Box): el ARMA regular de
  cada serie se sustituye por un AR(p)/MA(q) libre, y sus factores
  estacionales multiplican la fila entera.

La «ruta V» de antes —especificaciones sin univariante, con lo propio a
cero— ya no es la ruta V. El motor las sigue aceptando y la puerta dice que
no son óptimos.

## 2. Lo que el motor ofrece

| para | opción |
|---|---|
| la ruta V por la puerta | `-route v` |
| qué deterministas se estiman en el conjunto | `-det LISTA`: `all`, una clase (`seasonal`, `calendar`, `interv`, `trend`), `SERIE:clase`, o el nombre de un coeficiente (`"w0_IPC[step 8 1990]"`) |
| de dónde salen los que quedan fijos | `-detbase file\|gate` |
| un shock común a varias series | `-det` con el nombre del término en cada una |
| la forma echelon con índices dados | `-echelon n1,n2,…` (implica la ruta V; órdenes cruzados `0 0`) |
| elegir los índices | `-echelon search [-nmax N] [-criterion SC\|HQ]`: da la tabla y para |
| el contraste del factor de fila | `-factortest` (ruta V y forma echelon) |
| simplificar lo propio | `-zero "phi_X[B^k]"`, `"theta_X[B^k]"` (ruta V) |
| el arranque | `-start zero\|ols\|echelon` |

sim_gui y atsw-cli no usan todavía ninguna. Siguen por la ruta U, y sus
baterías pasan con el motor nuevo.

Para qué sirve cada cosa, en corto:
- **Un VAR** por la ruta V funciona sin más: los tres arranques llegan al
  mismo óptimo.
- **Un VARMA con MA** lleno no está identificado y acaba en el muro del MA
  desde cualquier arranque. El camino es `-echelon search`, y después
  `-echelon` con los índices que elige. No lo arregla ningún arranque.
- **El factor de fila es una restricción**: si alguna serie tiene AR o MA
  estacional y se usa la ruta V, pásese `-factortest`.

## 3. Lo que cambió de lo que la GUI ya usaba (ruta U)

Tres comportamientos por defecto. Los números que la GUI enseñaba pueden
moverse.

1. **El arranque es `ols` cuando q = 0** (antes, cero). Mismo óptimo; los
   errores estándar cambian menos de un 0,6 % y la búsqueda termina por el
   gradiente. Saca del estancamiento a los VAR con escalas dispares (un tipo
   en puntos básicos junto a precios en %). Con MA sigue en cero.
   `-start zero` da lo de antes.
2. **Una serie que la ventana común recorta tiene sus deterministas
   reestimados en la puerta**, y fijos después (antes se quedaban en los del
   fichero, de la muestra larga). Puede ser grande: en m6, θ de EU pasa de
   0,9327 a 0,9073 y la verosimilitud de −1752,52 a −1725,49.
   `-detbase file` da lo de antes. Las series sin recortar no cambian.
3. **`-split`** siembra la media en la media muestral (antes, en cero) y
   escribe en la cabecera el nombre del origen sin su ruta.

Una prueba de atsw-cli y otra del MCP fijaban un LR a cuatro decimales y hubo
que darles tolerancia (el punto de parada depende del arranque): si se fija
un número del motor en una prueba de la GUI, con tolerancia.

## 4. Lo nuevo en la salida (`.out`)

El lector (`lib/outvarma`) sigue leyendo lo de siempre; estas líneas y
bloques son nuevos y hoy los ignora.

| dónde | texto |
|---|---|
| cabecera | `Model : VARMA(p,q), route V: …` o `Model : echelon form, Kronecker indices 1,0 (route V; …)` |
| cabecera | `Deterministics : re-estimated: interv (12 of 23); the rest fixed at the .pre` / `fixed at the gate's values (11 estimated there, series by series)`. Las dos de siempre (`re-estimated`, `fixed at the .pre`) no cambian |
| cabecera | `Univariate ARMA : replaced by the vector's (route V)` |
| cabecera | `Zero coefficients:` lista también los propios |
| tras la puerta | el bloque `ROUTE V: THE VECTOR FIRST` (o `THE ECHELON FORM (route V), Kronecker indices …`): por serie, el ARMA regular del univariante y los grados del factor de fila; y las notas `… has a regular AR(3): this model, with p = 1, does not contain it` |
| antes de estimar | `Start of the cross terms: ols` / `ols, the whole matrix` / `echelon (AR x 0.388^k, …)` / `zero (the ols start was not admissible)` |
| la tabla | nombres nuevos: `AR0[WTI<-IPC]` (el término contemporáneo de la forma echelon), `w0_IPC[step 8 1990]` (un determinista estimado) |
| el LR | en la ruta V el encabezado dice `against the same model without cross terms`; la línea `LR = …` es la de siempre |
| al final | el bloque `THE ROW FACTOR (-factortest)`, con su línea `row factor: LR = …  df = …  p-value = …` |
| `-echelon search` | el bloque `THE ECHELON INDEX SEARCH`, la tabla de candidatos y `Chosen by SC: 1,0 (by HQ: 1,1)`. No hay modelo estimado en ese `.out` |

## 5. Las baterías

- `make check` en la raíz antes de fusionar cualquier cambio del motor: lo
  conducen `gui/sim`, `gui/atsw-cli`, `lib/partida`, `lib/outvarma` y las
  pruebas del MCP.
- `engines/drvarma/tests/rutas/rutas.sh` (veinte minutos, fuera de `make
  check` y de CI): antes y después de todo cambio en el motor.
- CI en verde en las tres plataformas antes de fusionar.

## 6. Lo que queda abierto en el motor

- `cross="residual"` (la forma 3.22 de Jenkins y Alavi) existe solo en el
  porte: por traer al C.
- Las pasadas siguientes de la búsqueda de índices, con el factor de fila
  reestimado en el conjunto: solo en el porte.
- La batería de las rutas tiene 23 abiertos: los VARMA llenos con MA (21) y,
  desde el 22-nov, el VARMA(1,1) por la ruta V de los sistemas con GS2 y GS3.
  Ahí se vio que el porte no acababa donde el C. La causa estaba en el porte
  (invertía el MA(1) propio fuera de la región invertible, como el cast de
  fue; este motor no lo hace) y está corregida en drvarma-python (BUG-0030):
  los 64 casos monetarios coinciden en verosimilitud e iteraciones. No se
  cierran con un arranque; son el caso de uso de la forma echelon.
- Un comentario de `src/drvarma.c` dice todavía «unchanged since 0.4.1».

## 7. Estado y pendiente a 23-nov-2026

**Hecho desde esta nota.** La batería de las rutas cubre GS1, GS2, GS3 y FF
por las dos rutas (#221). El porte (drvarma-python) es fiel a este motor
también donde la búsqueda acaba en el muro de invertibilidad (su BUG-0030,
#34): 64 casos monetarios, 16 pares con MA por la ruta V, 24 por la ruta U y
la forma echelon de la harina, iguales en verosimilitud al sexto decimal y
en iteraciones. sima opera con la ruta V del motor (`load_pre(route="V")`).

**La escalera no toma el invertible de un MA.** fue sí lo hace, en su cast:
un MA(1) con |θ| > 1 pasa a 1/θ, y por eso fue nunca estima un MA por encima
de 1. Es una regla del univariante, donde θ y 1/θ son el mismo modelo. En un
sistema no lo son, salvo con series independientes: lo que se juzga es el
operador MA entero, y este motor rechaza el punto si no es invertible, como
Mauricio (1995, sec. 3). El porte la había heredado al reutilizar el cast de
fue; ya no.

**Pendiente en el motor, por orden:**

1. *Avisar cuando un MA propio acaba fuera de (−1, 1)* (aprobado el 23-nov).
   Puede pasar: visón y rata almizclera, VARMA(2,1), ruta U, arranque cero,
   «convergió» con θ_MUSKRAT = 1,5483 y logL −551,7158; desde ols, −545,5623
   en el muro. El operador del sistema es invertible, pero se lee como un
   modelo univariante no invertible dado por convergido. La salida debe
   decirlo; el porte y sima, con ella. No cambia ningún número.
2. *Estudio aparte:* si la escalera debe impedir que un factor MA(1) que
   viene de un modelo univariante salga del círculo. Cambiaría resultados:
   estudio, C con toda la batería, hermanos y porte (`docs/MOTORES.md`).
3. La referencia de la batería de las rutas no se ha regenerado entera: los
   hechos de GS1–3 por la V se añadieron de una pasada filtrada.
4. Lo de la sección 6 que sigue abierto: `cross="residual"` y las pasadas
   siguientes de la búsqueda, solo en el porte; el comentario de `drvarma.c`.

**Pendiente fuera del motor, que lo condiciona:**

- *Publicación* (parada): fue 0.1.23 primero (el porte necesita su
  `cast_us_py(flip=False)`), luego drvarma 0.4.0, drvec, sima y atsw.
- *El porte sin su núcleo compilado* (Python puro) es más de cien veces más
  lento y no se puede homologar en el muro: decidir si deja de ser una caída
  silenciosa.
- *sima:* retirar la ruta V antigua y rehacer los tutoriales de Tiao y Box y
  de Lütkepohl y Poskitt (el segundo cambia de números); montar los
  univariantes de la ruta V con art en autónomo; el rodaje con un agente.
- *fue:* comprobar si su porte toma el invertible del MA(1) estacional como
  hace `fue.c`.

# Estudio: la ruta V (Tiao y Box) en la madre y en sim_gui

**Estado: estudio para discutir, no un plan aprobado.** 19-nov-2026. Pedido
por el analista para el vídeo «SIM 2: Tiao y Box», y para decidir si cada
escuela necesita su propia ventana. Parte de `DISENO-sim.md` (§0.3, §3.2, §8:
la ruta V en la segunda versión) y lo aterriza con lo que se ha medido.

Nombres: la **ruta V** es Tiao y Box (1981), «el vector primero». **Box y Tiao
(1977)** es el análisis canónico, que en sima es un paso opcional (N1b) de las
dos rutas, útil sobre todo con series diferenciadas.

---

## 0. Resumen

1. **El motor ya sirve.** La escalera de drvarma C estima la ruta V con un
   `.inp` de fue por serie (solo su transformación y un AR(p)/MA(q) propio
   libre): con el horno de gas da lo mismo que sima, logL −2638.762693, los
   parámetros a seis decimales y la LR 211.758 con 13 g.l. Lo único que fallaba
   era `lib/prewhiten` con datos negativos (BUG-0001, arreglado en #196).
2. **Lo que falta es todo lo de alrededor**: el alta del sistema desde las
   series, las especificaciones que dependen de p y q, la identificación
   sobre el vector (M(l), R_k, S_k), la vara de medir de N5 y los textos que
   hoy dan por hecho un modelo univariante en cada fila.
3. **Una ventana por escuela, no; un carril por caso, sí** (§3). Un solo
   sim_gui donde el caso lleva su ruta y la ruta elige la página de
   identificación y los textos; el resto (estimación, diagnosis, estructura,
   previsión, IRF) se comparte. Es como lo hace sima (un protocolo, la ruta
   como atributo de la sesión) y como sim_gui ya trata el VEC (la página
   «R · Rango» solo aparece cuando toca). La forma echelon sería otro carril.
4. **Seis fases** (§7). Las dos primeras bastan para estimar por la ruta V
   desde la madre; el vídeo necesita además la M(l) en C (fase 3) y la vara
   (fase 4).

---

## 1. Cómo lo hace sima (Python)

`sima-python/src/sima/mcp_server.py` (M), `raw.py` (R), `evidence.py` (E);
la identificación es `drvarma-python/identification_mv.py` (IM).

| nodo | herramienta | en la ruta V |
|---|---|---|
| N0r | `load_data` | la tabla en niveles; igual en las dos rutas |
| N0c | `characterize` | λ, d, estacionalidad de cada serie con el motor de art; λ solo 0 o 1, y 1 obligatorio con datos ≤ 0; el analista corrige con `set` |
| N0v | `write_specs` | `<S>_v.inp` por serie: transformación, armónicos libres a 0 si los hay, la media si d = D = 0 **sembrada con la media muestral** (sembrarla con 0 llevaba el AR a una raíz unitaria) y **sin ARMA**. Fija `route = "V"` en la sesión |
| N1 | `run_gate` | el mismo cálculo; el texto dice que la puerta **solo certifica el reparto**: la diagonal es ruido blanco (o paseo aleatorio con d = 1), no un modelo que batir |
| N1b | `canonical_analysis` | sobre los niveles transformados (sin diferencias regulares); se ofrece si hay ≥ 2 series diferenciadas. En el gas no se usó (d = 0) |
| N2 | `identify_matrices` | R_k(w), S_k (Yule-Walker), S_k(q) de Alavi y la **autorregresión por pasos con M(l)**; sin el método 2 (no hay univariantes que preblanquear). `identify_cross` no tiene rama V |
| N3 | `estimate` | genera a demanda `<S>_v_p{p}q{q}[_z…].inp` con el AR(p)/MA(q) propio libre; la LR es «contra el mismo modelo sin términos cruzados» (su BUG-0002), no contra los univariantes. No escribe `.pre` del sistema |
| N4s | `simplify` | también los coeficientes **propios** `phi_S[B^k]`, `theta_S[B^k]`; los ceros propios van como filas `0.000000  0` en un fichero con sufijo `_zar4.5.6` |
| N4t | `structure` | igual que en U; si es una red, ofrece mtram con los `.pre` de la vara |
| N5 | `evaluate` | la **vara de medir**: los univariantes del analista si los da; si no, los construye la primera vez (`build_univariate`, el primer candidato de art estimado con fue: `<S>_u.inp/.pre`, `yardstick.json`). Compara RMSE por serie y horizonte |
| N6 | `forecast`, IRF, FEVD | sin rama V |

**La M(l)** (IM:234, `stepwise_ar`): para l = 1…L, con L = max(1, min(K,
n/(3m))), un AR(l) por MCO **con constante** sobre la muestra común t = L+1…n;
M(l) = −(N − ½ − l·m)·ln(|S(l)|/|S(l−1)|), N = n − L − 1, χ²(m²). La tabla:
los símbolos +/−/. de Φ_l (|t| > 2), M(l), p, y la diagonal de Σ(l). Son
unas 75 líneas; R_k, S_k y S_k(q), otras 60. Solo usan numpy y la cola de la
χ²; el canónico, además, un problema de autovalores generalizado.

**Las cifras del gas** (`examples/tiao_box/out`), que serán la prueba del C:

- M(l), l = 1…11: 1649.7, 665.1, 31.7, 22.5, 5.6, 12.8, 1.8, 8.0, 3.7, 1.0,
  4.0 (4 g.l.): orden 6 con un hueco en 5; S_k corta en 4.
- VAR(6): logL −2638.762693, 28 parámetros, LR 211.758 (13 g.l.), Hosking
  Q(8) = 10.16.
- Estructura: CO2 → gas, LR 6.01 (6 g.l., p = .42): **sin realimentación**;
  el gas entra en el CO2 con un retardo de 3.
- El AR(6) restringido de Tiao y Box (13 ceros, propios y cruzados): logL
  −2647.479824; el AR(3) del gas 1.968, −1.364, 0.339; el retardo
  AR3[CO2<-gas] = −0.534.
- Evaluación contra los univariantes (estimando con 200, H = 3): gana en las
  6 casillas; en el CO2 el RMSE baja a 0.61–0.76 del univariante.

**Tres cosas de sima que conviene saber** (para su propia sesión, no se tocan
desde aquí): N0v no está en la lista de nodos del guion y sale sin etiqueta;
el guion de N2 anota los cortes fuera de la diagonal aunque la ruta sea V; y
las salidas guardadas del ejemplo (`out/08`, `10`, `12`, `15`) son de
versiones anteriores, con la etiqueta vieja de la LR.

---

## 2. Qué hay hoy en C y dónde choca la ruta V

**El alta.** «SIM ▸ Nuevo sistema…» (`gui/atsw/src/casos_gui.c`) es un diálogo
con una fila por serie y un desplegable de modelos; la ruta U es la entrada
`@partida`. Al aceptar: el `.pre` de cada entrada, la ventana común, el
**sha256 del `.pre`** y `pr_caso_add`. En el manifiesto (`lib/proyecto`),
`PrEntrada` es {serie, modelo, sha} y `entradas_validas` exige que cada
entrada sea un modelo de la lista US. **No hay marca de ruta.** atsw-cli
`caso-nuevo` exige el `.pre` («un caso entra con su .pre»).

**sim_gui ya lee `.inp`** (`lib/sistema`, `SisSerie.es_pre`), y las órdenes de
drvarma se arman en `sis_orden_puerta` y `sis_orden_modelo`. Pero:

| sitio | qué supone | en la ruta V |
|---|---|---|
| `puerta.c:242-258, 306-310` | «reproduce los univariantes»; «no es un óptimo… estímalo en fue» | todas las series saldrían «no es un óptimo» con un consejo equivocado |
| `identifica.c:239, 278-285` | la CCF de los residuos univariantes | en V hace falta la M(l) y las R_k del vector |
| `modelo.c`, `estructura.c` | los ceros son cruzados (`-zero` rechaza los propios en `escalera.c:1667-1700`) | en V se simplifican también los propios |
| `estima.c:379`, `diagnosis.c:64-73`, `escalera.c:1964` | «LR contra los univariantes» | es contra el mismo modelo sin cruzados |
| `prevision.c:154-167, 903-908` | el patrón es el diagonal de los mismos ficheros | en V el patrón tiene que salir de otros ficheros: los univariantes |
| `pagina.c:5-8` | la diagonal es el modelo de fue de la serie | en V es una fila del sistema |
| `casos_gui.c:252` (`clase_caso`) | VARMA o VEC | no distingue U de V |
| `sis.py` (asistente) | `load_pre` de modelos estimados | no hay `write_specs` |

Y una tensión de fondo: **en V las especificaciones dependen de p y q**, y
hoy cambiar un fichero de entrada cambia su sha y obliga a otro caso.

---

## 3. ¿Una ventana por escuela?

| | qué es | a favor | en contra |
|---|---|---|---|
| **A. Una sim_gui que se adapta** | el caso lleva su ruta; las páginas cambian textos | un solo programa | las páginas se llenan de `if (ruta == V)` |
| **B. Una ventana por escuela** (`sim_ja`, `sim_tb`, …) | cada escuela su programa | cada vídeo, su ventana | siete páginas duplicadas, o una librería de páginas que ya es la opción C; el analista tiene que saber cuál abrir |
| **C. Un programa, un carril por caso** | el caso lleva su ruta; la ruta elige **qué módulo** ocupa cada página que difiere, y el título y la presentación dicen la escuela | lo que difiere se escribe aparte y lo común se comparte; cada escuela tiene su recorrido y su vídeo | hay que separar bien la interfaz entre páginas |

**Cuánto difiere cada página entre U y V:**

| página | U (Jenkins y Alavi) | V (Tiao y Box) | ¿módulo propio? |
|---|---|---|---|
| 1 · Series | series y sus modelos | series y su transformación | no: textos |
| 2 · Puerta | la puerta es una prueba: reproduce los univariantes | solo certifica el reparto | no: textos |
| 3 · Identificación | CCF de residuos univariantes, enlaces | M(l), R_k, S_k (y el canónico) | **sí** |
| 4 · Modelo | p, q cruzados, enlaces, ceros cruzados | p, q del vector, ceros propios y cruzados | en parte |
| 5 · Estimación | igual | igual | no |
| 6 · Diagnosis | LR contra los univariantes | LR contra el modelo sin cruzados; simplificar también los propios | no: textos y una opción |
| 7 · Previsión | vara = el diagonal | vara = los univariantes, otros ficheros | no: de dónde sale la vara |

Solo la identificación es otra cosa. **Recomendación: C.** Es la decisión de
sima (un protocolo, la ruta como atributo) y la de sim_gui con el VEC (la
página R aparece solo con series I(1)). La ventana dice la escuela en el
título («SIM · Tiao y Box: C3») y en la presentación de la página 3, así que
cada vídeo enseña su carril. La forma echelon, cuando haya motor, sería un
tercer carril: su identificación (los índices de Kronecker) y su página 4
(índices en vez de p y q).

---

## 4. El diseño propuesto para la ruta V

### 4.1 El caso: la ruta en el manifiesto y las especificaciones en el caso

- `PrCaso` gana un campo **`ruta`** (`U` por defecto; `V`). Un manifiesto
  viejo, sin el campo, es U: no cambia nada de lo que hay.
- En la ruta V, cada `PrEntrada` es **{serie, muestra, especificación, sha}**:
  no apunta a un modelo de la lista US, sino a `_casos/Cn/espec/<S>.inp`, la
  **especificación base** (transformación, armónicos, media sembrada con la
  media muestral, **sin ARMA**). El sha es el de ese fichero. Es lo que pide
  `DISENO-sim.md` §8: «los ficheros viven en el sistema, no en la lista US».
  `entradas_validas` gana la rama V.
- **Las especificaciones de cada corrida** (con su AR(p)/MA(q) propio y sus
  ceros propios) las escribe la corrida en `work/<C>_<cNN>_<S>.inp`, a partir
  de la base. Así **cambiar p no crea otro caso**: es otra corrida, con su
  padre en el linaje, como hoy. Y los ceros propios no necesitan `-zero`: van
  como coeficientes fijos (`0  0`) en esos ficheros, como hace sima, y el
  motor no se toca. Comprobado con el gas: φ₄, φ₅ y φ₆ fijos a 0 en su
  fichero, la escalera estima 25 parámetros en vez de 28 y los deja en 0.

### 4.2 La puerta, con la base

La puerta se pasa con las **especificaciones base, sin ARMA**: la diagonal es
ruido blanco (o paseo aleatorio), la puerta certifica el reparto, y sus
residuos son las series estacionarias centradas, que es lo que lee la
identificación. Es la elección de sima, y resuelve la tensión que vio el
estudio de sim_gui: si la puerta se pasara con el AR(p) propio, las CCF serían
de los residuos de un AR(p) que nadie ha elegido. La página 2 lo dice: «En la
ruta V la puerta no es una prueba de modelos: comprueba que el sistema lee
cada serie como fue». Que salga «no es un óptimo» en los armónicos sembrados
a 0 es normal y no lleva el consejo de «estímalo en fue».

### 4.3 La identificación en C (página 3 del carril V)

Una librería nueva, **`lib/idvector`**, con el oráculo en drvarma-python
(`identification_mv.py`):

- R_k(w) con los errores de Bartlett y los símbolos +/−/.;
- S_k por Yule-Walker y S_k(q) de Alavi (las matrices de correlación parcial
  de Jenkins y Alavi: sirven también al carril U, que hoy solo tiene la CCF);
- la autorregresión por pasos con M(l), la tabla de Tiao y Box;
- (fase 6) el análisis canónico, con `gsl_eigen_gensymm`; drvarma ya enlaza
  GSL.

Prueba: las cifras del gas (M(l) = 1649.7, 665.1, 31.7…) y las de los
ejemplos de sima, contra el porte. La página propone p (el último M(l)
significativo, y el corte de S_k si es distinto) y pasa a la página 4, como
hoy «Al modelo». Sin ESCC (Tiao y Tsay 1983) ni modelos de componentes
escalares: tampoco están en sima.

### 4.4 La vara de medir (N5)

En V el patrón son **modelos univariantes** de cada serie, con la misma
transformación que su especificación. Por orden:

1. el **elegido** de la lista US de la serie, si su λ, d y D coinciden con la
   especificación;
2. si no, un **modelo de partida** de `lib/partida` (`pt_construye`, la ruta U
   de hoy), marcado «sin revisar», que queda en la lista US como cualquier
   otro.

Se fija al dar de alta el caso (el diálogo lo enseña: «la vara de C3:
CO2/m02, gas/m01») y queda en el manifiesto del caso, con sus sha. La página
7 compara contra ellos con la misma ventana.

### 4.5 El alta en la madre

«SIM ▸ Nuevo sistema…» gana la elección de ruta, que el diseño llamaba la
tercera pestaña: «Con sus modelos (Jenkins y Alavi)» / «El vector primero
(Tiao y Box)». En la V, por serie: λ, d, D o armónicos, propuestos por
`lib/partida` (`pt_lambda`, `pt_d`, los armónicos) y editables, como el
`set` de sima; con datos ≤ 0, λ = 1 sin opción. Y la vara (§4.4).

### 4.6 Textos

La LR en V: «contra el mismo modelo sin términos cruzados»; los univariantes
llegan en la previsión. Se cambia en sim_gui (`estima.c`, `diagnosis.c`), no
en el motor: el `.out` de drvarma sigue diciendo «against the univariates» en
las dos rutas, y la ventana lo traduce según la ruta. Si se quiere arreglar
también en el `.out`, es un cambio de motor con su BUG, aparte.

### 4.7 atsw-cli y el asistente

- `atsw-cli proyecto caso-nuevo --ruta V SERIE... [--lambda S=0] [--d S=1]`
  escribe las bases y la vara; `caso-puerta`, `caso-estima` (que escribe las
  especificaciones de la corrida), `caso-simplifica` (con los propios en V) y
  `caso-estructura` aceptan el caso V.
- `atsw-sis` gana `write_specs` (el nombre de sima) y, con la fase 3,
  `identify_matrices`; las instrucciones, el carril V. Las herramientas del
  asistente son las mismas órdenes que la ventana.

---

## 5. Lo que no cambia

- **El motor.** drvarma C no se toca: la ruta V cabe en la escalera tal cual
  (§0). Los ceros propios van en los ficheros, no en `-zero`.
- La estimación, la diagnosis, la estructura, la previsión, la IRF y el
  linaje de corridas.
- Los casos U que ya existen.

---

## 6. Decisiones que pide

1. **Carril por caso (C) frente a una ventana por escuela (B).** El estudio
   recomienda C.
2. **La puerta con la base sin ARMA** (§4.2), como sima.
3. **Las especificaciones de cada corrida en `work/`**, para que cambiar p no
   cree otro caso (§4.1).
4. **La vara fijada al dar de alta**, del elegido o de un modelo de partida
   (§4.4); sima la construye perezosamente en N5.
5. **La M(l), R_k y S_k en C** (`lib/idvector`), con drvarma-python de
   oráculo. Las S_k también para el carril U.
6. **El canónico, aparte** (fase 6): el vídeo del gas no lo necesita (d = 0).

---

## 7. Fases, cada una en su PR

| fase | qué | para |
|---|---|---|
| 1 | `lib/proyecto`: el campo `ruta`, las entradas V y sus sha; `lib/partida`: la especificación base; atsw-cli `caso-nuevo --ruta V`, `caso-puerta` y `caso-estima` con las especificaciones por corrida. Prueba: el gas, logL −2638.762693 | estimar por la ruta V sin ventana |
| 2 | La madre: la elección de ruta en «Nuevo sistema…» y la clase del caso; sim_gui: el carril (título, puerta, textos de la LR, simplificar los propios, Ver modelo) | estimar por la ruta V en las ventanas |
| 3 | `lib/idvector` (R_k, S_k, M(l)) y la página 3 del carril V; las S_k también en el U | identificar por la ruta V |
| 4 | La vara: el alta la fija, la página 7 compara contra ella | evaluar por la ruta V |
| 5 | `atsw-sis`: `write_specs`, `identify_matrices`, el carril V en las instrucciones | el asistente |
| 6 | El análisis canónico (Box y Tiao 1977) | series diferenciadas |

El vídeo «SIM 2: Tiao y Box» se graba al acabar la fase 4, con el horno de
gas, siguiendo el ejemplo `tiao_box` de sima: las especificaciones, la puerta,
la M(l) (orden 6), el VAR(6), la estructura (el gas entra en el CO2 con un
retardo de 3, sin realimentación), el AR(6) restringido y la evaluación contra
los univariantes. Los datos del gas, la serie J de Box y Jenkins, tendrían
que entrar en los ejemplos de ATSW (Ayuda ▸ Ejemplos), como los de Jenkins y
Alavi.

# La madre por dentro: botones, menús y la escalera de modelos

Estudio del 18-oct-2026, a petición del analista:

> *«Lo que no veo en la madre es menús, ahora que tenemos. También los
> botones se refieren a los GUI de los programas; deberían apuntar al modelo
> que manejan, un poco como lo hacía TASTE. fue maneja modelos univariantes,
> US en los nemotécnicos de Box y Jenkins. […] En drtran tenemos algo más
> general y sería MT, de Multivariate Transfer. El botón de drtran está en
> una esquina de forma desordenada. Quizás deberíamos repensar cómo está
> organizada la madre respecto a sus botones y menús.»*

Es un estudio con propuesta y plan, **sin código**. **El analista ya ha
decidido todas las preguntas** (§8), la última después de ver la maqueta
(<https://claude.ai/artifact/9rHr1juao4k6LRUF2yU1Hs>), y lo que sigue está
escrito con esas decisiones. Continúa
`ESTUDIO-taste-interfaz.md`, que ya concluyó que la madre es superior a TASTE
en lo que importa, y lo que se toma de TASTE aquí es una sola idea: **el menú
se organiza por la clase de modelo**. Las capturas son las del manual
(`manual/img/`), anteriores al #57: les falta el botón «Cuaderno…», que no
cambia nada de lo que se discute.

---

## 0. El veredicto, primero

**El analista tiene razón en los dos puntos, y el del botón de drtran es peor
de lo que parece.**

1. **La madre ha crecido a ~55 acciones repartidas en cinco sitios** (§1), y
   sólo una parte se ve. Lo que no está en un botón sólo se encuentra con el
   botón derecho, y quien no lo sabe no lo encuentra. Una barra de menús es la
   forma estándar de que **todo** esté en un sitio que se recorre.
2. **Los botones nombran programas** (`→ fue`, `→ fug`, `drtran`, «Abrir en
   drtran», «Prever con fuf»), y el analista piensa en **modelos y pasos**:
   identificar, estimar, diagnosticar, prever un US o un MT. El programa es
   un detalle de implementación que tiene que estar a la vista (los errores y
   los ficheros son suyos) pero en segundo plano.
3. **El botón `drtran` de la esquina es el único camino a MT que NO deja
   rastro.** Lanza `drtran_gui --proyecto` **sin caso**, y sin caso drtran
   escribe en la caché con nombres fijos (`modelo.out`, `red.dag`…) que se
   pisan en cada estimación (`gui/drtran/src/proyecto_gui.c`, «sin caso: la
   caché y el nombre FIJO de siempre»). Es el botón más visible de MT, y es el
   que produjo los modelos «viejos» que hoy hay que pasar por «Convertir en
   casos…». **El camino bueno (el caso) está escondido detrás de él.**

La propuesta en una línea: **una barra de menús `Proyecto · Datos · US · MT ·
VARMA · VECM · Ayuda`, construida con las mismas funciones que los menús contextuales; los
botones se quedan pegados a lo que gobiernan y se llaman por el paso
(«Estimar»), no por el programa; el botón de la esquina desaparece.** Tres
PR (§6), y VARMA y VECM llegan con sus propios hitos.

---

## 1. Inventario: lo que hay hoy y dónde

![La madre con una serie marcada](manual/img/taller-m01.png)

### 1.1 Los cinco sitios

| sitio | qué hay | sobre qué actúa |
|---|---|---|
| **Fila de arriba** | Abrir… (Ctrl+O) · Nuevo… (Ctrl+N) · Proyecto… · Linaje… · Cuaderno… · Datos… · *[etiqueta del proyecto]* · **drtran** | el proyecto; drtran, sobre nada |
| **Fila de la rejilla** | Modelo nuevo · Iterar · Elegir · Razón… │ → fug · → fue (Ctrl+E) | la serie y el modelo marcados |
| **Menú de la serie** (botón derecho o tecla de menú) | Identificación con fug · Especificar el primer modelo / Estimar *mNN* en fue · Otro modelo, desde los datos │ Modelo nuevo, en otra muestra ▸ │ Editar la serie… │ Media – desviación típica · Serie y ACF / PACF… · Identificar desde los datos… | la serie marcada |
| **Menú del modelo** (botón derecho, tecla de menú) | Abrir *mNN* en fue / Especificar un modelo con fue · Editar el .inp… · Ver modelo… · Diagnosis… · Orden de integración… · Estacionalidad (MEG)… · Ganancia… · Anómalos… · Identificar los residuos… · Prever con fuf… · Iterar: seguir desde su óptimo · Modelo nuevo, desde los datos · En otra muestra ▸ │ Borrar… (Supr) | el modelo marcado |
| **Vista del caso** (sustituye a la rejilla cuando se marca un caso) | Abrir en drtran │ Elegir · Razón… · Borrar modelo… │ Derivar caso… · Razón del caso… · Borrar caso… | el caso y su modelo marcado |

Y sueltos:
- el **«+»** de las hojas (muestra nueva);
- **«Nuevo caso…»** encima de la lista de casos;
- **«Convertir en casos…»** abajo, sólo si hay modelos viejos de drtran;
- el **menú del modelo del caso**: Ver modelo… / Ver la ecuación de… ·
  Abrir *cNN* en drtran · Devolver los univariantes…;
- **«Identificar…»** al pie del vistazo «Serie y ACF/PACF».

Los gestos: doble clic o Intro en un modelo (→ fue), y en un modelo del caso
(→ drtran). Las teclas del #57: Ctrl+O, Ctrl+N, Ctrl+E, Ctrl+R, Ctrl+Q,
la tecla de menú (o Mayús+F10) y Supr.

Las ventanas que se lanzan: **fue_gui** (estimar US), **gtk_fmg** (fug,
identificación), **fuf** por debajo de «Prever», **drtran_gui** (MT), y
dentro de la madre las ventanas de `lib/analisis` y los vistazos. drvarma_gui
existe (`engines/drvarma/gui`) pero la madre no lo lanza y no acepta
`--proyecto`; drvec no tiene GUI.

### 1.2 Lo que falla

**Duplicados que no dicen lo mismo.** La misma acción tiene hasta cinco
puertas y tres nombres:

| acción | puertas | nombres |
|---|---|---|
| estimar en fue | botón, menú de serie, menú de modelo, doble clic/Intro, Ctrl+E | «→ fue», «Estimar m01 en fue», «Abrir m01 en fue», «Especificar el primer modelo con fue», «Especificar un modelo con fue» |
| empezar desde los datos | botón, menú de serie, menú de modelo | «Modelo nuevo», «Otro modelo, desde los datos», «Modelo nuevo, desde los datos» |
| fug | botón, menú de serie | «→ fug», «Identificación con fug» |
| estimar en drtran | botón esquina, botón del caso, menú del caso, doble clic | «drtran», «Abrir en drtran», «Abrir c01 en drtran» |

**Huecos.** Al revés, hay acciones que viven en un solo sitio y no están
donde se las busca:
- **Elegir** y **Razón…** no están en el menú del modelo, que es justo donde
  se mira el modelo antes de elegirlo.
- **Borrar** un modelo de serie no tiene botón (sólo el menú y Supr); el del
  caso sí lo tiene. Esta asimetría es correcta, porque borrar no debe ser un
  botón al lado de Elegir, pero entonces el caso está mal.
- **La muestra nueva** es sólo el «+», un botón sin texto en la esquina de
  las pestañas.
- **Editar la serie** está sólo en el menú contextual.

**Nombres de programa donde se esperaba un verbo.** Ya se fijó la regla en
`DISENO-interfaz.md` §2: el botón es escueto y el porqué vive en el globo. La
madre la cumple en la forma, pero con el sujeto equivocado: «→ fue» es
escueto, pero dice *a dónde*, no *qué*. Para quien llega del manual, «→ fug»
y «→ fue» se distinguen por una letra.

**El orden no es el del método.** En la fila de arriba van primero el
proyecto, luego el linaje, el cuaderno y los datos, y al final del todo
drtran, separado de lo demás por la etiqueta del proyecto que ocupa el ancho.
Nada dice que hay una escalera: US → MT → VARMA.

**El botón de la esquina** (§0.3): visible, mal sitio y mal camino.

---

## 2. La nomenclatura: por clase de modelo

### 2.1 De dónde viene

Box y Jenkins distinguen el **modelo univariante estocástico** del **modelo
de función de transferencia**. TASTE lo llevó a la interfaz: su barra de menús
tenía un menú **US** y otro **UT**, y los dos repetían los mismos cuatro pasos
(Identificación · Modelo · Estimación · Previsión). El código está partido
igual: un módulo por paso y clase, `US*` y `TF*`, que es ID, MOD, EST y FO.
Al listar modelos, cada uno llevaba su tipo (USM/TFM). Es decir: **el usuario
de TASTE elegía primero la clase de modelo y luego el paso**, nunca un
programa.

### 2.2 La escalera de ATSW

| sigla | clase | programa(s) | estado en la madre |
|---|---|---|---|
| **US** | univariante estocástico | fue estima, fuf prevé | completo: series, modelos, linaje |
| **MT** | transferencia multivariante (*Multivariate Transfer*) | drtran | completo: casos |
| **VARMA** | sistema simultáneo, con retroalimentación | drvarma → drvarma_gui | sin integrar (hito drvarma_gui) |
| **VECM** (VARMA-VECM) | VARMA con cointegración | drvec → drvec_gui | sin GUI: hito nuevo, drvec_gui |
| — | identificación (no es una clase de modelo) | fug, art | herramienta de US |

**UT no se ofrece.** Era la transferencia de una sola salida; MT, con drtran,
admite varias salidas y una red, y **la contiene**: un MT con una salida *es*
un UT. Un menú UT sería una segunda puerta a lo mismo. Se cita aquí como
antecedente de la sigla, nada más.

**La identificación queda aparte** (fug y art): no es una clase de modelo,
sino el paso previo de US. Va dentro del menú US, como en TASTE, no como menú
propio.

### 2.3 Dónde se ve la sigla y dónde el programa

- **La sigla, donde se elige la clase**: los títulos de menú (US, MT) y las
  cabeceras de las dos listas de la izquierda: «Series — US» y «Casos — MT».
  Así la sigla se aprende donde viven los objetos, y la madre enseña la
  escalera sin decir nada: lo de arriba es US y lo de abajo, MT.
- **El verbo, en el botón y en la entrada de menú**: «Estimar», «Prever…»,
  «Diagnosis…».
- **El programa, en segundo plano pero siempre**: en el globo («Estimar: abre
  el modelo en fue_gui…»), en la barra de estado al pasar por la entrada
  («US · fue — se manda el .pre si lo hay…») y en la cabecera del caso, que
  hoy dice «drtran · muestra Pre-Covid» y diría «MT · drtran · muestra
  Pre-Covid». **No se esconde**: cuando algo falla, el mensaje es del
  programa, y el analista tiene que saber cuál era.

---

## 3. La propuesta

### 3.1 La barra de menús

```
 Proyecto   Datos   US   MT   Ayuda
```

**Proyecto** — lo que es del proyecto entero:

```
 Nuevo…                    Ctrl+N
 Abrir…                    Ctrl+O
 Releer                    Ctrl+R
 ─────────────────────────
 Propiedades…              (hoy «Proyecto…»)
 Linaje…
 Cuaderno…
 ─────────────────────────
 Salir                     Ctrl+Q
```

**Datos** — las series como datos, antes de cualquier modelo. Es el menú
«Datos» de TASTE, que dibujaba sobre cualquier serie:

```
 Importar series…          (hoy «Datos…»)
 Editar la serie…
 Muestra nueva…            (hoy sólo el «+»)
 ─────────────────────────
 Media – desviación típica
 Serie y ACF / PACF…
 Histograma                (nuevo en la madre; fug ya lo dibuja)
```

**El histograma ya existe en fug, pero no en la madre.** El motor lo dibuja
con su gráfica `-d` (`fp_histogram` en `lib/fugplot`: el histograma de la
serie estandarizada con la densidad normal), y gtk_fmg lo ofrece con su botón
«Histogram». Los vistazos de la madre llaman a fug con `-e` (media–DT) y `-c`
(serie y ACF/PACF), así que el histograma es **un tercer modo del mismo
vistazo, con `-d`**: no hay que dibujar nada nuevo, sólo pedírselo a fug. Va
en Datos porque describe los datos, como TASTE, que también tenía su
histograma en «Datos». (En `lib/analisis`, Anómalos dibuja otro histograma,
pero ése es de los residuos y se queda allí.)

**US** — la serie marcada y sus modelos, **en el orden del ciclo de Box y
Jenkins** (identificar, especificar y estimar, diagnosticar, prever) y con
las decisiones al final:

```
 Media – desviación típica
 Serie y ACF / PACF…
 Listado de identificación…        (fug; hoy «Identificación con fug» y «→ fug»)
 Identificar la estructura ARIMA…  (art; hoy «Identificar desde los datos…»)
 ─────────────────────────
 Modelo nuevo, desde los datos            ┐ planos: decidido, revisable
 Estimar m01                 Ctrl+E      │ con el uso (§3.1 ter)
 Editar el .inp…                          │ (fue; «Especificar el primer
 Iterar: seguir desde su óptimo           │  modelo» si sólo hay datos)
 En otra muestra ▸                        ┘
 ─────────────────────────
 Diagnosticar ▸                           (submenú: decidido, §3.1 ter)
     Diagnosis…
     Orden de integración…
     Estacionalidad (MEG)…
     Ganancia…
     Anómalos…
     Identificar los residuos…
 Ver modelo…                              (plano, justo debajo: decidido)
 ─────────────────────────
 Prever…                              (fuf)
 ─────────────────────────
 Elegir
 Razón…
 Borrar…                     Supr
```

**La sección «Identificar» de US sigue el orden en que trabaja el analista**
(decidido, §8):
1. Media–DT, para decidir λ.
2. Serie y ACF/PACF, para decidir d y D.
3. **Listado de identificación…**, que es el nombre de la escuela para la
   *primera identificación completa*. Hoy «Identificación con fug» y «→ fug»
   lanzan `gtk_fmg`, la GUI de fug, con el `.inp` de datos de la muestra
   (siempre el nodo de datos, aunque haya otro modelo marcado). Allí está
   el juego de identificación del motor («Identification Plots Set»: serie,
   histograma, media–DT, ACF y PACF para 0..r diferencias). El globo dice
   «Primera identificación completa (fug)».
4. **Identificar la estructura ARIMA…**, que es lo que hoy se llama
   «Identificar desde los datos…»: art propone la estructura ARMA o ARIMA,
   con sus contrastes y candidatos, y el nombre nuevo dice eso.

Las dos primeras **repiten** entradas de Datos. El analista lo acepta
explícitamente: en Datos son descripción y en US son el primer paso del
camino del analista, y un menú que obligara a saltar de Datos a US a mitad
de la identificación estaría ordenado por el programa y no por el método.
Las dos entradas llaman a la misma función.

### 3.1 ter Submenús en US: «Diagnosticar ▸» sí; «Especificar» y «Estimar», planos

**Decidido (§8): «Diagnosticar ▸» es un submenú.** Corrige lo que este
estudio decía en §4, «no meter los análisis del modelo en un submenú». La
razón es **la longitud**. Con la sección Identificar en el orden del
analista, el menú US pasa de veinte entradas, y las siete de diagnóstico
eran el bloque más largo y más homogéneo: todas piden un `.out` al día y
todas se apagan juntas por la misma razón. Un submenú con nombre de paso del
ciclo («Diagnosticar») no esconde nada. Dice qué hay dentro, y quien acaba de
estimar va a él directamente. Lo que el estudio temía era un cajón genérico
(«Análisis ▸»); éste es un paso del método.

**«Ver modelo…» sale del submenú (decidido, §8).** Queda como entrada plana
**justo debajo de «Diagnosticar ▸»**. La razón del analista: quien quiere ver
el modelo estimado no debe tener que entrar al submenú. Ver el modelo es lo
primero que se hace al volver de estimar, antes de decidir si hace falta
diagnosticar nada. Las otras seis siguen dentro. Se apaga por la misma regla
que el submenú (un `.out` al día y el resumen que fue dejó), con su propio
globo.

**«Especificar» y «Estimar» se quedan planos (decidido, §8, revisable con el
uso).** Se pensó en dos submenús más:
- «Especificar ▸»: Modelo nuevo desde los datos, Editar el .inp…, En otra
  muestra ▸;
- «Estimar ▸»: Estimar mNN, Iterar.

La maqueta los comparó con un conmutador, y el analista los deja planos con
una frase: *«el uso lo dirá»*. Lo que pesaba en contra era que **Estimar es
la acción más frecuente** y ya tiene Ctrl+E y un botón. Además, «En otra
muestra ▸» habría quedado como submenú dentro de otro submenú. Si el uso pide
lo contrario, el cambio es de una sección (§3.2) y llega a la barra y al
contextual a la vez.

**Cómo encajan en GTK3:**
- **Teclado.** Un `GtkMenuItem` con `gtk_menu_item_set_submenu` lleva la
  flecha ▸ y se abre con **→** (o Intro); **←** o Esc vuelven al menú de
  arriba. Es el comportamiento estándar, sin código propio. La entrada
  «Diagnosticar» no tiene atajo propio; sus hijas tampoco lo necesitan.
- **Globos y barra de estado.**
  - La entrada «Diagnosticar ▸» lleva su propio globo, con lo que contiene y
    por qué está apagada si lo está: el mismo `porque` de `an_estado()` que
    hoy apaga los análisis.
  - Cada hija lleva el suyo, como ahora.
  - La señal `select` existe igual en las entradas de un submenú, así que la
    barra de estado sigue al teclado también dentro (§3.1 bis).
- **Apagado.** Si el modelo no tiene un `.out` al día, se apaga la entrada
  del submenú entera, y no las seis hijas una a una, y también «Ver modelo…»
  debajo. Su globo dice por qué,
  y el menú es más corto también en ese estado.
- **Los contextuales siguen el mismo criterio**, porque se construyen con las
  mismas secciones (§3.2). La sección «diagnosticar» devuelve la entrada
  con submenú seguida de «Ver modelo…» plano, y el menú del modelo (botón
  derecho) la recibe igual que la barra. Si algún día Especificar y Estimar
  pasan a submenú, el cambio se hace en un solo sitio y llega a los dos. La prueba de §5 (cada
  entrada del contextual está en la barra con el mismo rótulo) recorre
  también los submenús.

**MT** — el caso marcado y sus modelos:

```
 Nuevo caso…
 ─────────────────────────
 Estimar c01                 Ctrl+E   (drtran; hoy «Abrir en drtran»)
 Ver modelo…
 Devolver los univariantes…
 ─────────────────────────
 Elegir
 Razón…
 Borrar modelo…
 ─────────────────────────
 Derivar caso…
 Razón del caso…
 Borrar caso…
 ─────────────────────────
 Convertir los modelos viejos en casos…   (sólo si los hay)
```

**Ayuda** — el manual (F1), las teclas y «Acerca de». En TASTE, F1 decía
«NO HAY AYUDA DISPONIBLE»; aquí abre el manual, que ya se publica con cada versión.

**Por qué estos cinco y no otros.**
- **Un menú por clase, cada uno con su GUI**: US → fue_gui, MT →
  drtran_gui, VARMA → drvarma_gui, VECM → drvec_gui. Es la escalera entera en
  la barra: `Proyecto · Datos · US · MT · VARMA · VECM · Ayuda`.
- **Por clase y no por objeto.** «Serie / Modelo / Caso» parecía la opción
  obvia, porque son las listas, pero en la madre **coinciden**: las series y
  sus modelos son US, y los casos y sus modelos son MT. Llamarlos por la clase
  dice además lo que el objeto no dice, que hay una escalera y en qué peldaño
  se está. Es lo que hacía TASTE y lo que pide el analista.
- **Datos aparte** porque lo que contiene sirve igual a US y a MT, y antes de
  ambos: una serie se importa, se describe y se mira antes de saber en qué
  clase de modelo va a entrar.
- **Sin «Ver» ni «Herramientas».** No hay nada que poner en «Ver»: las
  vistas se eligen marcando, no con modos. Un menú «Herramientas» acabaría
  siendo el cajón de lo que no se supo dónde poner.
- **VARMA y VECM entran cuando entren sus GUI** (§3.5).

### 3.1 bis Requisito: el título de cada menú dice su nombre completo

**Petición del analista (18-oct-2026).** Al marcar un menú de la barra, al
pasar el ratón o al llegar con el teclado, sale un globo con el **nombre
completo** de la clase de modelo y con qué programas se trabaja. Para
Proyecto, Datos y Ayuda, el globo dice lo que contiene. Tiene que funcionar
**como funcionan hoy los globos de la madre**: primero una línea que dice
qué es, luego, tras una línea en blanco, el porqué o lo que contiene, y al
final la tecla. Es el formato de «Abrir…» (`main.c`: «Un proyecto.yaml. Si
está roto se dice y NO se abre: nunca se pisa.\n\nCtrl+O») y de «→ fue»
(«…\n\nCtrl+E, como Estimar en las hijas…»).

| menú | primera línea del globo | lo que sigue | tecla |
|---|---|---|---|
| Proyecto | Proyecto — el manifiesto y lo que cuelga de él | abrir, crear, releer, propiedades, linaje y cuaderno; se guarda solo | Alt+P |
| Datos | Datos — las series como datos, antes de cualquier modelo | importar, editar la serie, muestras, media–DT y serie con ACF/PACF | Alt+D |
| US | US — Modelo univariante (Box-Jenkins), con fue y fuf | la serie marcada y sus modelos: identificar, estimar, diagnosticar, prever | Alt+U |
| MT | MT — Transferencia multivariante, con drtran | el caso marcado: varias series, cada una con su modelo univariante, de entrada y de salida | Alt+M |
| VARMA | VARMA — Modelo VARMA, con drvarma | **hito futuro** (drvarma_gui): sistemas con retroalimentación | Alt+V |
| VECM | VECM — VARMA con corrección de error (VECM), con drvec | **hito futuro** (drvec_gui): sistemas cointegrados | Alt+C |
| Ayuda | Ayuda — el manual, las teclas y «Acerca de» | F1 abre el manual | Alt+Y |

**Nota técnica (GTK3).**
- Un `GtkMenuItem` de la barra es un `GtkWidget` como cualquier otro, y
  **admite globo** con `gtk_widget_set_tooltip_text`. Sale al pasar el ratón,
  con el mismo formato que el resto de la madre.
- **Con el teclado**, GTK3 no enseña el globo al llegar a una entrada con F10
  o Alt+letra. Lo que sí hace es emitir la señal **`select`** del
  `GtkMenuItem`, tanto al pasar el ratón como al llegar con las flechas o el
  mnemónico, y **`deselect`** al salir. Con ellas, **la barra de estado sigue
  al foco del teclado**: el mismo texto del globo se escribe abajo, y se
  borra al salir.
- Esa misma señal sirve para las entradas de dentro de los menús (§3.2). El
  analista lee lo mismo llegue con el ratón o con el teclado; sólo cambia
  dónde: el globo o la barra de estado.
- **El analista lo quiere también con el teclado** en los títulos. Como
  GTK3 no lo hace solo, en `select` del título se enseña el mismo texto en un
  recuadro propio, al lado del menú desplegado para no taparlo. Puede ser un
  `GtkPopover` anclado al título o una ventana `GTK_WINDOW_POPUP` con el
  estilo `tooltip`. Se quita en `deselect`.
- **Dentro de los menús basta la barra de estado**: con el ratón, el globo
  normal; con el teclado, la barra de estado. Un recuadro por cada entrada
  recorrida con las flechas sería ruido.

### 3.2 Un menú, dos sitios: los constructores se comparten

La regla que impide que esto se degrade es la que ya rige `lib/analisis`:
*dos copias de una regla son dos reglas*. **El menú US de la barra no es una
lista nueva, sino `menu_serie()` seguido de `menu_modelo()`**, las mismas
funciones de `main.c` que hoy construyen los contextuales, partidas en
secciones reutilizables. Se construye **al abrirse** (señal `show` del
submenú), igual que el contextual, de modo que las etiquetas que dependen del
estado («Estimar m01» o «Especificar el primer modelo») y la sensibilidad
calculada con `an_estado()` salen idénticas. En MT, lo mismo con
`menu_corrida()` y los botones del caso.

Consecuencias:
- **Los contextuales no desaparecen.** Son la puerta rápida y siguen siendo
  un subconjunto exacto de la barra: lo que se puede hacer *con esto*.
- **Los huecos de §1.2 se cierran solos.** Al poner Elegir y Razón… en la
  sección del modelo, aparecen en el contextual y en la barra a la vez.
- **El globo con el porqué se conserva**, y además pasa a la barra de estado
  al recorrer el menú (señal `select`). Es la línea 25 de TASTE, pero con el
  contenido que allí faltaba: el estudio anterior vio que esa línea sólo
  repetía el rótulo.

### 3.3 Los botones que quedan

**La fila de arriba se queda entera, al principio** (decidido después de ver
la maqueta, §8). Se quedan Abrir…, Nuevo…, Proyecto…, Linaje…, Cuaderno… y
Datos…, con la etiqueta del proyecto. Lo único que sale es el botón
`drtran` de la esquina. Sus botones son puertas duplicadas de Proyecto y
Datos, y eso no cuesta nada: es lo que el analista, el manual y el vídeo ya
conocen. Quitarla, o dejar sólo «Cuaderno…», queda como opción para más
adelante, cuando la barra se haya usado un tiempo. La maqueta conserva el
conmutador para compararlo.

**La fila de la rejilla se queda**, porque está pegada a su sujeto (el modelo
marcado), y se ordena por el ciclo:

```
 hoy:        [Modelo nuevo] [Iterar] [Elegir] [Razón…] │ [→ fug] [→ fue]
 propuesta:  [Modelo nuevo] [Estimar] [Iterar] │ [Elegir] [Razón…]
```

- **«→ fue» pasa a ser «Estimar»** (Ctrl+E, el mismo verbo que en las hijas).
- **«→ fug» sale de la fila.** Desde que existen los vistazos, que son el
  mismo dibujo de fug sin levantar la herramienta, lanzar fug completo es lo
  raro y no lo frecuente. Queda como «Listado de identificación…» en US y en
  el contextual de la serie.

**La fila del caso se queda** y usa el mismo verbo:

```
 hoy:        [Abrir en drtran] │ [Elegir] [Razón…] [Borrar modelo…] │ [Derivar caso…] [Razón del caso…] [Borrar caso…]
 propuesta:  [Estimar] │ [Elegir] [Razón…] │ [Derivar caso…] [Razón del caso…]
```

Los dos «Borrar» salen de la fila y se quedan en el menú MT, en el
contextual y con Supr, como ya pasa con los modelos de serie: **lo
destructivo, separado** (`DISENO-interfaz.md` §2.5).

**Ctrl+E estima lo que se ve.** La derecha es una pila (modelos de la serie o
vista del caso): Ctrl+E es «Estimar» de la que esté visible. Así el mismo
gesto vale en US y en MT, y el programa que se abre es cosa de la clase.

**El botón `drtran` de la esquina desaparece, y drtran sin caso también
(decidido, §8).** La razón del analista: **drtran siempre necesita modelos
para el input y el output**, es decir, los univariantes de cada serie con
su `.pre`. Eso es exactamente lo que fija un caso. Un drtran sin caso no tiene
de dónde sacarlos, salvo que el analista los busque a mano, y lo que estime no
queda escrito. A MT se entra por «Nuevo caso…» o por «Estimar» sobre un caso
que ya existe.

### 3.4 Teclado

- **Mnemónicos con Alt**, estándar en Windows y en GTK: Alt+P Proyecto, Alt+D
  Datos, Alt+U US, Alt+M MT, Alt+V VARMA, Alt+C VECM, Alt+Y Ayuda. F10 abre la barra (lo hace GTK por
  defecto).
- **Los atajos del #57 se quedan** y pasan a verse en el menú, que es donde se
  aprenden (`gtk_widget_add_accelerator` sobre la entrada). Se añaden sólo
  dos: **F1** (manual) y **Supr** en el caso. No hacen falta más; un atajo por
  cada entrada es ruido que nadie recuerda.
- **`<Primary>` en vez de Ctrl fijo.** Hoy se registra `GDK_CONTROL_MASK`; con
  `gtk_accelerator_parse( "<Primary>e", … )`, en macOS es Cmd, como espera
  quien usa un Mac.

### 3.5 VARMA y VECM: un menú y una GUI cada uno, cuando lleguen

- **Decidido (§8): cada uno tiene su menú y su GUI.**
  - El menú **VARMA** lanza drvarma_gui.
  - El menú **VECM** lanza una GUI de drvec, drvec_gui, que **no existe** y
    es un hito nuevo, posterior al de drvarma_gui.
  - La barra final es `Proyecto · Datos · US · MT · VARMA · VECM · Ayuda`.
- **Los dos trabajan sobre casos**, como MT (`motor: drvarma` / `drvec`, que
  `lib/proyecto` ya prevé con su campo `motor`). Tendrán los mismos pasos:
  nuevo caso, estimar, ver modelo, elegir, razón, derivar.
- **Los pasos de un peldaño al siguiente.** «Pasar a VARMA» desde un caso MT
  con ciclo, que es donde MT se acaba (`DISENO-casos.md` §4). «Pasar a VECM»
  desde un caso VARMA cuyas series estén cointegradas.
- **Lo que tiene hoy drvarma_gui no decide el reparto.** Aunque ya lleve VECM
  y Johansen, en la madre VECM es su propia clase, con su propio motor
  (drvec). Lo que haga drvarma_gui por dentro se ordena en el diseño de su
  hito.
- **La lista de casos gana una columna «Clase»** (MT, VARMA o VECM) cuando
  haya más de una. Hoy sería una columna con el mismo valor en todas las
  filas.
- **La cabecera de casos** pasa de «Casos — MT» a «Casos» cuando convivan
  varias clases, y la clase se lee en la columna.
- **No se ponen entradas en gris** «para cuando llegue». Una entrada que nunca
  se enciende enseña que el menú miente.

### 3.6 GTK3: GtkMenuBar, no GMenu

Hay dos formas de hacer la barra:

- **`GtkMenuBar` con `GtkMenuItem`** (recomendada). Es la API que ya usan los
  contextuales, **admite globos** (`gtk_widget_set_tooltip_text` sobre la
  entrada) y se puede construir al abrir. Funciona igual en Linux y en
  Windows. En macOS se ve dentro de la ventana y no en la barra global del
  sistema: es menos nativo, pero igual en las tres plataformas.
- **`GMenuModel` + `GAction` con `gtk_application_set_menubar`.** Es más
  moderna y centraliza la sensibilidad en acciones, y en macOS va a la barra
  global. Pero **en GTK3 las entradas de un GMenu no tienen globos**, y aquí
  el globo es donde vive el porqué (y el porqué de que una entrada esté
  apagada, que es lo más útil de los menús de hoy). Además obliga a reescribir
  los ~40 manejadores como acciones. **No compensa**: pierde lo que distingue a
  esta interfaz para ganar algo que no se ha pedido.

---

## 4. Qué se gana y qué se pierde

**Se gana:**
- **Todo en un sitio que se recorre.** Las ~55 acciones, en cinco menús, con
  las desactivadas explicando por qué.
- **Lo que hoy sólo descubre quien prueba el botón derecho**, a la vista para
  quien llega del manual.
- **La escalera visible**: US y MT en la barra y en las cabeceras, y el sitio
  de VARMA, decidido.
- **Un verbo por acción, con un solo nombre**: «Estimar» en US y en MT, con
  Ctrl+E en los dos.
- **El camino sin registro deja de ser el más visible.**

**Se pierde, y hay que decirlo:**
- **Nada de la fila de arriba**: se queda entera (§3.3). Lo que cambia para
  quien ya la conoce es sólo el botón `drtran` de la esquina.
- **La sigla es opaca para el alumno nuevo.** «US» no dice nada a quien no ha
  leído a Box y Jenkins. Se compensa con el globo del título del menú y el
  glosario del manual, pero es un coste real. La alternativa
  («Univariante», «Transferencia») es más clara y menos fiel a la escuela.
- **macOS sin barra global** (§3.6).
- **Las capturas del manual y el guion del vídeo** se quedan viejos. Se
  rehacen en el próximo release, como siempre, pero es trabajo: el manual
  tiene una docena de capturas de la madre y sus diálogos.

**Lo que NO hay que hacer:**
- **No migrar a GMenu/GAction**, por lo dicho en §3.6.
- **No poner barra de menús en las hijas.** fue_gui y drtran_gui están bien;
  son herramientas de una tarea, con sus páginas y su barra de pasos. El menú
  hace falta en la madre porque es la única ventana con muchos objetos y
  muchas acciones.
- **No quitar los contextuales ni el doble clic.** Son la vía rápida, y el
  #57 acaba de hacerlos accesibles con teclado.
- **No meter los análisis en un cajón genérico** «Análisis ▸». Lo que sí se
  hace (decidido) es un submenú con nombre de paso, «Diagnosticar ▸», por la
  longitud del menú US (§3.1 ter). Corrige lo que aquí decía la primera
  versión del estudio.
- **No ofrecer UT** (§2.2), ni entradas en gris para VARMA y VECM antes de
  sus hitos (§3.5).
- **No hacer una barra de iconos.** Los iconos de GTK para «estimar» o
  «diagnosis» no existen, y unos inventados se leen peor que una palabra.

---

## 5. Riesgos

- **Que la barra y los contextuales diverjan.** Es el riesgo principal, y lo
  que lo evita es §3.2 (los mismos constructores). Una prueba lo vigila:
  cada entrada del contextual tiene que estar en la barra con el mismo
  rótulo y la misma sensibilidad.
- **Que el menú US sea largo.** Pasaba de veinte entradas. Con
  «Diagnosticar ▸» y «Ver modelo…» debajo baja a unas dieciséis, con
  Especificar y Estimar planos (§3.1 ter). Si el uso dice que sigue siendo
  largo, el siguiente paso es meterlos en submenú.
- **Ctrl+E con dos sujetos.** Si la pila muestra el caso, Ctrl+E abre
  drtran. Es lo que se quiere, pero hay que decirlo en la barra de estado
  («Estimar c01 (MT) en drtran…») para que no sorprenda.
- **Las pruebas.** `test_gui.c` conduce los botones de la madre por puntero
  (`A.b_fue`, `A.b_iterar`…, 26 usos) y los menús contextuales abriéndolos y
  buscando la entrada por su rótulo (`menu_abierto`, `menu_modelo_prueba`).
  `boton_que_dice` (17 usos) sólo busca en las ventanas de análisis, que no
  se tocan. Cambiar rótulos rompe pocas pruebas, y se rompen en el PR que
  los cambia, que es lo que se quiere.

---

## 6. El plan: tres PR y dos hitos

Cada fase deja la madre usable y las pruebas en verde. **Las capturas del
manual y el guion del vídeo no se tocan en ninguna**: se rehacen en el
próximo release.

**Fase 1 — la barra, sin quitar nada.** Hecha (#66).
- Se parten `menu_serie`, `menu_modelo` y `menu_corrida` en secciones; la
  barra `Proyecto · Datos · US · MT · Ayuda` se construye con ellas al
  abrirse. VARMA y VECM no aparecen hasta sus hitos.
- Entran en el contextual del modelo **Elegir** y **Razón…**, y en el de la
  serie, **Muestra nueva…**.
- Globo → barra de estado al recorrer el menú (`select`/`deselect`), y los
  globos de los títulos con el nombre completo de cada clase (§3.1 bis).
- Los atajos se ven en las entradas; `<Primary>`; F1 abre el manual (con
  `lib/sitio` para encontrarlo instalado).
- **Pruebas:** una fase `barra` en `test_gui.c` que recorre los cinco menús,
  comprueba que cada entrada del contextual está en la barra con el mismo
  rótulo y sensibilidad, y activa tres entradas (Releer, Diagnosis…,
  Nuevo caso…) con el mismo efecto que su botón.
- **Riesgo:** bajo. No se quita nada, sólo se añade.

**Fase 2 — los nombres por el paso y la clase.** Hecha (#67).
- «→ fue» → «Estimar»; «Abrir en drtran» → «Estimar».
- «Series — US» y «Casos — MT» en las cabeceras; «MT · drtran · …» en la
  cabecera del caso.
- Los globos y los mensajes de estado dicen el programa
  («Estimar m01 (US) en fue…»).
- Ctrl+E sigue a la pila (US o MT).
- Unificar las variantes de «Modelo nuevo» y de «Estimar» en un nombre cada
  una (§1.2).
- «Identificación con fug» pasa a «Listado de identificación…» e
  «Identificar desde los datos…» pasa a «Identificar la estructura ARIMA…».
  El manual (§5 y §6) lo recoge en el próximo release.
- El vistazo «Histograma» (fug `-d`) en Datos, junto a los otros dos.
- **Pruebas:** las que buscan esos rótulos se actualizan; una nueva comprueba
  Ctrl+E con un caso marcado.

**Fase 3 — quitar lo que sobra.** Hecha: el botón `drtran` y drtran sin caso, «→ fug» y los dos «Borrar» de la fila del caso ya no están. La fase `quitados` de la prueba de la madre comprueba que no vuelven y que lo que hacían sigue en US, MT y el botón derecho.
- **La fila de arriba NO se quita**: se queda entera (§8).
- Fuera el botón `drtran` de la esquina, y con él drtran sin caso (decidido).
- Fuera «→ fug» y los dos «Borrar» de las filas.
- **Pruebas:** las que usan `a->b_drtran` pasan a activar la entrada de menú
  MT; las de la fila de arriba no cambian.
- **Es la fase que cambia lo que el analista ya conoce**: va sola, y
  después de que haya usado la barra con la fase 1 un tiempo.

**Hito drvarma_gui — el menú VARMA.**
- Menú VARMA, que lanza drvarma_gui con el caso.
- Columna «Clase» en los casos.
- «Pasar a VARMA» desde un caso MT con ciclo.
- Depende de que drvarma_gui acepte `--proyecto` y `--caso`. Va dentro del
  hito (`RELEVO.md` §6), no antes.

**Hito drvec_gui — el menú VECM.** Posterior al de drvarma_gui.
- Es un hito nuevo: **la GUI de drvec no existe**.
- Su diseño parte, como el de drvarma_gui, de lo aprendido en el porte de
  drvec y en sima.
- En la madre: menú VECM, casos con `motor: drvec` y «Pasar a VECM» desde
  un caso VARMA.

Las fases 1 y 2 se pueden juntar si se prefiere un solo PR; la 3 conviene
que vaya aparte.

---

## 7. Preguntas abiertas

Ninguna. Las dos últimas se cerraron el 18-oct-2026 (§8):
- la fila de arriba se queda entera (decisión 3);
- Especificar y Estimar se quedan planos (decisión 13), revisable con el uso. La maqueta es privada y todo en ella es simulado:
<https://claude.ai/artifact/9rHr1juao4k6LRUF2yU1Hs>.

---

## 8. Decisiones (18-oct-2026)

1. **Menús con la sigla**: `US`, `MT`, `VARMA`, `VECM`. El globo del título
   explica la sigla; las entradas van en castellano llano (§2.3).
2. **drtran sin caso desaparece**, junto con el botón de la esquina. La razón
   del analista: *el menú de drtran siempre necesita modelos para el input y
   el output*, y eso es lo que da un caso (§3.3).
3. **La fila de arriba se queda entera, al principio**, decidido después de
   ver la maqueta. Sólo sale el botón `drtran`. La fase «quitar lo que
   sobra» ya no la toca (§3.3, §6).
4. **«→ fug» sale de la fila de la rejilla.** Queda en el menú US y en el
   contextual de la serie (§3.3).
5. **VARMA y VECM, cada uno con su menú y su GUI.**
   - VARMA lanza drvarma_gui, que llega con el hito drvarma_gui.
   - VECM lanza drvec_gui, un hito nuevo y posterior: la GUI no existe.
   - Barra final: `Proyecto · Datos · US · MT · VARMA · VECM · Ayuda`.
   - Queda corregido lo que este estudio decía antes, que VARMA-VECM iba
     dentro de VARMA (§3.5, §6).
6. **Datos, como menú propio** (§3.1).
7. **Requisito nuevo: el título de cada menú dice su nombre completo**, al
   pasar el ratón y al llegar con el teclado, con el formato de los globos de
   hoy (§3.1 bis).
8. **La sección «Identificar» de US, en el orden del analista**: Media–DT ·
   Serie y ACF/PACF · Listado de identificación… (fug, la «primera
   identificación completa») · Identificar la estructura ARIMA… (art, hoy
   «Identificar desde los datos…»). La repetición con Datos se acepta
   explícitamente: es el camino del analista (§3.1).
9. **Los histogramas de los datos van en Datos.** El histograma existe en
   fug (`-d`, el botón «Histogram» de gtk_fmg) pero no en la madre: entra
   como tercer vistazo (§3.1).
10. **Para después del diseño de la madre**, anotado en `TODO.md`:
    - la comprobación de la instalación al arrancar;
    - la ventana de presentación con los créditos;
    - el icono de la web.
11. **«Diagnosticar ▸» es un submenú del menú US**, por la longitud del menú.
    Corrige el «no hacer» de la primera versión (§3.1 ter).
12. **«Ver modelo…» sale del submenú Diagnosticar** y queda plano, justo
    debajo de «Diagnosticar ▸», en el menú US y en el contextual del modelo.
    Quien quiere ver el modelo estimado no debe tener que entrar al submenú
    (§3.1 ter).
13. **«Especificar» y «Estimar» se quedan planos.** En palabras del
    analista, «el uso lo dirá». Es una decisión **revisable con el uso**: si
    el menú sigue pesando, pasan a submenú con un cambio de sección (§3.1 ter,
    §3.2).

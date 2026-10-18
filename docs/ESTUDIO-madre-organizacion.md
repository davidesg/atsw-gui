# La madre por dentro: botones, menús y la escalera de modelos

Estudio del 18-oct-2026, a petición del analista:

> *«Lo que no veo en la madre es menús, ahora que tenemos. También los
> botones se refieren a los GUI de los programas; deberían apuntar al modelo
> que manejan, un poco como lo hacía TASTE. fue maneja modelos univariantes,
> US en los nemotécnicos de Box y Jenkins. […] En drtran tenemos algo más
> general y sería MT, de Multivariate Transfer. El botón de drtran está en
> una esquina de forma desordenada. Quizás deberíamos repensar cómo está
> organizada la madre respecto a sus botones y menús.»*

Es un estudio con propuesta y plan, **sin código**. Continúa
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
Ayuda`, construida con las mismas funciones que los menús contextuales; los
botones se quedan pegados a lo que gobiernan y se llaman por el paso
(«Estimar»), no por el programa; el botón de la esquina desaparece.** Cuatro
PR pequeños (§6).

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
| **VARMA** | sistema simultáneo, con retroalimentación | drvarma | sin integrar (hito drvarma_gui) |
| **VARMA-VECM** | VARMA con cointegración | drvec | sin GUI |
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
```

**US** — la serie marcada y sus modelos, **en el orden del ciclo de Box y
Jenkins** (identificar, especificar y estimar, diagnosticar, prever) y con
las decisiones al final:

```
 Identificar desde los datos…      (art)
 Identificación completa en fug…
 ─────────────────────────
 Modelo nuevo, desde los datos
 Estimar m01                 Ctrl+E   (fue; «Especificar el primer modelo» si sólo hay datos)
 Editar el .inp…
 Iterar: seguir desde su óptimo
 En otra muestra ▸
 ─────────────────────────
 Ver modelo…
 Diagnosis…
 Orden de integración…
 Estacionalidad (MEG)…
 Ganancia…
 Anómalos…
 Identificar los residuos…
 ─────────────────────────
 Prever…                              (fuf)
 ─────────────────────────
 Elegir
 Razón…
 Borrar…                     Supr
```

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
- **VARMA no tiene menú todavía** (§3.5).

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

**La fila de arriba desaparece.** Todo lo que tenía va a Proyecto y Datos, que
quedan justo encima. La etiqueta del proyecto pasa al título de la ventana
(«ATSW — ipc-wti») y, con la ruta, al globo del título y a «Propiedades…».
Gana una fila de alto y, sobre todo, deja de tener un botón suelto a la
derecha.

**La fila de la rejilla se queda**, porque está pegada a su sujeto (el modelo
marcado), y se ordena por el ciclo:

```
 hoy:        [Modelo nuevo] [Iterar] [Elegir] [Razón…] │ [→ fug] [→ fue]
 propuesta:  [Modelo nuevo] [Estimar] [Iterar] │ [Elegir] [Razón…]
```

- **«→ fue» pasa a ser «Estimar»** (Ctrl+E, el mismo verbo que en las hijas).
- **«→ fug» sale de la fila.** Desde que existen los vistazos, que son el
  mismo dibujo de fug sin levantar la herramienta, lanzar fug completo es lo
  raro y no lo frecuente. Queda en US y en el contextual de la serie.

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

**El botón `drtran` de la esquina desaparece.** A MT se entra por un caso:
«Nuevo caso…» o «Estimar» sobre uno que ya existe. Si se quiere conservar la
exploración sin registro (probar dos series en drtran antes de declarar el
caso), puede ir al fondo del menú MT como «Explorar en drtran, sin caso…»,
con el aviso en la etiqueta de que no se guarda. **Mi recomendación es
quitarlo**: un caso cuesta un diálogo, se puede borrar si no tiene modelos, y
lo que no queda escrito es lo que este proyecto lleva un año arreglando.
Pero es decisión del analista (§7).

### 3.4 Teclado

- **Mnemónicos con Alt**, estándar en Windows y en GTK: Alt+P Proyecto, Alt+D
  Datos, Alt+U US, Alt+M MT, Alt+Y Ayuda. F10 abre la barra (lo hace GTK por
  defecto).
- **Los atajos del #57 se quedan** y pasan a verse en el menú, que es donde se
  aprenden (`gtk_widget_add_accelerator` sobre la entrada). Se añaden sólo
  dos: **F1** (manual) y **Supr** en el caso. No hacen falta más; un atajo por
  cada entrada es ruido que nadie recuerda.
- **`<Primary>` en vez de Ctrl fijo.** Hoy se registra `GDK_CONTROL_MASK`; con
  `gtk_accelerator_parse( "<Primary>e", … )`, en macOS es Cmd, como espera
  quien usa un Mac.

### 3.5 VARMA y VARMA-VECM: cuando lleguen, no antes

- **El sitio está reservado por el diseño, no por la pantalla.** Cuando el
  hito drvarma_gui integre drvarma con los casos (`motor: drvarma`, que
  `lib/proyecto` ya prevé), entra un menú **VARMA** entre MT y Ayuda. Tendrá
  los mismos pasos y la entrada «Pasar a VARMA» que `DISENO-casos.md` §4
  prevé para el caso con ciclo, que es donde MT se acaba.
- **VARMA-VECM va dentro de VARMA**, no como menú propio: es la misma clase
  con cointegración, y el GUI actual de drvarma ya lleva VECM y Johansen.
  Mientras drvec no tenga GUI, no aparece.
- **La lista de casos gana una columna «Clase»** (MT o VARMA) en ese momento.
  Hoy sería una columna con el mismo valor en todas las filas.
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
- **Una fila de botones menos.**

**Se pierde, y hay que decirlo:**
- **Lo que se aprendió con los botones de arriba.** El analista, el manual y
  el vídeo enseñan «Datos…» y «Linaje…» como botones. Pasan a estar un clic
  más lejos (Proyecto ▸ Linaje…). Para lo que se usa una vez por sesión es
  aceptable; por eso «Cuaderno…» es la duda (§7).
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
- **No meter los análisis del modelo en un submenú** «Análisis ▸». Son lo que
  más se usa después de estimar; un submenú los esconde para ahorrar diez
  líneas de altura que sobran.
- **No ofrecer UT** (§2.2) ni entradas en gris para VARMA-VECM (§3.5).
- **No renombrar ahora «Identificar desde los datos…»** («Proponer
  candidatos…» sería más exacto, porque art propone y no estima). Es el
  nombre que usa el manual en §6, y cambiarlo no arregla nada que falle.
- **No hacer una barra de iconos.** Los iconos de GTK para «estimar» o
  «diagnosis» no existen, y unos inventados se leen peor que una palabra.

---

## 5. Riesgos

- **Que la barra y los contextuales diverjan.** Es el riesgo principal, y lo
  que lo evita es §3.2 (los mismos constructores). Una prueba lo vigila:
  cada entrada del contextual tiene que estar en la barra con el mismo
  rótulo y la misma sensibilidad.
- **Que el menú US sea largo** (unas 20 entradas). Está dentro de lo normal
  para un menú con separadores por paso, y la alternativa (submenús) es
  peor, por lo dicho en §4.
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

## 6. El plan: cuatro PR pequeños

Cada fase deja la madre usable y las pruebas en verde. **Las capturas del
manual y el guion del vídeo no se tocan en ninguna**: se rehacen en el
próximo release.

**Fase 1 — la barra, sin quitar nada.**
- Se parten `menu_serie`, `menu_modelo` y `menu_corrida` en secciones; la
  barra `Proyecto · Datos · US · MT · Ayuda` se construye con ellas al
  abrirse.
- Entran en el contextual del modelo **Elegir** y **Razón…**, y en el de la
  serie, **Muestra nueva…**.
- Globo → barra de estado al recorrer el menú.
- Los atajos se ven en las entradas; `<Primary>`; F1 abre el manual (con
  `lib/sitio` para encontrarlo instalado).
- **Pruebas:** una fase `barra` en `test_gui.c` que recorre los cinco menús,
  comprueba que cada entrada del contextual está en la barra con el mismo
  rótulo y sensibilidad, y activa tres entradas (Releer, Diagnosis…,
  Nuevo caso…) con el mismo efecto que su botón.
- **Riesgo:** bajo. No se quita nada, sólo se añade.

**Fase 2 — los nombres por el paso y la clase.**
- «→ fue» → «Estimar»; «Abrir en drtran» → «Estimar».
- «Series — US» y «Casos — MT» en las cabeceras; «MT · drtran · …» en la
  cabecera del caso.
- Los globos y los mensajes de estado dicen el programa
  («Estimar m01 (US) en fue…»).
- Ctrl+E sigue a la pila (US o MT).
- Unificar las variantes de «Modelo nuevo» y de «Estimar» en un nombre cada
  una (§1.2).
- **Pruebas:** las que buscan esos rótulos se actualizan; una nueva comprueba
  Ctrl+E con un caso marcado.

**Fase 3 — quitar lo que sobra.**
- Fuera la fila de arriba; el proyecto al título de la ventana.
- Fuera el botón `drtran` de la esquina (o al fondo de MT, según §7).
- Fuera «→ fug» y los dos «Borrar» de las filas.
- **Pruebas:** las que usan `a->b_drtran` y los botones de arriba pasan a
  activar la entrada de menú.
- **Es la fase que cambia lo que el analista ya conoce**: va sola, y
  después de que haya usado la barra con la fase 1 un tiempo.

**Fase 4 — VARMA, con el hito drvarma_gui.**
- Menú VARMA, columna «Clase» en los casos y «Pasar a VARMA» desde un caso
  MT con ciclo.
- Depende de que drvarma_gui acepte `--proyecto` y `--caso`.
- No se empieza antes que ese hito, y VARMA-VECM espera a que drvec tenga
  GUI.

Las fases 1 y 2 se pueden juntar si se prefiere un solo PR; la 3 conviene
que vaya aparte.

---

## 7. Preguntas abiertas (son del analista)

1. **¿Títulos de menú con la sigla o con la palabra?** `US · MT` (la escuela,
   TASTE, lo que pediste) o `Univariante · Transferencia` (más claro para el
   alumno). Mi recomendación es la sigla, con el globo explicándola y las
   entradas en castellano llano.
2. **¿drtran sin caso: fuera del todo, o al fondo del menú MT** como
   «Explorar en drtran, sin caso…»? Recomiendo fuera.
3. **¿Fuera toda la fila de arriba, o se queda algún botón?** El candidato a
   quedarse es «Cuaderno…», que se abre a menudo.
4. **¿«→ fug» sale de la fila de la rejilla?** Recomiendo que sí, ahora que
   están los vistazos; queda en el menú US y en el contextual de la serie.
5. **Cuando llegue VARMA: ¿un menú por clase (`US · MT · VARMA`) o un solo
   menú «Sistemas»** que cambie según el motor del caso? Recomiendo uno por
   clase, como TASTE y como la escalera; VARMA-VECM dentro de VARMA.
6. **¿Datos como menú propio o dentro de Proyecto?** Recomiendo propio:
   describir una serie es previo a elegir la clase de modelo, y en TASTE
   también lo era.

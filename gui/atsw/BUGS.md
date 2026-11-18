# Fallos conocidos — la madre (atsw_gui)

## BUG-0001 — en Windows, cada motor que lanza un GUI abre una consola, y el vistazo congela la madre

**Encontrado:** 2026-10-27, por el usuario, en Windows 10/11 con el paquete
v0.2.1. En Linux y macOS no pasa.
**Arreglado:** 2026-10-27 (PR #95), en `lib/sitio`, `lib/engine`,
`gui/atsw/src/vistazo.c`, `gui/fug/src/fug_run.c` y `gui/fue`.

### Síntoma

En el vistazo de la madre («Media – desviación típica», «Serie y ACF/PACF»,
el histograma), cada cambio de λ, d o D tarda mucho en verse, la ventana se
queda congelada mientras, y asoma una ventana de consola. «Derivar» un
modelo a fue_gui también asoma una. La impresión del usuario: cada orden que
corre la madre abre una consola.

### Por qué

- **La consola.** Los motores (fue, fug, art...) son programas de consola y
  los GUIs se enlazan con `-mwindows`, sin consola. Todos los GUIs lanzaban
  los motores con `g_spawn_*` (`lib/engine` con `g_spawn_async_with_pipes`,
  el vistazo y fug_gui con `g_spawn_sync`). En Windows, GLib lanza a través
  de `gspawn-win64-helper.exe` --un programa sin consola, porque el que lanza
  no la tiene-- y el motor, hijo de ese ayudante, recibe una consola NUEVA,
  con su ventana. GLib no tiene una bandera para pedir `CREATE_NO_WINDOW`, ni
  en `g_spawn` ni en `GSubprocess`. Además fue_gui abría los PDF con
  `system("start ...")`, que pasa por `cmd.exe`: otra consola.
- **fue_gui era un programa de consola.** Su Makefile ponía `-mwindows` solo
  con `OS=windows`, que es lo que vale al compilar cruzado; en MSYS2 `OS`
  viene del entorno como `Windows_NT`, así que fue_gui.exe salía con el
  subsistema de consola (`objdump -p`: `00000003 (Windows CUI)`) y abría una
  al arrancar. Es la ventana que se veía al «Derivar» desde la madre.
- **La lentitud.** El vistazo (`vistazo.c`, `dibuja`) corría fug con
  `g_spawn_sync` en el hilo de la interfaz, una vez por cada clic en un
  mando. En Windows cada corrida eran dos procesos (el ayudante y fug), una
  consola que se crea y se destruye y el antivirus mirando cada ejecutable;
  la ventana no respondía hasta que acababa, y los clics seguidos se
  encolaban.

### Arreglo

- **`lib/sitio`**: `sitio_lanza_con_tuberias` y `sitio_lanza_y_espera`, con la
  firma de `g_spawn_async_with_pipes` y `g_spawn_sync`. En Linux y macOS
  llaman a esas, tal cual: nada cambia. En Windows lanzan con
  `CreateProcessW`, sin el ayudante de GLib, y con `CREATE_NO_WINDOW` si el
  que lanza no tiene consola (el criterio de GLib para elegir ayudante: un
  programa de consola, como atsw-cli o una batería, sigue compartiendo la
  suya con el motor). El hijo hereda solo sus tres manejadores
  (`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`); la entrada que no se pide como
  tubería es `NUL`. Y `sitio_abre` (solo Windows), con `ShellExecuteW`, para
  abrir un fichero con su programa.
- **`lib/engine`** lanza por `sitio_lanza_con_tuberias`: vale para todas las
  corridas de la madre (derivar, identificar, art, fue -d...), de fue_gui y
  de drtran_gui. El *job object* de BUG-0001 de drtran_gui sigue igual: el
  manejador del proceso es el mismo tipo que daba GLib.
- **`vistazo.c`**: fug corre con `engine_start`, sin parar la ventana; un
  clic espera 250 ms a que no llegue otro (tres clics seguidos son una
  corrida); hay una corrida a la vez --si se pide otra, la que hay se para
  y su resultado se tira--; se ve el dibujo anterior hasta que llega el
  nuevo. El pie se pone con el primer dibujo, y «Al cuaderno» apunta la
  transformación del dibujo que se ve, no la de los mandos.
- **`fug_run.c`** (fug_gui) corre fug por `sitio_lanza_y_espera`.
- **fue_gui** abre los PDF con `sitio_abre` en vez de `system("start ...")`,
  y su Makefile pone `-mwindows` también con `OS=Windows_NT`.

drvarma_gui (`engines/drvarma/gui`) tiene su propio `CreateProcessW` y
`g_spawn_sync`; no se toca aquí (no va en el paquete).

### Comprobado

- **En Windows, en la CI**: `lib/sitio/prueba_consola.sh` (paso «Sin consola
  al lanzar los motores» de `build.yml`). Un programa compilado con
  `-mwindows` lanza `hijo_consola.exe` (de consola) por `engine_run`,
  `engine_start`, el camino del vistazo y el de fug_gui, y el hijo dice si
  tiene ventana (`GetConsoleWindow`). Mira también que los cuatro GUIs
  están enlazados con el subsistema «Windows GUI», y mide un dibujo del
  vistazo con fug. Con la prueba sola, sobre main (los dos primeros commits
  del PR), en el runner `windows-latest`:

      engine_run     SI ASOMA UNA CONSOLA
      engine_start   SI ASOMA UNA CONSOLA
      vistazo        SI ASOMA UNA CONSOLA
      fug_gui        SI ASOMA UNA CONSOLA
      el vistazo con fug: 269.4 ms por dibujo (media de 10)
      gui/fue/bin/fue_gui.exe: 00000003 (Windows CUI)

  y con el arreglo pasa (ver el PR).
- `lib/sitio/test_lanza.c` (en la batería de gui/drtran, las tres
  plataformas): las dos funciones lanzan, separan salida y error, devuelven
  el estado y pasan los argumentos con espacios, comillas y barras.
- La batería de la ventana de la madre: tres clics seguidos en λ no corren
  fug en el acto y son una sola corrida.

Lo que no se puede ver sin un Windows de verdad: el `system("start ...")` de
fue_gui (abriría un visor en la CI) ni el efecto del antivirus del usuario.

### Reproducción

`bugs/BUG-0001-repro/repro.py`: mira el CÓDIGO --que el vistazo no corre fug
con `g_spawn_sync` y espera a los clics, que lib/engine y fug_gui lanzan por
lib/sitio, que lib/sitio usa `CREATE_NO_WINDOW` y que fue_gui no usa
`system`-- y, en Windows con bash de MSYS2, corre además
`lib/sitio/prueba_consola.sh`. Sale con 1 mientras alguna falle. Fuera de
Windows prueba que el código va por el camino arreglado, no que la consola
no asome: eso lo prueba la CI de Windows.

## BUG-0002 — instalada con el .deb, la madre no encuentra a fug y el vistazo no se abre

**Encontrado:** 2026-10-27, en la revisión de v0.2.2 con los casos de los
vídeos (`.github/workflows/revision-casos.yml`), en Linux con el `.deb`.
En Windows y macOS no pasa.
**Arreglado:** 2026-10-28 (v0.2.3): la lista de `lanza_fug()` lleva ya el sitio del
paquete, `"./%s"`, como las demás. `repro.py` sale con 0.

### Síntoma

Con el `.deb` instalado y ATSW abierto desde el menú o con `atsw`,
«Serie y ACF / PACF…» y «Media – desviación típica» no abren su ventana. La
escena 01 del vídeo del IPC subyacente se para en «ventana vistazo», y la
barra de estado de la madre dice «No encuentro el motor fug.» (runner
ubuntu-24.04, run 40597482182 de `revision-casos`). Con `/usr/lib/atsw/bin`
en el PATH, la ventana sí se abre.

### Por qué

El vistazo busca a fug con `sitio_busca()`, y su lista de sitios
(`gui/atsw/src/vistazo.c`, `lanza_fug`) solo tiene el del árbol de
compilación, `"../../engines/fug/%s"`. Le falta el del paquete, `"./%s"`
(todos los programas en el mismo directorio, `packaging/programas.sh`), que
sí tienen las demás listas (`gestos.c`, `an_identifica.c`, `an_formales.c`).
Sin él cae al PATH:

- en Linux el PATH no tiene `/usr/lib/atsw/bin`: fug no se encuentra, o se
  encuentra otro más viejo (un `/usr/local/bin/fug` de una instalación
  anterior se cuela sin avisar, como pasó con fue_gui);
- en Windows no se ve: `g_find_program_in_path` mira antes el directorio del
  propio `.exe`;
- en macOS tampoco: el lanzador de la `.app` pone `Resources/bin` el primero
  del PATH.

El arreglo natural es añadir `"./%s"` a esa lista. Espera el visto bueno.

### Reproducción

`bugs/BUG-0002-repro/repro.py`: resuelve cada sitio de `lanza_fug()` como lo
haría `sitio_busca()` en la disposición del paquete (la madre y fug en el
mismo directorio, nada en el PATH) y sale con 1 mientras ninguno dé con fug.
En la pantalla lo prueba la fila «Escena extra/vistazo-sin-path» de
`revision-casos.yml`, que abre el vistazo sin el directorio del paquete en el
PATH.

## BUG-0003 — pr_escribir pisa en silencio un manifiesto que no puede leer

**Encontrado:** 2026-11-14, al repasar lo bajo de la auditoría de procesos
(`docs/AUDITORIA-procesos.md`, C6). Es de `lib/proyecto`, y por tanto de todos
los que escriben el manifiesto: la madre, sus ventanas, fue_gui, drtran_gui,
sim_gui y atsw-cli.
**Arreglado:** 2026-11-14: `pr_escribir` se niega con `PR_EILEGIBLE` y dice
por qué. `repro.py` sale con 0.

### Síntoma

Un programa tiene el proyecto abierto. Otro, de un ATSW más nuevo, escribe en
el manifiesto una clave que el primero no conoce. El primero guarda un cambio
suyo: devuelve 0, sin aviso, y la clave nueva ha desaparecido del fichero. Lo
mismo con un `schema_version` mayor o con un manifiesto estropeado a mano: lo
que había se sustituye por la copia de este programa.

### Por qué

Desde la fase 1, `pr_escribir` es transaccional: coge el cerrojo, lee el
manifiesto del disco y, si otro lo cambió, fusiona. Pero si esa lectura
**fallaba**, se trataba igual que si el fichero no existiera
(`hay = existe(path) && leer_crudo(...) == 0`). No había nada con qué
fusionar, y la copia de aquí se escribía encima.

Al abrir no pasaba: `pr_leer` rechaza la clave desconocida, así que ningún
programa arranca con un manifiesto que no entiende. El hueco era el de quien
ya lo tenía abierto cuando otro lo cambió.

### Arreglo

Si el manifiesto en disco existe y no se puede leer, `pr_escribir` no escribe,
devuelve `PR_EILEGIBLE` y el mensaje dice dónde está lo que no entiende
(`fichero:línea: clave`). Un fichero **vacío** (`PR_EVACIO`) sí se escribe:
no hay nada que perder.

### Reproducción

`bugs/BUG-0003-repro/repro.py` compila `pisa.c` contra `lib/proyecto`: escribe
un manifiesto, le añade una clave desconocida y guarda encima un cambio con la
copia que tenía en memoria. Sale con 1 si `pr_escribir` devuelve 0 y la clave
ha desaparecido. Lo prueba también la sección 10 de `lib/proyecto/test_proyecto.c`.

## BUG-0004 — una muestra no se recorta por el principio

**Encontrado:** 2026-11-16, por el analista con el IPC de Portugal: una muestra
«desde 1/2010 hasta 12/2019» seguía empezando en 2002.
**Arreglado:** 2026-11-16 (#182). `repro.py` sale con 0.

### Síntoma

El final de la muestra se respetaba; el principio, no. Todo lo que colgaba de
la muestra —art, fue, los casos— se estimaba desde el principio de los datos.

### Por qué

`atsw_genera_inp` (`src/genera.c`) solo recibía el «hasta»: el «desde» de la
muestra no se usaba en ningún sitio, ni al poblarla (`atsw_puebla_muestra`)
ni al derivar un modelo en ella (`gestos.c`).

### Arreglo

`atsw_genera_inp` recibe también el «desde», y `atsw_desde_i` —la otra punta
de `atsw_hasta_n`— da la primera observación de la ventana. Las muestras
creadas antes del arreglo conservan su nodo de datos viejo: hay que crearlas
de nuevo.

### Reproducción

`bugs/BUG-0004-repro/repro.py` compila `recorta.c` con `genera.c` sobre los
datos del manual, declara la muestra 1/2010 – 12/2019, la puebla como la
madre y lee el .inp. Sale con 1 si no empieza en 1/2010 con 120
observaciones. Lo prueban también los dos extremos en `tests/test_genera.c`.

## BUG-0005 — un proyecto creado con «Nuevo…» no ve lo que hace el asistente

**Encontrado:** 2026-11-17, al grabar el vídeo del asistente: en un proyecto
recién creado en la madre, el asistente creó y estimó un modelo, y la madre
no lo enseñó.
**Arreglado:** 2026-11-17. `repro.py` sale con 0.

### Síntoma

Con un proyecto abierto con «Abrir…», un ejemplo o `--proyecto`, lo que
escribe el asistente aparece solo (#181). Con uno creado con «Nuevo…», no:
hacía falta volver a abrirlo.

### Por qué

La vista en vivo (`src/vigila.c`) se pone en `atsw_abre`. «Nuevo…»
(`on_nuevo`, `src/main.c`) no pasa por ahí: crea el manifiesto y lo guarda
sin vigilarlo. Tampoco guardaba el estado de la ventana Asistente del
proyecto que se dejaba, ni ponía el del nuevo.

### Arreglo

`on_nuevo` hace lo mismo que `atsw_abre`: guarda el estado de la ventana del
proyecto que se deja y, con el nuevo creado, llama a `atsw_vigila` y a
`atsw_asistente_restaura`.

### Reproducción

`bugs/BUG-0005-repro/repro.py` lanza la madre en un broadwayd con el módulo
de escenas del manual: crea un proyecto con «Nuevo…», carga los datos del
manual y, con la madre abierta, da un paso como el asistente (`atsw-cli
proyecto nuevo … --por asistente`). Sale con 1 si en 20 s la barra de estado
no dice «, del asistente.». Para eso el módulo de escenas tiene dos órdenes
nuevas: `lanza` (otro programa, sin esperar) y `aguarda` (un texto a la
vista).

## BUG-0006 — la misma Q de Ljung-Box con dos p, y Q(g.l.) leído como Q(retardo)

**Encontrado:** 2026-11-18, en el estudio de lo que art-python ya aprendió
(`docs/ESTUDIO-art-python-asistente.md` §3.6): art tuvo los dos (sus
BUG-0195 y BUG-0213).
**Arreglado:** 2026-11-18, en `lib/outfile`, `lib/dictamen`,
`lib/analisis/an_anomalos.c` y `atsw-cli anomalos`.

### Síntoma

Sobre un AR(2) (el golden RIPC.2 de fue), la diagnosis decía
«Q(37)=43.4 p=0.217» y la ventana de anómalos, con la misma Q, «Q(39) con
43.4 (p = 0.289)». Y la diagnosis rotulaba cada peldaño con sus grados de
libertad: «Q(10)» para la Q de 12 retardos, y si fallaba, «donde peor es en
el retardo 10». Un fallo en el retardo 12 —estacional— se leía como del 10.

### Por qué

- **Anómalos** contrastaba la Q de m retardos con m grados de libertad, sin
  descontar los parámetros ARMA. El motor y la diagnosis sí los descuentan.
- **La diagnosis** guardaba de cada línea del `.out` la Q y sus g.l., no el
  retardo, que es el primer número de la línea, y rotulaba con los g.l.

### Arreglo

- `lib/outfile` lee también el retardo de cada peldaño (`lb_lag_`), y
  `fueout_gl_q` da los g.l. de una Q de m retardos: m − (p + q + P + Q),
  como el motor.
- La diagnosis escribe «Q(12)=8.6 (10 g.l.) p=0.575» y el retardo que es
  («donde peor es en el retardo 12»). `DX_DATO` pasa a 256 para que la
  escalera entera quepa.
- Anómalos, en la ventana y en `atsw-cli anomalos`, «Q(39), 37 g.l.», con su
  p; el JSON lleva `gl`.

### Reproducción

`bugs/BUG-0006-repro/repro.py`: el golden RIPC.2 con `atsw-cli dictamen` y
`anomalos`. Sale 1 con el atsw-cli de antes y 0 con el arreglo.

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
**Arreglado:** no (abierto).

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

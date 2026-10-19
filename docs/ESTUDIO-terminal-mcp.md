# Un asistente en la madre: la terminal, los MCP a medida y cuándo

Estudio del 19-oct-2026, a petición del analista:

> *«¿Qué posibilidades tenemos de introducir en atsw-gui un terminal, y que
> ese terminal tenga un MCP que pueda manejar los motores de atsw-gui?»*

Y sus precisiones, el mismo día:

> *«Los MCP serían MTM (Made to Measure) de los que tenemos en Python. Sería
> necesario que funcione en Windows y Mac, porque los usuarios de Linux son
> marginales y a los estudiantes no les puedo obligar a tener Linux. drvec ya
> funciona con sima.»*

Y una pregunta añadida: **cuándo** se hace, antes o después de los hitos
drvarma_gui y drvec_gui de `ESTUDIO-madre-organizacion.md` §6 (aquí, §8).

Es un estudio con propuesta y plan, **sin código**. Continúa una consulta
anterior, la del 23-sep-2026, que quedó anotada en `DISENO-madre.md` §10
(«una consola de verdad NO, un panel de conversación SÍ»). Lo que ha cambiado
desde entonces, y por eso se reabre:

- los tres MCP en Python han madurado (art 0.2.3, drtran 0.2.6, sima 0.2.0
  con el VEC);
- la madre tiene casos MT, barra de menús por clase y relectura por huella;
- y el analista pide ahora MCP **a medida** de la madre, no los genéricos.

Lo que entonces se concluyó sobre las terminales sigue en pie, y aquí se
comprueba otra vez (§3).

---

## 0. El veredicto, primero

1. **Lo que hace falta no es una terminal: es un MCP que hable el idioma del
   proyecto.** Hoy los tres MCP trabajan sobre ficheros sueltos y llevan su
   propia gestión (`guion.json`); no saben qué es `m03`, ni un caso, ni el
   linaje, ni el cuaderno. El MTM es esa capa (§2), y es el 80 % del valor.
2. **Empotrar una terminal en Windows es un proyecto, no un widget.** VTE no
   existe en Windows. Una terminal propia sobre ConPTY y `libvterm` es
   posible pero son semanas, y luego mantenerla en tres sistemas (§3).
3. **La recomendación:** el asistente corre en la terminal del sistema (o en
   Claude Desktop), **abierta por la madre en la carpeta del proyecto y ya
   configurada**, y la madre ve en vivo lo que hace. Para el estudiante es una
   entrada de menú; para nosotros, días y no semanas. La terminal empotrada
   queda como fase posterior **sólo si** la externa se queda corta.
4. **El MCP crece con la escalera:** US y MT después de la fase 3; VARMA con
   drvarma_gui; VECM con drvec_gui. El empaquetado de Python en Windows y
   macOS, que es lo más incierto, se ensaya ya, en paralelo (§8).

---

## 1. Lo que hay hoy

### 1.1 Los tres MCP

| | **art** (`art-mcp`) | **mtram** (`mtram`) | **sima** (`sima`) |
|---|---|---|---|
| paquete | `art-tseries` 0.2.3 | `drtran` 0.2.6 | `sima-tseries` 0.2.0 (local; PyPI 0.1.1) |
| repositorio | `davidesg/art-python` | `davidesg/drtran-python` | `davidesg/sima-python` |
| clase | US | MT | VARMA y VECM |
| `mcp_server.py` | 12 530 líneas, 51 herramientas | 3 350 líneas, 21 | 2 446 líneas, 31 |
| carriles | guiado y autónomo | guiado y autónomo | guiado y autónomo |
| motor | `fue` Python (el C de fue compilado con cffi, `_fue_engine`) y `pyfug` | el **porte** Python de drtran, sobre `fue` y `drvarma` | `drvarma` (porte, verosimilitud compilada) y `drvec` (porte) |
| dependencias | numpy, scipy, pandas, matplotlib, openpyxl, mcp | numpy, scipy, fue, drvarma; `[mcp]`: matplotlib, jinja2, pyfug | drvarma, drvec, fue, pyfug, numpy, scipy, mcp |

**drvec ya está en sima**, como dice el analista: el nodo N1c
`prepare_vec` hace la pregunta de la cointegración, y `estimate_vec` y los
pasos C2-C8 estiman, diagnostican y prevén el VEC en niveles con el porte de
drvec. Un sistema lleva su clase, VARMA o VEC, y las herramientas finales la
respetan. Lo que dije antes («drvec no tiene MCP») estaba mal.

**Cómo tratan los ficheros.** Los tres siguen el convenio de la escalera:
leen `.inp` y `.pre` con el lector de fue (`fue.load`/`cast_us`), y sólo el
programa que estima escribe un `.pre`. art escribe además `.out`, gráficos y
un `guion.json` con los nodos y su razón; mtram escribe `.inp` (nunca `.pre`)
y sus `.out`; sima, sus `.out` y un guion propio.

### 1.2 Lo que no casa con la madre

La madre es **la otra encarnación del taller** (`DISENO-madre.md` §10): su
proyecto es `proyecto.yaml`, que `lib/proyecto` escribe y lee. Los MCP no lo
conocen:

| la madre tiene | los MCP hacen |
|---|---|
| `IPC/m03`, con padre, versión y razón en el manifiesto | ficheros con el nombre que diga el usuario |
| **el linaje no se pierde**: `pr_deriva()` lo escribe al copiar | nada; el linaje vive en su `guion.json` |
| casos `C1` con `motor` y sus entradas `serie/modelo@sha256` (`DISENO-casos.md` §2.3) | una lista de `.pre` que se pasa a mano |
| `_casos/C1/work/C1_c01.out …` | el `.out` donde caiga |
| el cuaderno, `cuaderno/cuaderno.md` con `img/` | nada |
| `m00` son los datos, y nadie los edita | `load_data` crea un `.inp` de datos nuevo |

**Y lo que sí casa:** la madre **ya relee el manifiesto y los `.out` por
huella** (tamaño + fecha) al recuperar el foco (`main.c:1829`,
`ventana.c:46`), con dos reglas pensadas para esto: un manifiesto roto por
fuera no se traga, y la escritura propia no se toma por ajena. Lo que haga
un proceso al lado ya aparece en la ventana cuando el analista vuelve a ella.

### 1.3 Los motores: los mismos, pero no los mismos binarios

La madre y sus GUI llaman a los **binarios en C** (`engines/fue`, `fuf`,
`fug`, `drtran`, `art`). Los MCP llaman a los **paquetes Python**:

- **US**: fue Python lleva el C de fue compilado. El modelo que sale es el
  mismo; los bytes del `.out` no (el informe lo escribe Python). La batería
  `conformidad/` lo mide así a propósito: compara **modelos**, no bytes.
- **MT, VARMA, VECM**: son **portes**. Cuadran con el C dentro de tolerancia
  (los oráculos son los C: drvec-v0.12 para drvec), no al byte.

Esto importa para el MTM (§2.3): un `.pre` que entra en el proyecto tiene
que poder abrirse en fue_gui o drtran_gui y **no moverse**.

---

## 2. Qué es «MTM» en concreto

### 2.1 Una capa, no tres copias

**MTM no es copiar los tres servidores y retocarlos.** Son 18 000 líneas con
su historia de bugs; una copia se queda atrás en un mes. MTM es **un servidor
nuevo y delgado, `atsw-mcp`**, que:

- **importa** las funciones de análisis de art, drtran y sima como
  bibliotecas, con versión fijada;
- **sustituye** lo que choca con la madre: la gestión de ficheros y de
  proyecto, y la entrada de datos;
- **añade** lo que la madre sabe y ellos no: series, modelos, casos, linaje
  y cuaderno.

Un solo servidor con **juegos de herramientas por clase**, `us_*`, `mt_*`,
`varma_*` y `vecm_*`, como los menús de la barra. Se encienden por fases
(§8). Las instrucciones del servidor, que es donde vive el método (los
carriles, las preguntas obligatorias), se heredan de las de cada MCP y se
reescriben para el proyecto.

### 2.2 Qué se queda, qué sale y qué se añade

| | herramientas | por qué |
|---|---|---|
| **se quedan**, envueltas | US: identificación (`guided_identification`, `unit_root_analysis`, `seasonal_analysis`…), estimación y diagnosis (`confirm_and_estimate`, `estimate_and_diagnose`, `overparameterization_analysis`), contrastes formales y MEG (`formal_tests`, `meg_frequency`, `meg_reformulate`), intervención (`intervention_ladder`, `test_interventions`…), previsión. MT: `identify_link`, `refine_link`, `set_network`, `estimate`, `diagnose`, `overfit`, `forecast`. VARMA/VECM: las de sima | es el método; no se reescribe |
| **salen** | `load_data`, `create_inp`, `preview_data` (los datos entran por Datos de la madre: `m00`); los `guion_*` y `export_guion` (el `guion.json` es de la otra encarnación, P4); `load_example` de sima, salvo decisión | chocan con el proyecto |
| **a decidir** | `build_model` y `batch_build`, el atajo auto-ARIMA | art ya dice que no es el carril autónomo; para estudiantes puede ser mejor no ofrecerlo (decisión 6) |
| **nuevas** | `proyecto_resumen` (series, modelos, casos, elegidos); `modelo(serie, id)` que da la ruta; `derivar` (nuevo `mNN` desde un `.pre`, con padre); `razon` y `elegir`; `caso_nuevo` (MT, con las entradas y su hash); `cuaderno_apunte` (texto y figura al cuaderno); `comprobar` (qué está instalado, con versiones) | el idioma de la madre |

**La razón, gratis.** `DISENO-madre.md` §8 bis no pide razón en la GUI
porque un modal «¿por qué?» se contesta `asdf`. El asistente sí la da:
`guion_node` de art la rechaza vacía y 881 de 924 nodos la llevan. En el MTM,
la razón del nodo va **a la razón del modelo** en el manifiesto.

### 2.3 Los motores: análisis en Python, el óptimo con el motor de la madre

Dos caminos, y es una decisión (decisión 3):

- **(A) Todo en Python**, como hoy. Simple; los `.pre` del asistente vienen
  de fue Python o de los portes. Para US el modelo es el mismo; para MT, a
  tolerancia, y entonces drtran_gui al abrirlo **puede mover** los números.
- **(B) El análisis en Python, la estimación que entra en el proyecto con el
  binario de la madre** (`fue`, `drtran`, los mismos que van en el paquete).
  Así el `.pre` es exactamente el que daría la GUI, y el invariante «corre
  fue sobre un `.pre` y no se mueve» se cumple con el mismo programa.

**Recomiendo B.** Es la regla de siempre: *sólo el programa que estimó
puede afirmar un óptimo*. Y en el proyecto, ese programa tiene que ser el de
la madre. Cuesta poco: el MTM llama al binario como ya lo hace la madre
(`lib/sitio` lo encuentra), y Python sólo lee el `.out`.

### 2.4 El manifiesto: un solo escritor

El manifiesto lo escribe `lib/proyecto`, en C, con un subconjunto de YAML
que **rechaza** lo que no entiende. Si el MTM lo escribe en Python, ya hay
dos escritores, y la batería de conformidad del `.inp` enseña lo que eso
cuesta (seis implementaciones, dos huecos que nadie ve). Por eso:

> **El MTM no escribe `proyecto.yaml`.** Llama a una herramienta de línea
> de órdenes nueva, `atsw-proyecto`, compilada de `lib/proyecto` y empaquetada
> con la madre: `atsw-proyecto deriva P IPC m02`, `… razon P IPC m03 "…"`,
> `… caso-nuevo P drtran IPC/m10 WTI/m03`. Leer con `yaml.safe_load` sí
> puede, porque `lib/proyecto` lo garantiza.

Es lo mismo que decidimos para los `.pre`: un solo lector, el de fue; nunca
un segundo.

### 2.5 Dónde vive y cómo se versiona

**Recomiendo el monorepo**, en `mcp/atsw_mcp/`, con su `pyproject.toml`:

- el contrato (los nombres `mNN`, los casos, el manifiesto) es de la madre, y
  cambia con ella en el mismo PR;
- las pruebas que importan cruzan los dos lados: el MTM escribe y la madre lo
  ve (§7);
- art-python, drtran-python y sima-python siguen en sus repositorios y se
  usan como dependencias fijadas, con un `uv.lock` en el monorepo.

**Versión**: la de ATSW. `atsw-mcp` 0.1.6 va con ATSW 0.1.6 y fija versiones
exactas de art-tseries, drtran, sima-tseries, fue, drvarma, drvec y pyfug.
`comprobar` las enseña, y la comprobación de arranque de la madre (TODO)
también.

**Lo que hay que resolver antes de fijar:**

- **drvec no está en PyPI**, y fue 0.1.18 tampoco: los dos esperan al
  release conjunto (memoria del proyecto). sima 0.2.0, con el VEC, tampoco
  está publicado.
- Los tres piden `mcp>=1.0,<2`; en PyPI ya hay `mcp` 2.3. Hay que fijar la 1.x
  o subir los tres a la 2.

---

## 3. La terminal en Windows, macOS y Linux

Comprobado otra vez, y la tabla de `DISENO-madre.md` §10 se mantiene, con un
matiz (libvterm):

| opción | Windows | macOS | Linux | coste | comentario |
|---|---|---|---|---|---|
| **la terminal del sistema**, abierta por la madre | sí: Windows Terminal (`wt -d DIR`), PowerShell o cmd | sí: Terminal.app (`open -a Terminal DIR`, o AppleScript para lanzar la orden) | sí: `x-terminal-emulator`, gnome-terminal… | **días** | la ventana es otra, pero la abre la madre ya configurada |
| **VTE** (`vte-2.91`) | **no** | no oficial | sí | bajo en Linux | depende de los pty de Unix; el README de VTE no promete más que GTK. En macOS hay un port en MacPorts (0.82), sin soporte del proyecto. Aquí ni está instalado |
| **terminal propia** con `libvterm` | ConPTY (Windows 10 1809+) | `forkpty` | `forkpty` | **semanas**, y mantenerla | libvterm (MIT, C, lo usa Neovim) da el parser y el estado de pantalla; el dibujo en `GtkDrawingArea`, el teclado, el ancho Unicode y el redimensionado son nuestros. Claude Code es una TUI exigente: pantalla alterna, colores, ratón |
| **panel de conversación propio** (no es terminal) | sí | sí | sí | **medio-alto** | ver §3.1 |
| **Claude Desktop** como cliente, sin terminal | sí | sí | beta | **casi cero** | ver §4 |

### 3.1 El panel propio: lo que cuesta de verdad

Un `GtkTextView` con la conversación, que enseña filas, gráficos y `.out`
como tales. Es lo más bonito, pero:

- **Es un cliente de un LLM, no sólo del MCP.** El bucle agéntico, los
  permisos y el contexto tendríamos que hacerlos nosotros, o usar el Agent
  SDK de Anthropic. El SDK es Python o TypeScript, y se autentica con **clave
  de API** (Console, pago por uso): cada estudiante, o la universidad,
  necesitaría una. Usar su suscripción de claude.ai desde un producto
  nuestro no es lo previsto; habría que comprobar las condiciones.
- `DISENO-madre.md` §10 ya lo advertía: *«meter aquí la autenticación y el
  bucle agéntico sería reimplementar Claude Code dentro de una ventana de
  GTK, y mantenerlo»*.

Queda como opción para más adelante, no para empezar.

---

## 4. El cliente

| cliente | Windows | macOS | cuenta | sin red | para estudiantes |
|---|---|---|---|---|---|
| **Claude Code** (CLI) | sí, nativo desde Windows 10 1809 (`irm https://claude.ai/install.ps1 \| iex`, sin administrador; Git for Windows opcional, sin él usa PowerShell) | sí, macOS 13+ | Pro, Max, Team, Enterprise o Console. **El plan gratuito no incluye Claude Code** | **no**: necesita red | potente, pero es una terminal |
| **Claude Desktop** | sí | sí | también con el plan gratuito; que los MCP **locales** entren en el gratuito hay que comprobarlo | no | lo más amable: ventana, sin terminal. Un MCP local se instala editando su `claude_desktop_config.json` o con un `.mcpb` de doble clic |
| otros clientes MCP (VS Code, LM Studio, goose…) | sí | sí | cada uno la suya | **algunos sí, con un modelo local** | posible, pero con modelos locales el método se resiente; no lo daría como camino de la asignatura |
| nuestro panel (§3.1) | sí | sí | clave de API | no | caro |

**Dos consecuencias para la asignatura:**

- **Sin red no hay asistente**, con ningún cliente razonable. La madre
  funciona igual sin él, y así tiene que seguir: el asistente es un añadido,
  nunca un requisito.
- **La cuenta es lo que más pesa.** Claude Code exige plan de pago; Claude
  Desktop no. Si la universidad tiene un acuerdo (Claude for Education u
  otro), cambia todo. Es una decisión del analista (decisión 2), y condiciona
  qué cliente va primero.

**Configuración por proyecto.** Claude Code lee un `.mcp.json` de la carpeta
del proyecto: la madre lo escribe al abrir el asistente y apunta a
`atsw-mcp --proyecto .`. Claude Desktop tiene una configuración global, no
por carpeta, así que allí el MTM toma el proyecto de un fichero que la madre
mantiene con **el proyecto abierto** (por ejemplo
`~/.config/atsw/proyecto-activo`), o de una herramienta `abrir_proyecto`.

---

## 5. El empaquetado

### 5.1 Lo que hay hoy

`.github/workflows/paquetes.yml` construye:

- en Windows, con MSYS2 UCRT64, un zip portátil y un instalador Inno Setup
  (sin administrador);
- en macOS, una `.app` en `.dmg` para arm64, x86_64 y x86_64-macos11, con
  dylibbundler;
- en Linux, un `.deb`.

Todo es **C**: `packaging/programas.sh` lista los binarios, sin Python. La
prueba de empaquetado corre los motores en un entorno limpio, sin MSYS2 ni
Homebrew. Tamaños de la v0.1.4: instalador de Windows 22 MB, `.dmg` arm64 15
MB.

### 5.2 Cómo meter Python

| | qué es | tamaño (comprimido, aprox.) | pegas |
|---|---|---|---|
| **Python embebido + ruedas** | Windows: el «embeddable zip» de python.org; macOS: `python-build-standalone` (el que usa uv), reubicable. Se instalan las ruedas binarias dentro | +80-120 MB (numpy, scipy, pandas, matplotlib son el grueso; scipy sola son ~110 MB sin comprimir) | firmar y notarizar en macOS cada `.so`; el instalador pasa de 22 a ~130 MB |
| **PyInstaller** | un ejecutable congelado | parecido | falsos positivos de antivirus en Windows, arranque lento en modo un fichero |
| **instalación bajo demanda con uv** | la madre lleva `uv` (un binario) y, la primera vez, «Instalar el asistente…» descarga Python y las ruedas a `%LOCALAPPDATA%\ATSW\asistente` o `~/Library/Application Support/ATSW/asistente`, con el `uv.lock` fijado | el instalador apenas crece (lo que pese uv) | necesita red **la primera vez**, que el asistente necesita igual |

**Recomiendo bajo demanda con uv.** El asistente necesita red de todos
modos, la madre sigue pesando 22 MB para quien no lo use, y el `uv.lock`
garantiza que todos los estudiantes tienen las mismas versiones. Es la
decisión 4.

**Lo que hay que comprobar en el ensayo (§8):**

- que hay **ruedas binarias** para Windows x64, macOS arm64 y macOS x86_64 de
  fue, drvarma, drvec y pyfug (drvec no está publicado: §2.5);
- que el C compilado de fue y la verosimilitud de drvarma **cargan** desde
  ese Python (ya pasó una vez que las ruedas instalaban sin poder cargar el
  motor: faltaba cffi);
- el tamaño real descargado;
- macOS 11 en Intel: las ruedas de scipy recientes ya no lo cubren.

### 5.3 La comprobación de arranque

El TODO «Después del diseño de la madre» ya pide comprobar al arrancar que
todo está instalado. El asistente entra en esa lista como **opcional**:
«asistente: no instalado (Instalar…)», «asistente: instalado, `atsw-mcp`
0.1.6; Claude Code no encontrado (cómo instalarlo)». Lo opcional no se pinta
como error.

---

## 6. El lado de la madre

### 6.1 Ver lo que hace el asistente, en vivo

La relectura al recuperar el foco ya existe (§1.2). Con la terminal al lado,
el analista **mira** la madre sin pulsar en ella, así que hace falta además
un `GFileMonitor` sobre `proyecto.yaml` y los `.out`:

- GLib tiene vigilancia de ficheros en los tres sistemas
  (ReadDirectoryChangesW en Windows, kqueue o FSEvents en macOS, inotify en
  Linux);
- se agrupan los avisos (medio segundo) y se relee con **la misma función**
  que el foco, con sus dos reglas: no tragarse un manifiesto a medio escribir,
  y no tomar la escritura propia por ajena;
- el modelo nuevo aparece en la rejilla con una marca breve, y la barra de
  estado dice «m04 de IPC, del asistente».

### 6.2 El linaje: quién dio el paso

El manifiesto gana un campo, **sólo cuando no es lo normal** (como `rol`):

```yaml
  IPC/m04:
    version: 4
    padre: m03
    razon: "el MA(12) sobra: t = 0,4"
    por: asistente
```

La ventana del linaje lo enseña. Así un profesor ve qué decidió el
estudiante y qué el asistente, que en la asignatura es lo importante.

### 6.3 La regla de seguridad: sólo dentro del proyecto

**El asistente sólo escribe dentro de la carpeta del proyecto.** Se hace
cumplir en el MTM, no en el cliente:

- toda ruta se resuelve contra la raíz (`realpath`) y se rechaza si sale de
  ella, con enlaces simbólicos incluidos;
- `m00` y los datos son de sólo lectura (`DISENO-madre.md`: «nadie los
  edita»);
- el MTM no ofrece órdenes de sistema.

En el cliente, una segunda capa: el `.claude/settings.json` que la madre
escribe con el `.mcp.json` permite las herramientas de `atsw-mcp` y niega
las de escribir y ejecutar. En Windows nativo Claude Code no tiene sandbox,
así que **la regla del MTM es la que vale**.

### 6.4 Opcional: el MCP de la propia madre

Un paso más, para después: que el asistente **mueva la interfaz**, no sólo
los ficheros. Por ejemplo «abre m03 en fue_gui», «enséñame el vistazo»,
«marca el caso C1». La madre escucharía en un socket local (`GSocketService`;
AF_UNIX existe en Windows 10 1803+, y si no, TCP en 127.0.0.1 con una ficha
aleatoria), y el MTM le mandaría órdenes. **No hace falta para empezar.** Va
en la fase final (§7) y sólo si se echa de menos.

### 6.5 La maqueta

```
┌ ATSW — SF_MEG ─────────────────────────────────────────────────────────┐
│ Proyecto  Datos  US  MT  Asistente  Ayuda                              │
│                          ┌──────────────────────────────────────────┐  │
│                          │ Abrir el asistente en una terminal  ⌃⇧A  │  │
│                          │ Usar el asistente en Claude Desktop…     │  │
│                          │ ──────────────────────────────────────── │  │
│                          │ Qué ha hecho el asistente…               │  │
│                          │ Instalar el asistente…                   │  │
│                          └──────────────────────────────────────────┘  │
│ Series — US       │ IPC  muestra completa                              │
│  IPC        ●     │  m00  datos                                        │
│  WTI              │  m01  ARIMA(0,1,1)(0,1,1)12     σ 0,412            │
│                   │  m02  + AI 2020-03              σ 0,398   elegido  │
│ Casos — MT        │  m03  + MA(12) libre            σ 0,397            │
│  C1  IPC ← WTI    │  m04  − MA(12)        ✦ asistente   σ 0,398        │
├───────────────────┴────────────────────────────────────────────────────┤
│ m04 de IPC, del asistente: «el MA(12) sobra: t = 0,4»                  │
└────────────────────────────────────────────────────────────────────────┘

┌ Windows Terminal — C:\Users\ana\ATSW\SF_MEG ───────────────────────────┐
│ > ¿el MA(12) de m03 se gana su sitio?                                  │
│ ● atsw · us_overparameterization (IPC/m03)                             │
│   θ12 = 0,031  t = 0,41 → no                                           │
│ ● atsw · derivar IPC m03 → m04, sin el MA(12)                          │
│ ● atsw · us_estimar IPC/m04 (fue)                                      │
│   σ 0,398, Q(24) = 19,3: limpio. Lo apunto en el cuaderno.             │
└────────────────────────────────────────────────────────────────────────┘
```

**«Asistente» como menú propio**, no dentro de Proyecto. Va después de las
clases y se apaga entero, con su globo, si no está instalado. Con VARMA y
VECM la barra queda `Proyecto · Datos · US · MT · VARMA · VECM · Asistente
· Ayuda`. Es la decisión 7.

---

## 7. El plan, por fases

Cada fase en su PR, con CI en las tres plataformas.

**Fase 0: ensayo del empaquetado** (en paralelo, ya).
- Un job de CI en Windows y macOS que instala con `uv` un Python y las ruedas
  de art-tseries, drtran y sima-tseries con versiones fijadas, y corre una
  prueba de humo: cargar un `.pre`, estimar, sin red después de instalar.
- Mide el tamaño y comprueba las ruedas (§5.2).
- No toca la madre. El resultado decide la 4.

**Fase 1: `atsw-proyecto` y el MTM de US y MT.**
- `atsw-proyecto`, compilado de `lib/proyecto`, con sus pruebas, en
  `programas.sh`.
- `mcp/atsw_mcp/` con `us_*`, `mt_*` y las herramientas del proyecto (§2.2),
  y la estimación con el binario de la madre (si se decide B).
- **Pruebas** (pytest en los tres sistemas): sobre el proyecto de ejemplo
  IPC ← WTI, el MTM deriva, estima y abre un caso; `test_proyecto` y la madre
  leen el manifiesto resultante; el `.pre` del asistente no se mueve en fue;
  rutas fuera de la raíz rechazadas.

**Fase 2: la madre abre el asistente.**
- El menú Asistente; escribir `.mcp.json` y `.claude/settings.json`; lanzar
  la terminal de cada sistema (una función por plataforma, y la prueba
  comprueba la orden construida, no abre ventanas en CI); la entrada de
  Claude Desktop.
- `GFileMonitor` y el campo `por: asistente` en el linaje.
- **Pruebas**: en `test_gui`, una fase `asistente` que escribe un modelo con
  `atsw-proyecto` por fuera y comprueba que la rejilla lo enseña sin foco.
- «Instalar el asistente…» con uv, si se decide así.

**Hito drvarma_gui → `varma_*`.** Las herramientas de sima para VARMA, con
los casos `motor: drvarma`, cuando la madre ya los enseña.

**Hito drvec_gui → `vecm_*`.** Igual para el VEC.

**Fase final, si se echa de menos:** el MCP de la madre (§6.4) y, sólo si la
terminal externa se queda corta, la terminal empotrada con libvterm.

---

## 8. Cuándo: antes o después de drvarma_gui y drvec_gui

La pregunta del analista. Lo que pesa:

**1. ¿Depende el contrato del MTM del formato de los casos que fijan los
hitos?** Para MT, **no**. El caso MT está fijado desde `DISENO-casos.md`
(`schema_version: 2`, `motor`, `entradas` con hash, `_casos/C1/work/`), y la
«Clase» que añade el hito drvarma_gui **se deduce de `motor`**: la fase 2 de
la madre ya tiene `atsw_clase_de()`. Para VARMA y VECM, **sí**: drvarma_gui
todavía no acepta `--proyecto` y `--caso`, y la disposición de sus corridas
en el caso no está escrita. Si el MTM escribiera casos VARMA ahora, el
contrato cambiaría dos veces.

**2. US y MT están estables** después de la fase 3. Es lo que el estudiante
usa primero en la asignatura, y donde el asistente rinde antes.

**3. El riesgo de que el asistente llegue más lejos que la madre.** Con sima
dentro del MTM antes de los hitos, el asistente produciría sistemas VARMA y
VEC que la madre **no sabe enseñar**: ficheros sueltos fuera del manifiesto,
que es justo lo que los casos vinieron a evitar con el botón drtran sin caso.
Por eso **el MTM no expone VARMA ni VECM hasta que la madre los enseña**. Un
estudiante que quiera VARMA antes puede usar sima tal cual, fuera del
proyecto, como hoy.

**4. El empaquetado no depende de nada**, y es lo que más puede sorprender
(ruedas, notarización, tamaño). Conviene saberlo pronto.

**5. El coste en los hitos.** Hacer US y MT antes retrasa drvarma_gui lo que
duren las fases 1 y 2. Hacerlo todo después deja la madre sin asistente
hasta que haya una GUI nueva entera (drvec_gui), que es el hito más largo.

**Recomendación: el MCP crece con la escalera**, que es la candidata del
analista, y la evaluación la confirma:

```
fase 3 de la madre ─→ fases 1 y 2 (US + MT) ─→ hito drvarma_gui + varma_* ─→ hito drvec_gui + vecm_*
fase 0 (ensayo de empaquetado), en paralelo desde ya ──┘
```

Así cada clase llega al asistente **con** su menú en la madre, nunca antes,
y el contrato de cada caso se escribe una sola vez.

---

## 9. Riesgos

- **La cuenta de los estudiantes** (§4). Si no hay acuerdo, el asistente es
  para quien pague, y la asignatura no puede depender de él. Por eso la madre
  sigue completa sin asistente.
- **Las versiones de Claude Code cambian cada semana.** La integración se
  limita a lo estable: `.mcp.json`, `.claude/settings.json`, lanzar `claude`.
  Nada de depender de detalles de su interfaz.
- **El método en manos del asistente.** Los MCP actuales ya lo imponen (los
  carriles, las preguntas obligatorias, `build_model` no es autónomo); el MTM
  hereda esas instrucciones, y el `por: asistente` deja a la vista quién
  decidió.
- **Dos encarnaciones.** El MTM respeta P4: escribe el proyecto de la madre
  con la herramienta de la madre y deja el `guion.json` a ATSW Python.

---

## 10. Decisiones abiertas

Numeradas para contestar como en `ESTUDIO-madre-organizacion.md` §8.

1. **Terminal empotrada o externa.** Recomiendo **externa** (la del sistema,
   abierta y configurada por la madre) y Claude Desktop como alternativa; la
   empotrada con libvterm, sólo en la fase final si se echa de menos.
2. **El cliente de la asignatura y la cuenta.** ¿Claude Code (plan de pago),
   Claude Desktop (también con el gratuito, por comprobar con MCP locales), o
   ambos? ¿Hay acuerdo de la universidad?
3. **Los motores del MTM.** Recomiendo **B**: el análisis en Python y la
   estimación que entra en el proyecto con los binarios de la madre (§2.3).
4. **Python: dentro del instalador o bajo demanda.** Recomiendo **bajo
   demanda con uv** (§5.2), a confirmar con la fase 0.
5. **Dónde vive el MTM.** Recomiendo **el monorepo**, `mcp/atsw_mcp/`, con
   art-python, drtran-python y sima-python como dependencias fijadas (§2.5).
6. **El atajo auto-ARIMA** (`build_model`, `batch_build`): ¿se ofrece a los
   estudiantes o sale del MTM?
7. **El menú «Asistente»**, propio y después de las clases (§6.5), o dentro
   de Proyecto.
8. **El campo `por: asistente`** en el linaje (§6.2), escrito sólo cuando no
   es lo normal.
9. **Cuándo** (§8). Recomiendo que **el MCP crezca con la escalera**: US y MT
   después de la fase 3; VARMA con drvarma_gui; VECM con drvec_gui; el ensayo
   de empaquetado, ya y en paralelo.
10. **El MCP de la madre** (§6.4), para mover ventanas: ¿se anota para la
    fase final o se descarta?

---

**Estado: propuesto, a la espera de las decisiones.**

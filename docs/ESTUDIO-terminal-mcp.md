# Un asistente en la madre: la terminal, los MCP a medida y cuándo

Estudio del 19-oct-2026, a petición del analista:

> *«¿Qué posibilidades tenemos de introducir en atsw-gui un terminal, y que
> ese terminal tenga un MCP que pueda manejar los motores de atsw-gui?»*

Y sus precisiones, el mismo día:

> *«Los MCP serían MTM (Made to Measure) de los que tenemos en Python. Sería
> necesario que funcione en Windows y Mac, porque los usuarios de Linux son
> marginales y a los estudiantes no les puedo obligar a tener Linux. drvec ya
> funciona con sima.»*

> *«Es importante que maneje los binarios en C, además son mucho más
> rápidos.»*

> *«El uso que se da en clase es un ejemplo de cómo se puede utilizar para
> enseñar, pero no debe condicionar su desarrollo… Se puede usar en modo
> guiado y automático. La docencia no condiciona el diseño. Su uso va a ser
> más general.»*

Es un estudio con propuesta y plan, **sin código**. **El analista ya ha
decidido casi todo** (§11), y lo que sigue está escrito con esas decisiones.
**Los usuarios son analistas e investigadores en general**; la práctica 5 de
la asignatura (P05) es un ejemplo de uso, del que aquí sólo se toman sus
lecciones técnicas (§3). Continúa una consulta anterior, la del 23-sep-2026,
anotada en `DISENO-madre.md` §10 («una consola de verdad NO, un panel de
conversación SÍ»). Lo que ha cambiado desde entonces, y por eso se reabre:

- los tres MCP en Python han madurado (art 0.2.3, drtran 0.2.6, sima 0.2.0
  con el VEC);
- buena parte del análisis de art ya está **en C**, en `lib/`, portado para
  la madre y cotejado con art-python número a número (§2.2);
- la madre tiene casos MT, barra de menús por clase y relectura por huella;
- y el analista pide ahora MCP **a medida** de la madre, sobre sus motores
  en C.

---

## 0. El veredicto, primero

1. **Lo que hace falta no es una terminal: es un MCP que hable el idioma del
   proyecto y maneje los motores de la madre.** Hoy los tres MCP trabajan
   sobre ficheros sueltos, con su propia gestión (`guion.json`) y sobre los
   paquetes Python; no saben qué es `m03`, ni un caso, ni el linaje, ni el
   cuaderno. El MTM es esa capa (§2), y es el 80 % del valor.
2. **El camino principal es C** (decidido). El MTM es una capa Python
   delgada —el protocolo MCP, las instrucciones, los dos carriles— que llama
   a los binarios de la madre (fue, fuf, fug, art, drtran, y drvarma y drvec
   cuando lleguen) y a una orden nueva, `atsw-cli`, que expone sin ventana lo
   que `lib/` ya calcula. Python queda sólo para lo que no tiene equivalente
   en C, con su inventario (§2.2).
3. **Los dos carriles, guiado y autónomo, son de primera.** El autónomo es el
   de art: los mismos nodos, con el asistente en la silla del analista y la
   razón de cada decisión por escrito. No es `build_model` (§2.3).
4. **La terminal es la del sistema** (decidido). La madre la abre en la
   carpeta del proyecto, ya preparado para el cliente de cada uno: Claude
   Code, CodeWhale, Codex o Gemini CLI (§4, §5). **Después, empotrada**,
   como hace Kate: VTE en Linux, y en Windows y macOS un widget propio con
   libvterm y ConPTY si un ensayo de una semana lo justifica, siempre con la
   externa detrás (§5.1).
5. **El asistente abre las ventanas de gráficos de la madre**, no dibujos
   suyos: `atsw_gui --ver …` abre una sola ventana, de sólo lectura, con el
   mismo gráfico que se ve en la madre (§7.4). Es imprescindible con los
   modelos que no ven imágenes, y es el embrión del MCP de la madre.
6. **Lo que hace el asistente queda en el linaje** como `por: asistente`:
   se sabe siempre quién decidió cada paso (§7.2).
7. **El MCP crece con la escalera** (decidido): US y MT después de la fase 3;
   VARMA con drvarma_gui; VECM con drvec_gui. Python se instala bajo demanda
   con uv, y el ensayo de empaquetado empieza ya, en paralelo (§9).

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
drvec.

**Y la suite que los junta, `atsw` en PyPI** (2.0.0), instala los tres. Su
orden **`atsw-mcp` no es un servidor**: es un instalador que registra los tres
en Claude Code (`atsw/mcp_install.py`). Su cabecera explica por qué no son
uno solo, y el argumento vale para el MTM (§2.1): *las instrucciones de los
tres se contradicen a propósito*. art y sima empiezan con «¿guiado o
autónomo?» y mtram con «¿cuál es la salida?»; mtram declara y contrasta la
exogeneidad, y sima supone todo endógeno.

**Cómo tratan los ficheros.** Los tres siguen el convenio de la escalera:
leen `.inp` y `.pre` con el lector de fue, y sólo el programa que estima
escribe un `.pre`. art escribe además `.out`, gráficos y un `guion.json` con
los nodos y su razón. Los gráficos son PNG de matplotlib: con `ART_VIEWER=1`
art abre cada uno en una ventana, y `ART_FIG_DIR` dice dónde se guardan.

### 1.2 Lo que no casa con la madre

La madre es **la otra encarnación del taller** (`DISENO-madre.md` §10): su
proyecto es `proyecto.yaml`, que `lib/proyecto` escribe y lee. Los MCP no lo
conocen:

| la madre tiene | los MCP hacen |
|---|---|
| `IPC/m03`, con padre, versión y razón en el manifiesto | ficheros con el nombre que diga el usuario |
| **el linaje no se pierde**: `pr_deriva()` lo escribe al copiar | nada; el linaje vive en su `guion.json` |
| casos `C1` con `motor` y sus entradas `serie/modelo@sha256` (`DISENO-casos.md` §2.3) | una lista de `.pre` que se pasa a mano |
| el cuaderno, `cuaderno/cuaderno.md` con `img/` | nada |
| `m00` son los datos, y nadie los edita | `load_data` crea un `.inp` de datos nuevo |
| los motores en C, y las ventanas de la escuela (el lienzo de fue, de fug) | los paquetes Python, y PNG de matplotlib |

**Y lo que sí casa:** la madre **ya relee el manifiesto y los `.out` por
huella** al recuperar el foco (`main.c:1829`, `ventana.c:46`), con dos reglas
pensadas para esto: un manifiesto roto por fuera no se traga, y la escritura
propia no se toma por ajena.

### 1.3 La regla de diseño que esto aprovecha

`DISENO-madre.md` §10 lo dejó escrito el 20-sep-2026, pensando en esto: *«todo
lo que la madre sepa hacer tiene que poder hacerse sin la ventana. Es decir,
la lógica en `lib/proyecto`, `lib/datos` y `lib/tabla`, y `gui/atsw` sólo como
vista.»* Se ha cumplido más allá de esas tres: los contrastes formales, las
raíces, los anómalos y el veredicto de la diagnosis también viven en `lib/`,
sin GTK en su cálculo. Falta sólo **la puerta sin ventana**: hoy esas
funciones sólo se llaman desde las GUI. Esa puerta es `atsw-cli` (§2.2).

---

## 2. Qué es «MTM» en concreto

### 2.1 Una capa delgada, tres servidores

**MTM no es copiar los tres servidores y retocarlos**, ni reescribirlos.
Es **una capa delgada en Python** que:

- **habla MCP** y lleva las instrucciones de cada clase, con sus dos
  carriles;
- **llama a C** para todo lo que C sabe hacer (§2.2);
- **importa de art, drtran y sima** sólo lo que no tiene equivalente en C;
- **añade** el idioma de la madre: series, modelos, casos, linaje, cuaderno y
  sus ventanas de gráficos.

**Tres servidores, no uno**, por la razón que ya da la suite (§1.1): cada
clase tiene sus propias instrucciones y su propia pregunta inicial.

| servidor | clase | instrucciones de | motores en C | se enciende |
|---|---|---|---|---|
| `atsw-us` | US | art | fue, fuf, fug, art | fase 1 |
| `atsw-mt` | MT | mtram | drtran (y fue para los univariantes) | fase 1 |
| `atsw-sis` | VARMA, y VECM después | sima | drvarma, drvec | VARMA con drvarma_gui; el VEC, con drvec_gui |

Comparten un módulo, `atsw_mcp.comun`, con las herramientas del proyecto y
las de las ventanas.

**Los nombres, sin chocar con la suite.** `atsw-mcp` ya existe y un usuario
puede tener los dos. El código vive en `mcp/atsw_mcp/` (decidido), la
distribución se llama **`atsw-gui-mcp`**, y las órdenes son `atsw-us`,
`atsw-mt` y `atsw-sis`; **ninguna se llama `atsw-mcp`**.

### 2.2 C primero: qué hace C y qué queda en Python

**El camino principal es C** (decisión 11). Por tres razones: es el motor
que la madre usa, así que lo que hace el asistente es exactamente lo que
haría el analista en la GUI, y un `.pre` suyo no se mueve al abrirlo en
fue_gui; es **mucho más rápido**, y el autónomo encadena decenas de
estimaciones; y quita a la instalación casi todo el peso de Python (§6.2).

**Por dónde se llega a C:**

- **los motores**, que ya son órdenes: `fue` (estimar, diagnosis, el gráfico
  `-c`), `fuf` (prever), `fug` (listado de identificación, histograma `-d`,
  media-DT), `art` (el identificador: propone órdenes con su evidencia,
  `DATA_art.cand`), `drtran`; y `drvarma` (con su escalera, `escalera.c`) y
  `drvec` con sus hitos;
- **`atsw-cli`, una orden nueva** compilada de `lib/`, con subórdenes y
  salida en texto y JSON: `atsw-cli proyecto deriva …`, `atsw-cli raices
  adf …`, `atsw-cli formales meg …`. Es lo que la madre ya calcula, sin la
  ventana. Sustituye a la `atsw-proyecto` de la versión anterior de este
  estudio, que era sólo su primera suborden.

**El inventario.** Por herramienta de los MCP de hoy, adónde va en el MTM.
«C» quiere decir que ya existe en C y se coteja con art-python número a
número; «C, a comprobar», que la pieza en C existe pero hay que confirmar
que cubre lo mismo; «Python», que no hay equivalente en C.

| herramienta (MCP de hoy) | en el MTM | dónde |
|---|---|---|
| **US** | | |
| `unit_root_analysis` (ADF, KPSS, d = 0, 1, 2) | C | `lib/raices` (los contrastes de `engines/art/src/unit_root_tests.c`) |
| `ar_factorization` | C | `lib/raices` |
| `formal_tests`: Shin-Fuller, DCD regular y estacional, sobre y subdiferenciación, simplificación estacional | C | `lib/formales`, reestimando con fue |
| `meg_frequency`, MEG y DCD_f | C | `lib/formales` |
| `meg_reformulate` | C | `lib/formales` + `pr_deriva` («Reformular como estocástica…») |
| `test_seasonal_simplification` | C | `lib/formales` |
| identificación de órdenes (el candidato) | C | `engines/art` + `lib/artcand` (`an_identifica_*`) |
| `seasonal_analysis` | C, a comprobar | `engines/art/src/seasonal_detection.c`; la F estacional ya está en `an_identifica_datos` |
| `estimate_and_diagnose`, `confirm_and_estimate` (estimar) | C | `fue`, con el reinicio de `lib/formales` |
| diagnosis en veredictos | C | `lib/dictamen`, `lib/verdict`, `lib/outfile`, `lib/gof` |
| `preliminary_outlier_scan`, `residual_outlier_scan`, `residual_episodes` | C, a comprobar | `lib/anomalos` (con la calibración) |
| `suggest_intervention_form`, `test_interventions` | C, a comprobar | `lib/intervencion`, `lib/inpdet`, `an_sugerir` |
| `model_histogram` | C | `fug -d` |
| `model_equation_display` | C, a comprobar | la ecuación de fue_gui |
| `generate_forecast`, `update_and_forecast` | C | `fuf` |
| `boxcox_analysis` | Python, con el gráfico en C | el media-DT es `fug`; la λ recomendada es de art-python |
| `overparameterization_analysis` | Python sobre C | orquesta estimaciones de `fue` |
| `seasonal_param_analysis`, `incident_configurations`, `intervention_ladder` | Python | art-python; estiman con `fue` |
| `guided_identification` (el pipeline de nodos, el `objetivo`) | Python | es el protocolo: se queda en la capa |
| el veredicto encadenado de los contrastes (`describe_formal_tests`) | Python | **no se portó a C a propósito** (BUG-0215, BUG-0232); el MTM da cada contraste con su número |
| `verify_optimum`, `compare_versions`, `extend_sample` | Python sobre C | orquestan `fue` |
| `full_report`, `save_identification_report`, `sps_dashboard` | Python | informes |
| **MT** | | |
| `identify_link`, `refine_link`, `plot_ccf` | C | `lib/prewhiten`, `lib/ccfplot` |
| `set_network`, `check_operators` | C | `lib/netfile` (el `.dag`), `lib/slots` (el `.cns`) |
| `estimate` | C | `drtran` |
| `diagnose` | C | `lib/outdiag`, `lib/diagmv` |
| `forecast` | C | `drtran` + `lib/outfcst` |
| `impulse_response`, `variance_decomposition` | C, a comprobar | lo que imprime `drtran` |
| `identify_network`, `overfit`, `calibrate`, `evaluate` | Python sobre C | orquestan `drtran` |
| **VARMA y VECM** (con sus hitos) | | |
| estimar, la escalera desde los `.pre` de fue | C | `drvarma` (`escalera.c`), `drvec` |
| diagnosis multivariante | C | `lib/diagmv` |
| `identify_echelon`, `canonical_analysis`, `identify_matrices`, `study_estimation`, `prepare_vec` | Python, a revisar en cada hito | sima |

**Lo que queda en Python es de dos clases.** Una es **orquestación**:
sobreparametrizar, verificar un óptimo, evaluar fuera de muestra, recorrer
una red; son bucles sobre el motor, y en el MTM esos bucles llaman al binario
en C. La otra es **análisis sin equivalente en C** (la λ de Box-Cox, la
escalera de intervenciones, la identificación echelon). Esa se importa de
art-python o sima tal cual, con versión fijada. Cuando una de ellas se porte a
C para la madre, como pasó con los contrastes formales, pasa al camino de C
sin cambiar la herramienta.

### 2.3 Los dos carriles, y el atajo auto-ARIMA

**Los dos carriles son de primera.** El MTM hereda de cada MCP su pregunta
inicial y sus dos carriles:

- **guiado**: el analista decide cada nodo; el asistente enseña la evidencia,
  propone alternativas y para;
- **autónomo**: el asistente recorre **los mismos nodos** y decide cada uno,
  con su razón por escrito, y entrega el recorrido y el modelo.

**El atajo auto-ARIMA (`build_model`, `batch_build`) no es el autónomo.**
Es un ajuste de una llamada, con las reglas de la escuela pero sin nodos: no
sobreparametriza, no mira Semana Santa, no pasa los contrastes formales ni
reformula. El propio art lo advierte (BUG-0180): un «autónomo» que se reduce a
esa llamada es lo peor de los dos.

**La decisión 6, revisada.** En la versión anterior lo saqué del MTM con
razones de docencia, que ya no valen. Sobre el diseño:

- **en contra de incluirlo**: deja en el proyecto modelos **sin el recorrido
  que los justifica**, que es lo que el linaje y la razón existen para
  guardar; y es todo Python (el pipeline de art), así que sería el único
  camino que produce modelos sin pasar por los motores en C;
- **a favor**: para un investigador con muchas series, `batch_build` da una
  primera pasada rápida, un punto de partida o una referencia, y art lo
  admite cuando se pide expresamente.

**Decidido: fuera de la fase 1** (decisión 14). Más adelante, si se quiere,
como una herramienta explícita, **«ajuste automático»**, que estima con el
`fue` de la madre y deja cada modelo en el linaje con `por: asistente` y la
razón «ajuste automático, sin recorrido».

### 2.4 Qué sale y qué se añade

| | herramientas | por qué |
|---|---|---|
| **salen** | `load_data`, `create_inp`, `preview_data` (los datos entran por Datos de la madre: `m00`); los `guion_*` y `export_guion` (el `guion.json` es de la otra encarnación, P4); `load_example` de sima, salvo decisión | chocan con el proyecto |
| **nuevas** | `proyecto_resumen` (series, modelos, casos, elegidos); `modelo(serie, id)` que da la ruta; `derivar` (nuevo `mNN` desde un `.pre`, con padre); `razon` y `elegir`; `caso_nuevo` (MT, con las entradas y su hash); `cuaderno_apunte` (texto al cuaderno); `ver` (abre una ventana de gráficos de la madre, §7.4); `comprobar` (qué está instalado, con versiones) | el idioma de la madre |

**La razón, gratis.** `DISENO-madre.md` §8 bis no pide razón en la GUI
porque un modal «¿por qué?» se contesta `asdf`. El asistente sí la da:
`guion_node` de art la rechaza vacía y 881 de 924 nodos la llevan. En el MTM,
la razón del nodo va **a la razón del modelo** en el manifiesto.

### 2.5 El manifiesto: un solo escritor

El manifiesto lo escribe `lib/proyecto`, en C, con un subconjunto de YAML
que **rechaza** lo que no entiende. Si el MTM lo escribiera en Python habría
dos escritores, y la batería de conformidad del `.inp` enseña lo que eso
cuesta. Por eso **el MTM no escribe `proyecto.yaml`**: llama a `atsw-cli
proyecto` (`deriva`, `razon`, `elige`, `caso-nuevo`). Leer con
`yaml.safe_load` sí puede, porque `lib/proyecto` lo garantiza. Es lo mismo que
decidimos para los `.pre`: un solo lector, el de fue; nunca un segundo.

### 2.6 Dónde vive y cómo se versiona

**En el monorepo** (decidido): `mcp/atsw_mcp/` con su `pyproject.toml`, y
`atsw-cli` junto a la madre, en `gui/atsw-cli/` (decisión 16), compilado con
el resto y en `programas.sh`. El contrato (los nombres `mNN`, los casos, el manifiesto, las
subórdenes de `atsw-cli`) cambia con la madre en el mismo PR, y las pruebas
que importan cruzan los dos lados (§8).

**Versión**: la de ATSW. `atsw-gui-mcp` 0.1.6 va con ATSW 0.1.6 y con su
`atsw-cli`. Las dependencias Python (sólo las de lo que queda en Python) van
fijadas con un `uv.lock`, como la P05 fijó `art-tseries==0.2.2` y funcionó.
`comprobar` enseña las versiones, y la comprobación de arranque (TODO)
también.

**Lo que hay que resolver antes de fijar:**

- **drvec no está en PyPI**, y fue 0.1.18 y sima 0.2.0 tampoco: esperan al
  release conjunto. No bloquea la fase 1, y con C primero pesa menos.
- Los tres piden `mcp>=1.0,<2`; en PyPI ya hay `mcp` 2.3. Hay que fijar la 1.x
  o subir los tres a la 2.

---

## 3. Lo que enseñó la P05

La práctica 5 de la asignatura (13-oct-2026) fue el primer uso real de art
con un asistente en ordenadores ajenos, en Windows y macOS: cada grupo
construyó y comparó modelos del IPC en su portátil. **Es un ejemplo de uso,
no el que guía el diseño**; aquí van sólo sus lecciones técnicas. Los datos,
los nombres y las notas no salen de la carpeta de la asignatura.

### 3.1 La instalación

- **uv con versión fijada funcionó**: `uv tool install art-tseries==0.2.2` y
  `uv tool update-shell`, sin instalar Python aparte.
- **El plan B fue pip** (Python de python.org con «Add python.exe to PATH»;
  `pipx` en macOS).
- **Lo que falló en Windows fue PowerShell**, no uv: las políticas de
  ejecución bloquean el `irm … | iex` del instalador. Las salidas fueron
  `winget install astral-sh.uv` u otra terminal.
- **Los Mac con chip Apple necesitan macOS 12** o posterior: scipy no
  publica ruedas para macOS 11 en arm64. Los Intel funcionan desde macOS 11.

**Consecuencia:** la madre lleva **su propio `uv`** y lo llama directamente;
no hay script de PowerShell que bloquear (§6.2). Y con C primero, lo que uv
instala es mucho menos.

### 3.2 Los clientes

Funcionaron cuatro, y el analista ha tenido éxito con varios modelos,
Claude incluido:

| cliente | cómo se conecta el MCP | instrucciones | nota |
|---|---|---|---|
| **CodeWhale + DeepSeek** | `~/.codewhale/mcp.json` (global) | lee `AGENTS.md` | el camino probado en la P05, con una clave personal de DeepSeek |
| **Claude Code** | `claude mcp add --scope user … --env …` | `CLAUDE.md`, que carga `@AGENTS.md` | |
| **Codex** | `~/.codex/config.toml` (global) | lee `AGENTS.md` | |
| **Gemini CLI** | `~/.gemini/settings.json`, con `timeout: 600000` | `.gemini/settings.json` del proyecto con `context.fileName: AGENTS.md` | el único sin confirmar en la P05 |

### 3.3 Los ficheros de instrucciones

- **Un `AGENTS.md` común**, al que cada cliente llega por su camino:
  `CLAUDE.md` con `@AGENTS.md` para Claude Code, `.gemini/settings.json` para
  Gemini, y CodeWhale y Codex lo leen solos.
- **Hay un límite de ~50 KB**: CodeWhale descarta **entero** un fichero de
  instrucciones más grande. Las instrucciones del servidor de art son 51 KB.
- **Hay clientes que no pasan las instrucciones del servidor al modelo.**
  Por eso iba una copia íntegra de reserva, `instrucciones_art_servidor.md`.
- **Para comprobar que se ha cargado**: pedirle que cite el título.

**Consecuencia:** las instrucciones de cada servidor tienen que caber con
holgura (objetivo: menos de 20 KB), y la madre genera ese andamiaje en cada
proyecto (§7.1).

### 3.4 Los gráficos

- **DeepSeek no ve imágenes.** Los gráficos los ve sólo el usuario.
- **`ART_VIEWER=1` abre cada gráfico en una ventana** (en Windows, sin la
  variable, no se abre ninguna), y `ART_FIG_DIR` guarda los PNG donde se le
  dice. WezTerm y kitty, con `imgcat`, eran opcionales.

**Consecuencia:** con un modelo que no ve, **la ventana de gráficos es la
mitad del análisis**. Es la razón de que la decisión 10 tenga una parte que
no espera al final (§7.4).

---

## 4. El cliente

| cliente | Windows | macOS | cuenta | sin red |
|---|---|---|---|---|
| **Claude Code** | sí, nativo desde Windows 10 1809, sin administrador; Git for Windows opcional | sí, macOS 13+ | Pro, Max, Team, Enterprise o Console; el plan gratuito no lo incluye | no |
| **CodeWhale + DeepSeek** | sí (`winget`) | sí | clave de DeepSeek | no |
| **Codex** | sí | sí | ChatGPT | no |
| **Gemini CLI** | sí (Node.js 20+) | sí | cuenta de Google | no |
| Claude Desktop | sí | sí | también el gratuito; los MCP locales en el gratuito, por comprobar | no |

**Cuentas:** sin acuerdo con la universidad (decisión 3), cada usuario usa la
suya; para la asignatura, el sistema de la P05.

**Optimizado para Claude, pero no atado a él** (decisión 2):

- **todo en texto**: cada herramienta devuelve los números que sostienen la
  decisión, y el gráfico va aparte, en su ventana (§7.4);
- **nada que sólo entienda un cliente**: ni sintaxis propia de Claude en las
  instrucciones, ni herramientas que dependan de leer imágenes;
- **tiempos de espera largos** para las estimaciones largas (los 10 minutos
  que la P05 puso en Gemini), aunque con C se necesiten menos;
- **instrucciones cortas** (§3.3);
- **una prueba de CI sin cliente de LLM**: una sesión MCP a pelo (§8).

**Sin red no hay asistente**, con ningún cliente. La madre funciona igual sin
él, y así tiene que seguir.

---

## 5. La terminal

**Decidido: la del sistema** (decisión 1). La madre la abre en la carpeta del
proyecto:

| | cómo |
|---|---|
| Windows | Windows Terminal (`wt -d DIR`) si está, que en Windows 11 viene de serie; si no, `cmd /k` en DIR. **Nunca un script de PowerShell** (§3.1) |
| macOS | Terminal.app (`open -a Terminal DIR`) |
| Linux | `x-terminal-emulator`, o la que diga `$TERMINAL` |

No lanza el cliente: abre la terminal **con un aviso de una línea** sobre qué
escribir según el cliente configurado.

### 5.1 Y empotrada: lo que hace Kate

El analista propone, el 20-oct-2026: *«en la versión Linux podríamos
integrar el terminal, y en las versiones en MS Windows y Mac he visto que
Kate empotra una consola del sistema. Revisar esta opción.»* Revisado en las
fuentes de KDE:

- **Kate no tiene terminal propia.** Su complemento (`addons/konsole/
  kateconsole.cpp`) carga en tiempo de ejecución **la KPart de Konsole**
  (`kf6/parts/konsolepart`) y la maneja con `TerminalInterface`. Si no la
  encuentra, pinta «Konsole not installed». No hay código distinto para
  macOS: la misma KPart en los tres sistemas.
- **En Windows, esa KPart es Konsole compilado para Windows.** El `Pty.cpp`
  de Konsole elige en compilación: `KPtyProcess` en Unix y, con `Q_OS_WIN`,
  `ConPtyProcess`, de `src/ptyqt/conptyprocess.cpp`, que crea una
  **pseudoconsola de Windows (ConPTY)** con `CreatePseudoConsole`. Ese
  fichero viene de la biblioteca ptyqt y lleva licencia **MIT**; el resto de
  Konsole es GPL-2.0-or-later.
- **Su historia.** El terminal de Kate en Windows llegó en la 23.04
  (febrero de 2023), primero sobre qtermwidget (una bifurcación vieja de
  Konsole) y ptyqt, y el propio equipo lo llamó *«una solución temporal»*:
  lo que querían era portar Konsole a Windows y reutilizarlo, que es lo que
  hay hoy. Avisaban también de que ningún desarrollador de Kate usa Windows y
  de que mantener una terminal entera dentro del editor *«es mucho peso»*.
- **En macOS**, la KPart usa los pty de Unix. Ha tenido problemas de teclado
  propios: en 2022, ^C y ^D pedían ⌘ en el `.dmg` de Kate, y Konsole lleva un
  mapa de teclas especial para macOS.
- **Es una terminal de verdad**, no una consola de órdenes: el emulador
  VT102/xterm de Konsole, con colores, pantalla alterna y redimensionado. Lo
  que necesitan Claude Code, Codex o Gemini CLI, que son TUI a pantalla
  completa. Y ConPTY es la misma pieza sobre la que corre Windows Terminal,
  así que en Windows esas TUI funcionan sobre ella.

**Lo que se puede llevar a GTK3, y lo que no.** La KPart es Qt y KDE
Frameworks: meterla en la madre es meter Qt. **No se transfiere el
componente; sí la receta**: un emulador como biblioteca, ConPTY en Windows,
los pty de Unix en macOS y Linux, y el dibujo en un widget propio. Para
GTK3 eso da estas opciones:

| opción | Linux | macOS | Windows | TUI a pantalla completa | coste | mantenimiento |
|---|---|---|---|---|---|---|
| **(1) VTE** (`vte-2.91`, LGPL) | **sí**, es el de GNOME Terminal | Homebrew tiene `vte3` 0.84 **sólo para arm64**; en Quartz no lo usa nadie de quien sepamos | **no**: depende de los pty de Unix. Un port que sólo mostraba (`vte_terminal_feed`) se propuso y se cerró sin entrar (GNOME/vte #1236) | perfectas en Linux | **~1 semana** en Linux | bajo: lo mantiene GNOME |
| **(2) widget propio: libvterm + ConPTY / forkpty**, dibujado en un `GtkDrawingArea` | sí | sí (`forkpty`) | sí (ConPTY, Windows 10 1809+) | buenas si el dibujo y el teclado están bien | **4-6 semanas** para que esté fino | **alto**: tres sistemas, teclado, IME, fuentes |
| (3) VTE en Windows y macOS con su emulador y nuestro pty | — | posible | habría que portar VTE (C++20, meson, sus dependencias) y mantener la bifurcación | buenas | muy alto | muy alto |
| (4) **reparentar la ventana de consola nativa** | — | **imposible**: macOS no deja empotrar la ventana de otro proceso | **no**: `SetParent` entre procesos es legal pero, en palabras de Raymond Chen, como hacer malabares con motosierras (ata las colas de entrada de los dos hilos); en Windows 11 la consola puede abrirse en Windows Terminal, que es una ventana con pestañas y no un `conhost`; y `GtkSocket`/`GtkPlug` son sólo de X11 | — | — | — |

**El widget propio, en detalle**, porque es el único camino a Windows y
macOS:

- **libvterm** (Paul Evans, **MIT**, C) es un emulador VT220/xterm sin
  dibujo: lleva el estado de la pantalla, la pantalla alterna, los colores
  (256 y color verdadero) y avisa de lo que cambia. Lo usan Neovim (en su
  propia copia) y emacs-libvterm, y la página de atribuciones de Qt Creator
  lista libvterm 0.3.3 junto a la parte ConPty de ptyqt: **es la misma
  receta**, en otro editor que funciona en los tres sistemas.
- **Lo nuestro:**
  - el pty: ConPTY en Windows (unos cientos de líneas sobre
    `CreatePseudoConsole`, como el `conptyprocess.cpp` MIT), `forkpty` en
    macOS y Linux;
  - el dibujo de la rejilla con Pango y cairo (anchos dobles, negrita,
    cursor, colores);
  - el teclado (de `GdkEventKey` a `vterm_keyboard_*`), con las teclas
    muertas del teclado español (ñ, tildes) y el IME;
  - el ratón, la selección y el portapapeles, el pegado entre corchetes, el
    historial hacia atrás y el redimensionado (`vterm_set_size` y
    `ResizePseudoConsole`).
- **Lo que cuesta de verdad es el teclado y el acabado**, no el emulador. Es
  lo que a Kate le dio guerra en macOS, y lo que más se nota con una TUI.

**Recomendación: una sola API y tres motores, con la externa siempre detrás.**

```c
GtkWidget *atsw_terminal_new( const char *dir, char **argv, GError **err );
/* NULL si en esta plataforma no hay terminal empotrada: entonces se abre
   la externa, que funciona siempre (§5). */
```

- **Ahora:** la terminal externa (decisión 1). Es la única que funciona en
  todas partes y desde el primer día.
- **Fase T1, Linux:** `atsw_terminal_new` con **VTE**, dentro de la ventana
  Asistente, acoplable a la madre (§7.6). Barato y sólido, y donde no esté, la
  externa.
- **Fase T2, Windows y macOS, después de un ensayo:** una semana de
  prototipo de libvterm + ConPTY en un `GtkDrawingArea`, en Windows, con
  Claude Code y Codex dentro. **Se sigue sólo si pasa**: colores y pantalla
  alterna, redimensionado sin basura, las tildes y la ñ con teclado español,
  copiar y pegar, y que no se note lento. Si pasa, el mismo widget con
  `forkpty` sirve en macOS (y en Linux, si se prefiere un solo motor).
- **La externa no se va nunca**: es el respaldo si el widget falla, y la
  opción de quien prefiera su terminal.

---

## 6. El empaquetado

### 6.1 Lo que hay hoy

`.github/workflows/paquetes.yml` construye un zip portátil y un instalador
Inno Setup en Windows (MSYS2 UCRT64), una `.app` en `.dmg` para arm64,
x86_64 y x86_64-macos11, y un `.deb`. Todo es **C**: `packaging/programas.sh`
lista los binarios. Tamaños de la v0.1.4: instalador de Windows 22 MB, `.dmg`
arm64 15 MB.

**Con C primero, el grueso del asistente ya va en el paquete**: los motores y
`atsw-cli`, que se añade a `programas.sh`. drvarma y drvec entran con sus
hitos, que es cuando el MTM los necesita.

### 6.2 Python bajo demanda, con el uv de la madre

**Decidido: bajo demanda con uv** (decisión 4):

- **El paquete lleva el binario de `uv`** junto a los motores. No instala
  nada al instalar ATSW.
- **Asistente ▸ «Instalar el asistente…»** llama a ese `uv` directamente,
  sin PowerShell y sin tocar el PATH. Instala un Python y `atsw-gui-mcp` con
  su `uv.lock` en `%LOCALAPPDATA%\ATSW\asistente` o
  `~/Library/Application Support/ATSW/asistente`.
- **Lo que instala es poco.** El núcleo de `atsw-gui-mcp` sólo necesita
  `mcp`. Lo que queda en Python (§2.2) trae numpy, scipy y, si se importa
  art-python entero, pandas y matplotlib. Esas dependencias son **un extra
  opcional**, `atsw-gui-mcp[python]` (decisión 15): su falta sólo apaga las
  herramientas que lo necesitan, y cada una lo dice.
- **El plan B, pip**, en el manual, como en la P05.

**Lo que hay que comprobar en el ensayo (fase 0):**

- las ruedas binarias de lo que quede en Python, en Windows x64, macOS arm64
  y macOS x86_64;
- el tamaño real descargado, con y sin el extra, y el del `uv` del paquete;
- que cada cliente arranca los servidores en la carpeta desde la que se
  lanza (§7.1).

### 6.3 La comprobación de arranque

El TODO «Después del diseño de la madre» ya pide comprobar al arrancar que
todo está instalado. El asistente entra como **opcional**: «asistente: no
instalado (Instalar…)», «asistente: `atsw-gui-mcp` 0.1.6; clientes
encontrados: claude, codewhale». `atsw-cli` va en la lista de los motores, que
no es opcional. Lo opcional no se pinta como error.

---

## 7. El lado de la madre

### 7.1 El menú Asistente y el andamiaje del proyecto

**Menú propio** (decisión 7), después de las clases: `Proyecto · Datos · US
· MT · Asistente · Ayuda`, y con VARMA y VECM, `… · VECM · Asistente ·
Ayuda`. No se apaga nunca entero: sin el asistente instalado, sus entradas
de trabajo se grisan con su globo y quedan «Ventana del asistente» e
«Instalar el asistente…», que es donde se arregla.

| entrada | qué hace |
|---|---|
| **Ventana del asistente** (casilla, `<Primary><Shift>A`) | la enseña o la esconde (§7.6) |
| **Acoplar a la madre** (casilla) | la pega al lado derecho de la madre, o la suelta |
| — | |
| **Abrir la terminal del sistema** | la externa, en la carpeta del proyecto (§5). Siempre está, haya o no terminal empotrada |
| **Preparar el proyecto para el asistente…** | el andamiaje de abajo |
| — | |
| **Qué ha hecho el asistente…** | el linaje filtrado por `por: asistente` (§7.2) |
| **Instalar el asistente…** / **Comprobar el asistente…** | con el `uv` del paquete (§6.2); una vez instalado, la entrada cambia a comprobar, que enseña versiones y clientes |

«Preparar el proyecto para el asistente…» pregunta el cliente (o varios) y
escribe en la carpeta del proyecto:

| fichero | para quién | qué lleva |
|---|---|---|
| `AGENTS.md` | todos | cómo trabajar en un proyecto de ATSW: los nombres, el linaje, las ventanas, que los datos no se tocan. **Menos de 50 KB, con margen**. No elige carril: eso lo pregunta cada servidor |
| `CLAUDE.md` | Claude Code | `@AGENTS.md` |
| `.gemini/settings.json` | Gemini CLI | `context.fileName: AGENTS.md`, y los servidores con `timeout: 600000` |
| `.mcp.json` | Claude Code | los servidores del proyecto (la primera vez Claude Code pide aprobarlos) |
| `instrucciones_servidor.md` | los clientes que no pasan las instrucciones del servidor | la copia íntegra, generada con `atsw-us --instrucciones` y las de los otros |

`AGENTS.md` es del usuario: la madre lo escribe si no existe y no lo pisa.
Quien quiera fijar un procedimiento lo escribe ahí. Por ejemplo, un profesor
puede limitar a sus estudiantes al carril guiado, como se hizo en la P05.

Para **CodeWhale y Codex**, cuya configuración es global, la madre **enseña el
bloque** y se ofrece a añadirlo, con confirmación; nunca reescribe el fichero
entero. Como esa configuración no sabe de proyectos, el servidor **busca el
proyecto desde la carpeta en la que arranca**, subiendo hasta encontrar
`proyecto.yaml`.

La configuración lleva además `ATSW_GUI` y `ATSW_BIN`, las rutas de la madre
y de los motores del paquete, para que el MTM use **esos** binarios y no
otros que haya en el PATH.

**Las claves, nunca en el proyecto.** Cada cliente guarda la suya en su
configuración de usuario. Nada de lo que la madre escribe en el proyecto
lleva una clave: un proyecto se comparte, se copia y se publica.

### 7.2 El linaje: quién dio el paso

El manifiesto gana un campo, **sólo cuando no es lo normal**, como `rol`
(decisión 8):

```yaml
  IPC/m04:
    version: 4
    padre: m03
    razon: "el MA(12) sobra: t = 0,4"
    por: asistente
```

**Para qué: la trazabilidad.** En un proyecto en el que trabajan el
analista, a mano en la madre, y un asistente, en guiado o en autónomo, el
linaje tiene que decir **quién dio cada paso**. Si no, dentro de seis meses no
se distingue una decisión pensada de una tomada por el asistente. Es la
misma regla que `rol` y que «sin razón se ve como sin razón»: lo que no se
registra no se puede saber después.

- **Todo lo que escribe el MTM lleva `por: asistente`**, en los dos carriles.
  El servidor no puede saber quién tecleó la orden, y no lo finge.
- **Lo que el analista hace en la madre no lleva marca**, porque es lo
  normal: elegir un modelo, escribir su razón, derivar.
- La ventana del linaje y la rejilla enseñan la marca.

La P05 dejó una observación técnica que apoya hacerlo así: lo que dice un
fichero de instrucciones no garantiza quién decide, porque el usuario se lo
puede levantar al asistente con una frase. El registro tiene que estar en el
manifiesto, no en el texto.

### 7.3 Ver lo que hace el asistente, en vivo

La relectura al recuperar el foco ya existe (§1.2). Con la terminal al lado,
el analista **mira** la madre sin pulsar en ella, así que hace falta además
un `GFileMonitor` sobre `proyecto.yaml` y los `.out`:

- GLib vigila ficheros en los tres sistemas (ReadDirectoryChangesW, kqueue o
  FSEvents, inotify);
- se agrupan los avisos (medio segundo) y se relee con **la misma función**
  que el foco, con sus dos reglas;
- el modelo nuevo aparece en la rejilla con una marca breve, y la barra de
  estado dice «m04 de IPC, del asistente».

### 7.4 El asistente abre las ventanas de gráficos

Decisión 10: el MCP de la madre va al final, **pero el asistente tiene que
poder abrir las ventanas de gráficos ya**.

| | qué es | a favor | en contra |
|---|---|---|---|
| (a) `ART_VIEWER`, como los MCP de Python | el MTM dibuja un PNG con matplotlib y lo abre con el visor del sistema | ya existe; no toca la madre | **es otro dibujo**: se compararía el gráfico del asistente con el de la madre creyendo que se comparan dos modelos. `plothost.h` explica por qué la madre enlaza el lienzo de fue en vez de redibujarlo |
| (b) `atsw_gui --ver`, una ventana suelta | el MTM lanza la madre con argumentos y ésta abre **sólo esa ventana**, en un proceso aparte | el mismo gráfico de la madre; el código ya está hecho para esto; no hace falta canal | un proceso por ventana; no sabe de la madre abierta |
| (c) el canal con la madre abierta | un socket local; la madre abierta abre la ventana | la ventana sale en la madre del analista | es el MCP de la madre entero, que va al final. `GtkApplication` único no sirve: en Windows y macOS no hay bus de sesión de D-Bus |

**Recomiendo (b).** Es pequeño porque las ventanas ya están desacopladas:
las de `lib/analisis` piden a quien las abre un `AnHost`, y todos sus
punteros a función pueden ser NULL («entonces ese gesto no se ofrece»).
fue_gui y la madre ya son dos anfitriones distintos; `--ver` es un tercero,
mínimo:

```
atsw_gui --proyecto P --ver QUE SERIE[/MODELO] [--muestra M]

  QUE          ventana                                   de dónde sale
  serie        serie y ACF/PACF, con λ, d y D al pie     el vistazo, modo 0
  media-dt     media - desviación típica                  el vistazo, modo 1
  histograma   histograma (fug -d)                        el vistazo, modo 2
  residuos     residuos con anómalos + ACF/PACF (fue -c)  an_anomalos / an_diagnosis
  prevision    la previsión, en fue_gui                   fue_gui --modelo … --prever (ya existe)
```

- **Sólo lectura.** El `AnHost` de `--ver` no tiene `guarda` ni `abre`: no
  deriva, no escribe el manifiesto, no lanza nada. Sí tiene «Al cuaderno»: lo
  que va al cuaderno lo decide quien mira.
- **Un proceso aparte.** La madre ya es `G_APPLICATION_NON_UNIQUE`, así que
  una segunda copia no molesta a la abierta. Desde Python se lanza separado
  (en Windows con `DETACHED_PROCESS`), y la herramienta vuelve en seguida.
- **Lo que hay que mover:** el vistazo vive hoy en `gui/atsw/src/vistazo.c` y
  recibe la `Atsw` entera. Pasa a `lib/analisis` con un anfitrión como
  `AnHost`, igual que las demás ventanas. Es el único cambio de fondo.
- **La herramienta `ver(que, serie, modelo)`** lanza `$ATSW_GUI --ver …` y
  **devuelve en texto** los números que el gráfico enseña, sacados de
  `atsw-cli`: las ACF y PACF con sus bandas, la Q, los anómalos con su fecha.
  Así un modelo que no ve imágenes tiene la evidencia, y el usuario, la
  ventana.
- **MT**, de momento, abre drtran_gui con `--caso`, que ya existe. Sus
  gráficos sueltos (la CCF, la respuesta al impulso) van con el MCP de la
  madre.
- **Si no encuentra la madre**, cae en (a), avisando de que es otro dibujo.

**Es el embrión del MCP de la madre.** El vocabulario de `--ver` (qué
ventana, de qué serie y modelo) es el de las órdenes que el socket aceptará
después. Cuando llegue (c), cambia el transporte, no las órdenes.

### 7.5 La regla de seguridad: sólo dentro del proyecto

**El asistente sólo escribe dentro de la carpeta del proyecto.** Se hace
cumplir en el MTM, no en el cliente, porque los clientes son varios y no
todos tienen permisos (en Windows nativo, Claude Code no tiene sandbox):

- toda ruta se resuelve contra la raíz (`realpath`) y se rechaza si sale de
  ella, con enlaces simbólicos incluidos;
- `m00` y los datos son de sólo lectura (`DISENO-madre.md`: «nadie los
  edita»);
- el MTM no ofrece órdenes de sistema. Sólo lanza los binarios del paquete
  (`ATSW_BIN`), `atsw-cli` y `atsw_gui --ver`, siempre sin shell (como
  `lib/engine`) y con argumentos que construye él;
- ninguna clave en el proyecto (§7.1).

Donde el cliente lo permite, una segunda capa: el `.claude/settings.json` que
la madre escribe con el `.mcp.json` permite las herramientas de los
servidores y niega las de escribir y ejecutar.

### 7.6 La ventana Asistente, acoplable

**Decidido: una ventana propia, «Asistente», que se acopla a la madre**
(decisión 18), no un panel fijo abajo. Se puede tener suelta (en otro
monitor, junto al cliente) o pegada a la madre.

**Cómo se acopla. GTK3 no tiene acoplamiento propio**, así que se evaluó:

| | qué es | veredicto |
|---|---|---|
| **libgdl (GDL)** | la biblioteca de acoplamiento de GNOME (Anjuta, gtranslator) | **no**: está en el archivo de GNOME (`gitlab.gnome.org/Archive/gdl`), la última versión es la 3.40.0, y arrastrar paneles es mucho más de lo que hace falta para una sola ventana |
| **arrastrar para acoplar**, a mano | detectar que la ventana se suelta sobre el borde de la madre | **no**: la posición de las ventanas la decide el gestor de ventanas, y en Wayland una aplicación ni siquiera la conoce; en tres sistemas sería frágil |
| **un conmutador «Acoplar / Desacoplar»** que mueve el contenido entre una `GtkWindow` y un lado de la madre | `g_object_ref` del contenido, `gtk_container_remove` de donde está, `gtk_container_add` en el otro sitio, `g_object_unref` | **sí** |

**El conmutador, en detalle:**

- **Un solo contenido, `AtswAsistente`**, un `GtkBox` con la terminal (o la
  página de estado, abajo) y una barra fina arriba. Es lo que se mueve; la
  ventana suelta y el hueco de la madre son sólo dos sitios donde se pone.
- **La madre gana un `GtkPaned` horizontal por fuera**: a la izquierda lo
  que hay hoy (listas y modelos, con su separador de siempre), **a la
  derecha el Asistente** cuando está acoplado. Desacoplado, el lado derecho
  desaparece y la madre recupera su ancho.
- **A la derecha**, porque la izquierda ya es de las listas, abajo le
  quitaría alto a la rejilla de modelos (y es lo que el analista descartó),
  y una terminal necesita ~80 columnas, que caben al lado en una pantalla
  ancha.
- **El proceso sigue vivo al moverlo.** La sesión (el pty de VTE, o la
  ConPTY del widget propio) es del objeto, no de su `GdkWindow`: al cambiar de
  sitio el widget se desrealiza y se vuelve a realizar, y el proceso hijo ni
  se entera. La referencia de `g_object_ref` impide que `gtk_container_remove`
  lo destruya. Es lo primero que comprueba la prueba (§8). Después se le
  devuelve el foco.
- **Cerrar la ventana suelta la esconde**, no mata la sesión. La sesión
  termina al cerrar el proyecto o con «Terminar la sesión» en la barra del
  Asistente; si hay un proceso vivo al cerrar el proyecto, la madre pregunta.
- **Los tres sistemas igual.** Es GTK puro: no hay ventanas de otros procesos
  ni nada del sistema de ventanas (lo que descartó la opción 4 de §5.1).

**Recuerda su estado por proyecto**: suelta o acoplada, visible o no, el
tamaño y la posición de la ventana suelta y la posición del separador
acoplado. **No va en `proyecto.yaml`**, que es el proyecto y se comparte;
va en un `GKeyFile` del usuario, `ventanas.ini` en
`g_get_user_config_dir()/atsw/`, con una sección por raíz de proyecto. Al
abrir el proyecto, el Asistente vuelve como estaba. La madre no guarda hoy
estado de ventanas en ningún sitio, así que este fichero es nuevo y servirá
para lo demás que se quiera recordar.

**Lo que enseña, según la plataforma**, con un `GtkStack` de dos páginas:

- **«terminal»**, cuando `atsw_terminal_new` da un widget (Linux con VTE desde
  T1; Windows y macOS si la fase T2 sale adelante). La terminal arranca en la
  carpeta del proyecto con el intérprete del usuario (`$SHELL` en Linux y
  macOS), y la barra de arriba dice el estado en una línea.
- **«estado»**, cuando no hay terminal empotrada (Windows y macOS antes de
  T2, o un Linux sin VTE). No es una ventana vacía:
  - el botón grande **«Abrir la terminal del sistema»**, en la carpeta del
    proyecto (§5);
  - **la instalación del asistente**: `atsw-gui-mcp` y su versión, si está
    el extra `[python]`, `atsw-cli`, y los clientes encontrados (claude,
    codewhale, codex, gemini), cada uno con «falta: cómo instalarlo» cuando
    no está;
  - si el proyecto está preparado para el asistente, con el botón para
    hacerlo;
  - **lo último que ha hecho el asistente** en este proyecto, leído del
    linaje (`por: asistente`), que se actualiza con el `GFileMonitor`
    (§7.3).

  La página «estado» también se ve con la terminal, plegada bajo la barra.

### 7.7 La maqueta

**Acoplada**, en Linux con VTE (fase T1):

```
┌ ATSW — SF_MEG ────────────────────────────────────────────────────────────────────────────┐
│ Proyecto  Datos  US  MT  Asistente  Ayuda                                                 │
│ Series — US    │ IPC  muestra completa                   ║ Asistente   ● claude    [⇱][×] │
│  IPC      ●    │  m00  datos                             ║ ~/ATSW/SF_MEG                  │
│  WTI           │  m01  ARIMA(0,1,1)(0,1,1)12   σ 0,412   ║ > ¿el MA(12) de m03 sobra?     │
│                │  m02  + AI 2020-03            σ 0,398 ★ ║ ● atsw-us · sobreparametrizar  │
│ Casos — MT     │  m03  + MA(12) libre ✦ asist. σ 0,397   ║   θ12 = 0,031  t = 0,41 → no   │
│  C1 IPC ← WTI  │  m04  − MA(12)       ✦ asist. σ 0,398   ║ ● atsw-us · derivar m03 → m04  │
│                │                                         ║ ● atsw-us · estimar (fue 0,2 s)│
│                │                                         ║ ● atsw-us · ver residuos m04   │
├────────────────┴─────────────────────────────────────────╨────────────────────────────────┤
│ m04 de IPC, del asistente: «el MA(12) sobra: t = 0,4»                                     │
└───────────────────────────────────────────────────────────────────────────────────────────┘
  ║ el separador de fuera (GtkPaned)       [⇱] desacoplar       ★ elegido por el analista
```

**Suelta**, con el menú Asistente abierto en la madre:

```
┌ ATSW — SF_MEG ─────────────────────────────────────────┐   ┌ Asistente — SF_MEG ──── [⇲][×] ┐
│ Proyecto  Datos  US  MT  Asistente  Ayuda              │   │ ● claude · atsw-us, atsw-mt    │
│                     ┌────────────────────────────────┐ │   │ > ...                          │
│                     │ ☑ Ventana del asistente   ⌃⇧A  │ │   │                                │
│                     │ ☐ Acoplar a la madre           │ │   │                                │
│                     │ ────────────────────────────── │ │   │                                │
│                     │ Abrir la terminal del sistema  │ │   │                                │
│                     │ Preparar el proyecto…          │ │   │                                │
│                     │ ────────────────────────────── │ │   │                                │
│                     │ Qué ha hecho el asistente…     │ │   │                                │
│                     │ Comprobar el asistente…        │ │   └────────────────────────────────┘
│                     └────────────────────────────────┘ │            [⇲] acoplar
└────────────────────────────────────────────────────────┘
```

**La página «estado»**, en Windows o macOS antes de T2 (acoplada o suelta,
igual):

```
┌ Asistente — SF_MEG ──────────────────────────── [⇱][×] ┐
│                                                        │
│        [  Abrir la terminal del sistema  ]             │
│        en C:\Users\…\ATSW\SF_MEG                       │
│                                                        │
│  La instalación                                        │
│   ✓ atsw-gui-mcp 0.1.6   ✓ extra [python]   ✓ atsw-cli │
│   ✓ claude   ✓ codewhale   — codex (cómo instalarlo)   │
│   ✓ el proyecto está preparado (AGENTS.md, .mcp.json)  │
│                                                        │
│  Lo último del asistente                               │
│   IPC/m04  − MA(12)        «el MA(12) sobra: t = 0,4»  │
│   IPC/m03  + MA(12) libre  «el residuo pide el 12»     │
│                                                        │
│  La terminal empotrada llegará a Windows con la fase   │
│  T2; mientras, se trabaja en la del sistema.           │
└────────────────────────────────────────────────────────┘
```

---

## 8. El plan, por fases

Cada fase en su PR, con CI en las tres plataformas.

**Fase 0: ensayo del empaquetado y de los clientes** (en paralelo, ya).
- Un job de CI en Windows y macOS que, con el binario de `uv`, instala un
  Python y el núcleo con versiones fijadas, con y sin el extra de Python, y
  corre una prueba de humo sin red después de instalar.
- A mano, en un Windows y un Mac: que Claude Code, CodeWhale, Codex y Gemini
  CLI arrancan los servidores en la carpeta desde la que se lanzan.
- No toca la madre.

**Fase 1a: `atsw-cli`, la puerta sin ventana.**
- Las subórdenes de lo que `lib/` ya calcula para US y MT: `proyecto`
  (deriva, razón, elige, caso nuevo), `raices`, `formales`, `anomalos`,
  `dictamen`, `prewhiten`, con salida en texto y JSON. En `programas.sh`.
- Cierra los «C, a comprobar» del inventario de US y MT (§2.2): lo que falte
  se apunta, no se improvisa.
- **Pruebas**: cada suborden da los mismos números que la ventana de la
  madre que la usa (la misma función, dos anfitriones), en los tres
  sistemas.

**Fase 1b: `--ver` y el MTM de US y MT.**
- `atsw_gui --ver`, con el vistazo movido a `lib/analisis` (§7.4).
- `mcp/atsw_mcp/` con `atsw-us` y `atsw-mt`, los dos carriles, las
  herramientas del proyecto y `ver`, sobre `atsw-cli` y los motores; lo que
  queda en Python, importado.
- **Pruebas** (pytest en los tres sistemas): sobre el proyecto de ejemplo
  IPC ← WTI, el MTM deriva, estima y abre un caso, en guiado y en autónomo;
  `test_proyecto` y la madre leen el manifiesto; el `.pre` del asistente no se
  mueve en fue; rutas fuera de la raíz rechazadas; las instrucciones caben en
  20 KB; una sesión MCP a pelo, sin cliente de LLM, recorre las herramientas.
  En `test_gui`, `--ver` abre cada ventana sin escribir nada en el proyecto.

**Fase 2: la madre y el asistente.**
- El menú Asistente (§7.1); «Preparar el proyecto para el asistente…» con
  el andamiaje; abrir la terminal de cada sistema (la prueba comprueba la
  orden construida, no abre ventanas en CI); «Instalar el asistente…» con el
  `uv` del paquete.
- **La ventana Asistente** en los tres sistemas, todavía con la página
  «estado» sólo: el conmutador Acoplar/Desacoplar, el `GtkPaned` de fuera, y
  `ventanas.ini` con el estado por proyecto (§7.6).
- `GFileMonitor` y el campo `por: asistente`.
- **Pruebas**: en `test_gui`, una fase `asistente` que escribe un modelo con
  `atsw-cli` por fuera y comprueba que la rejilla lo enseña sin foco y con la
  marca; el andamiaje generado (los ficheros, sin claves, el `AGENTS.md` por
  debajo de 50 KB, y que no pisa uno existente); acoplar y desacoplar diez
  veces sin perder el contenido, y que al reabrir el proyecto el Asistente
  vuelve como estaba.

**Hito drvarma_gui → `atsw-sis`, VARMA.** `drvarma` en el paquete, las
subórdenes multivariantes de `atsw-cli` y las herramientas de sima para VARMA,
con los casos `motor: drvarma`, cuando la madre ya los enseña. Se revisa qué
de sima tiene ya equivalente en C.

**Hito drvec_gui → el VEC en `atsw-sis`.** Igual para el VECM.

**Fase T1: la terminal empotrada en Linux** (§5.1). `atsw_terminal_new` con
VTE, en la página «terminal» de la ventana Asistente; donde no haya VTE, la
página «estado» y la externa. Puede ir en cuanto esté la fase 2, sin esperar
a los hitos.
- **Pruebas**: en `test_gui` (Linux), la terminal arranca un proceso en la
  carpeta del proyecto; **el proceso sigue vivo (mismo PID, mismo pty) tras
  acoplar y desacoplar**; y se cierra con el proyecto. En Windows y macOS,
  `atsw_terminal_new` devuelve NULL y se ve la página «estado».

**Ensayo T2 (una semana) y, si pasa, fase T2: Windows y macOS.** Un
prototipo de libvterm + ConPTY en un `GtkDrawingArea` con Claude Code y Codex
dentro, juzgado con los criterios de §5.1. Si pasa, el widget propio detrás
de la misma `atsw_terminal_new`, con ConPTY en Windows y `forkpty` en macOS
(4-6 semanas), con la misma prueba de acoplar y desacoplar sin perder la
sesión. Si no pasa, se queda la página «estado» con la externa, y se anota
por qué.

**Al final:** el MCP de la madre, con el canal (§7.4 c) y el vocabulario de
`--ver` ampliado.

**Una necesidad posible, sin plan:** un gesto que empaquete el proyecto
(manifiesto, modelos, cuaderno) para compartirlo o entregarlo. Se apunta por
si el uso lo pide.

---

## 9. Cuándo: antes o después de drvarma_gui y drvec_gui

**Decidido: el MCP crece con la escalera** (decisión 9). Las razones:

1. **El contrato de MT no depende de los hitos.** El caso MT está fijado
   desde `DISENO-casos.md` (`schema_version: 2`, `motor`, `entradas` con
   hash, `_casos/C1/work/`), y la «Clase» que añade el hito drvarma_gui se
   deduce de `motor`: la fase 2 de la madre ya tiene `atsw_clase_de()`. El de
   VARMA y VECM **sí** depende: drvarma_gui todavía no acepta `--proyecto` y
   `--caso`, y la disposición de sus corridas no está escrita.
2. **US y MT están estables** después de la fase 3, y su análisis ya está en
   C casi entero.
3. **El asistente no llega más lejos que la madre.** Sin el hito, produciría
   sistemas que la madre no sabe enseñar. Hasta entonces, quien quiera VARMA
   usa sima tal cual, fuera del proyecto.
4. **El empaquetado no depende de nada**, y es lo que más puede sorprender.

```
fase 3 de la madre ─→ fases 1a, 1b y 2 (US + MT) ─→ hito drvarma_gui + VARMA ─→ hito drvec_gui + VECM
fase 0 (ensayo de empaquetado y clientes), en paralelo desde ya ──┘
```

---

## 10. Riesgos

- **`atsw-cli` es una interfaz más que mantener.** La mitiga que sea la misma
  función que llama la ventana, con dos anfitriones, y que la prueba compare
  los dos.
- **Lo que queda en Python puede divergir de C** donde los dos existan. La
  regla: si hay C, se usa C; nunca las dos implementaciones para lo mismo en
  el MTM.
- **Los clientes cambian cada semana**, y son varios. La integración se
  limita a lo estable: la configuración MCP de cada uno, el fichero de
  instrucciones, lanzar la terminal. La fase 0 se repite antes de cada
  versión.
- **Dos encarnaciones.** El MTM respeta P4: escribe el proyecto de la madre
  con la herramienta de la madre y deja el `guion.json` a ATSW Python.
- **El widget de terminal propio** (fase T2) es el riesgo de mantenimiento
  más alto del estudio: teclado, IME y fuentes en tres sistemas. Por eso va
  detrás de un ensayo con criterios, y la externa no se retira nunca.
- **Dos órdenes parecidas.** `atsw-mcp` (de la suite) y `atsw-us`/`atsw-mt`
  (del MTM) pueden convivir. El manual dice cuál es cuál.

---

## 11. Decisiones (19-oct-2026)

1. **La terminal, externa primero**: la del sistema, abierta por la madre
   en la carpeta del proyecto (§5). La empotrada, después (17).
2. **El cliente: Claude Code o los alternativos.** El MTM se optimiza para
   Claude pero tiene que seguir funcionando con otros clientes (§4).
3. **Sin acuerdo con la universidad.** Cada usuario, su cuenta; en la
   asignatura, el sistema de la P05 (§3.2, §4).
4. **Python bajo demanda con uv**, con el `uv` dentro del paquete (§6.2).
5. **En el monorepo, `mcp/atsw_mcp/`.** La distribución, `atsw-gui-mcp`, y
   las órdenes, `atsw-us`, `atsw-mt` y `atsw-sis`, para no chocar con el
   `atsw-mcp` de la suite (§2.1).
6. **El atajo auto-ARIMA**: revisada, ya sin razones de docencia (§2.3); ver
   la 14.
7. **Menú «Asistente» propio**, después de las clases (§7.1).
8. **`por: asistente` en el linaje**, sólo cuando no es lo normal, por
   trazabilidad: quién dio cada paso (§7.2).
9. **El MCP crece con la escalera**: US y MT después de la fase 3; VARMA con
   drvarma_gui; VECM con drvec_gui; el ensayo de empaquetado, ya (§9).
10. **El MCP de la madre, al final; las ventanas de gráficos, ya**, con
    `atsw_gui --ver` (§7.4), que es su embrión.
11. **Los motores: los binarios en C de la madre**, como camino principal y
    no sólo para la estimación final. Python, la capa MCP y lo que no tiene
    equivalente en C (§2.2).
12. **Los dos carriles, guiado y autónomo, son de primera.** La docencia es
    un uso, no condiciona el diseño: no hay «modo de clase». Quien quiera
    limitar el carril lo escribe en el `AGENTS.md` del proyecto (§7.1).
13. **Sin «Preparar la entrega…» en el plan**; queda como necesidad posible
    (§8).

14. **El atajo auto-ARIMA, fuera de la fase 1**, con un «ajuste automático»
    explícito posible más adelante (§2.3).
15. **Lo que queda en Python, un extra opcional**: `atsw-gui-mcp[python]`
    (§6.2).
16. **`atsw-cli`**, con ese nombre, en el monorepo junto a la madre:
    `gui/atsw-cli/` (§2.6).

17. **La terminal empotrada, en este orden** (§5.1): fase T1 con VTE en
    Linux; un ensayo de una semana de libvterm + ConPTY en Windows con los
    criterios de paso de §5.1; y, si pasa, la fase T2 en Windows y macOS.
    Todo detrás de `atsw_terminal_new`, con la externa siempre de respaldo
    (20-oct-2026).
18. **Una ventana propia «Asistente», acoplable**, no un panel abajo con F4
    (§7.6): un conmutador Acoplar/Desacoplar que mueve el contenido entre una
    ventana suelta y el lado derecho de la madre, con el estado recordado por
    proyecto.

### Preguntas que quedan

Ninguna de fondo. Dos detalles que se pueden decidir al hacer la fase 2, y
que propongo así salvo que se diga otra cosa:

- **El atajo de la ventana**, `<Primary><Shift>A`, si no choca con otro de la
  madre.
- **El intérprete de la terminal empotrada en Windows** (fase T2):
  PowerShell si está, y si no `cmd`. Con la regla de §3.1: sólo se abre el
  intérprete, nunca se ejecuta un script.

---

**Estado: decidido.**

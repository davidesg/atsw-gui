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
drvarma_gui y drvec_gui de `ESTUDIO-madre-organizacion.md` §6 (aquí, §9).

Es un estudio con propuesta y plan, **sin código**. **El analista ya ha
decidido casi todo** (§11). Lo que sigue está escrito con esas decisiones y
con lo que enseñó la práctica 5 de la asignatura (P05), la primera en la que
los estudiantes trabajaron con art y un asistente en su propio ordenador
(§3). Continúa una consulta anterior, la del 23-sep-2026, que quedó anotada
en `DISENO-madre.md` §10 («una consola de verdad NO, un panel de conversación
SÍ»). Lo que ha cambiado desde entonces, y por eso se reabre:

- los tres MCP en Python han madurado (art 0.2.3, drtran 0.2.6, sima 0.2.0
  con el VEC);
- la madre tiene casos MT, barra de menús por clase y relectura por huella;
- la P05 ha probado en Windows y macOS, con estudiantes, la instalación y
  cuatro clientes distintos;
- y el analista pide ahora MCP **a medida** de la madre, no los genéricos.

---

## 0. El veredicto, primero

1. **Lo que hace falta no es una terminal: es un MCP que hable el idioma del
   proyecto.** Hoy los tres MCP trabajan sobre ficheros sueltos y llevan su
   propia gestión (`guion.json`); no saben qué es `m03`, ni un caso, ni el
   linaje, ni el cuaderno. El MTM es esa capa (§2), y es el 80 % del valor.
2. **La terminal es la del sistema** (decidido). La madre la abre en la
   carpeta del proyecto, con el proyecto ya preparado para el cliente que use
   cada uno: Claude Code, CodeWhale con DeepSeek, Codex o Gemini CLI (§4).
   Empotrarla en Windows sería un proyecto, no un widget (§5).
3. **El asistente abre las ventanas de gráficos de la madre**, no dibujos
   suyos: `atsw_gui --ver …` abre una sola ventana, de sólo lectura, con el
   mismo gráfico que el analista ve en la madre (§7.4). Es imprescindible para
   los modelos que no ven imágenes, como DeepSeek, y es el primer paso del MCP
   de la madre.
4. **En modo de clase, el asistente propone y el estudiante decide en la
   madre.** Lo que escribe el asistente entra en el linaje como `por:
   asistente`; un profesor ve a primera vista quién decidió cada paso (§7.2).
5. **El MCP crece con la escalera** (decidido): US y MT después de la fase 3;
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
drvec. Un sistema lleva su clase, VARMA o VEC, y las herramientas finales la
respetan.

**Y la suite que los junta, `atsw` en PyPI** (1.6.1 en la P05, 2.0.0 hoy),
instala los tres. Su orden **`atsw-mcp` no es un servidor**: es un
instalador que registra los tres en Claude Code (`atsw/mcp_install.py`). Su
cabecera explica por qué no son uno solo, y el argumento vale para el MTM
(§2.1): *las instrucciones de los tres se contradicen a propósito*. art y
sima empiezan con «¿guiado o autónomo?» y mtram con «¿cuál es la salida?»;
mtram declara y contrasta la exogeneidad, y sima supone todo endógeno.

**Cómo tratan los ficheros.** Los tres siguen el convenio de la escalera:
leen `.inp` y `.pre` con el lector de fue (`fue.load`/`cast_us`), y sólo el
programa que estima escribe un `.pre`. art escribe además `.out`, gráficos y
un `guion.json` con los nodos y su razón; mtram escribe `.inp` (nunca `.pre`)
y sus `.out`; sima, sus `.out` y un guion propio. Los gráficos son PNG de
matplotlib: con `ART_VIEWER=1` art abre cada uno en una ventana, y
`ART_FIG_DIR` dice dónde se guardan.

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
| las ventanas de gráficos de la escuela (el lienzo de fue, de fug) | PNG de matplotlib, otro dibujo parecido |

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

### 2.1 Una capa por clase, no tres copias

**MTM no es copiar los tres servidores y retocarlos.** Son 18 000 líneas con
su historia de bugs; una copia se queda atrás en un mes. MTM es **una capa
delgada** que:

- **importa** las funciones de análisis de art, drtran y sima como
  bibliotecas, con versión fijada;
- **sustituye** lo que choca con la madre: la gestión de ficheros y de
  proyecto, y la entrada de datos;
- **añade** lo que la madre sabe y ellos no: series, modelos, casos, linaje,
  cuaderno y sus ventanas de gráficos.

**Tres servidores, no uno**, por la razón que ya da la suite (§1.1): cada
clase tiene sus propias instrucciones y su propia pregunta inicial, y
mezclarlas en un bloque borra las distinciones que la escalera existe para
mantener. Corrijo así lo que decía la primera versión de este estudio (un
solo servidor con juegos de herramientas).

| servidor | clase | sobre | se enciende |
|---|---|---|---|
| `atsw-us` | US | art | fase 1 |
| `atsw-mt` | MT | mtram | fase 1 |
| `atsw-sis` | VARMA, y VECM después | sima | VARMA con drvarma_gui; las herramientas del VEC, con drvec_gui |

Comparten un módulo común, `atsw_mcp.proyecto`, con las herramientas del
proyecto (§2.2) y las de las ventanas (§7.4).

**Los nombres, sin chocar con la suite.** `atsw-mcp` ya existe: es el
registrador de la suite `atsw` de PyPI, y un estudiante puede tener los dos.
Por eso:

- el código vive en `mcp/atsw_mcp/` (decidido); el módulo Python `atsw_mcp`
  no choca con el paquete `atsw`;
- la distribución se llama **`atsw-gui-mcp`**;
- las órdenes son `atsw-us`, `atsw-mt` y `atsw-sis`, y **ninguna se llama
  `atsw-mcp`**;
- no hace falta registrador: la madre deja el proyecto preparado (§7.1).

Las instrucciones de cada servidor, que es donde vive el método (los
carriles, las preguntas obligatorias), se heredan de las de cada MCP y se
reescriben para el proyecto **y más cortas** (§3.3).

### 2.2 Qué se queda, qué sale y qué se añade

| | herramientas | por qué |
|---|---|---|
| **se quedan**, envueltas | US: identificación (`guided_identification`, `unit_root_analysis`, `seasonal_analysis`…), estimación y diagnosis (`confirm_and_estimate`, `estimate_and_diagnose`, `overparameterization_analysis`), contrastes formales y MEG (`formal_tests`, `meg_frequency`, `meg_reformulate`), intervención (`intervention_ladder`, `test_interventions`…), previsión. MT: `identify_link`, `refine_link`, `set_network`, `estimate`, `diagnose`, `overfit`, `forecast`. VARMA/VECM: las de sima | es el método; no se reescribe |
| **salen** | `load_data`, `create_inp`, `preview_data` (los datos entran por Datos de la madre: `m00`); los `guion_*` y `export_guion` (el `guion.json` es de la otra encarnación, P4); `load_example` de sima, salvo decisión; **`build_model` y `batch_build`** (decisión 6, abajo) | chocan con el proyecto o con la asignatura |
| **nuevas** | `proyecto_resumen` (series, modelos, casos, elegidos); `modelo(serie, id)` que da la ruta; `derivar` (nuevo `mNN` desde un `.pre`, con padre); `razon` y `elegir`; `caso_nuevo` (MT, con las entradas y su hash); `cuaderno_apunte` (texto al cuaderno); `ver` (abre una ventana de gráficos de la madre, §7.4); `comprobar` (qué está instalado, con versiones) | el idioma de la madre |

**El atajo auto-ARIMA no está en el MTM** (decisión 6). `build_model` y
`batch_build` ajustan un modelo de una llamada, sin sobreparametrizar, sin
mirar Semana Santa y sin contrastes formales; el propio art advierte que no
son el carril autónomo (BUG-0180). En el MTM **no existen**, en ningún
carril: no se prohíben con una instrucción, simplemente no están. En la P05
el `AGENTS.md` los prohibía por escrito, y eso depende de que el modelo lo
lea y lo obedezca (§3.4). Quien quiera el atajo tiene art-python, fuera de
la madre.

**La razón, gratis.** `DISENO-madre.md` §8 bis no pide razón en la GUI
porque un modal «¿por qué?» se contesta `asdf`. El asistente sí la da:
`guion_node` de art la rechaza vacía y 881 de 924 nodos la llevan. En el MTM,
la razón del nodo va **a la razón del modelo** en el manifiesto.

### 2.3 Los motores: análisis en Python, el óptimo con el motor de la madre

Dos caminos. Es la única pregunta de la primera versión que sigue abierta
(§11, P1):

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

**En el monorepo, `mcp/atsw_mcp/`** (decidido), con su `pyproject.toml`:

- el contrato (los nombres `mNN`, los casos, el manifiesto) es de la madre, y
  cambia con ella en el mismo PR;
- las pruebas que importan cruzan los dos lados: el MTM escribe y la madre lo
  ve (§8);
- art-python, drtran-python y sima-python siguen en sus repositorios y se
  usan como dependencias fijadas, con un `uv.lock` en el monorepo.

**Versión**: la de ATSW. `atsw-gui-mcp` 0.1.6 va con ATSW 0.1.6 y fija
versiones exactas de art-tseries, drtran, sima-tseries, fue, drvarma, drvec y
pyfug. Es lo que la P05 hizo a mano con `art-tseries==0.2.2` («no la
actualices aunque veas otra más nueva»), y funcionó. `comprobar` enseña las
versiones, y la comprobación de arranque de la madre (TODO) también.

**Lo que hay que resolver antes de fijar:**

- **drvec no está en PyPI**, y fue 0.1.18 tampoco: los dos esperan al
  release conjunto (memoria del proyecto). sima 0.2.0, con el VEC, tampoco
  está publicado. No bloquea la fase 1, que sólo necesita art y drtran.
- Los tres piden `mcp>=1.0,<2`; en PyPI ya hay `mcp` 2.3. Hay que fijar la 1.x
  o subir los tres a la 2.

---

## 3. Lo que enseñó la P05

La práctica 5 (13-oct-2026): cada grupo construyó con art, en su portátil,
dos modelos del IPC de varios países (D = 0 y D = 1) y comparó sus
previsiones fuera de muestra. Aquí sólo van **las lecciones técnicas**; los
datos de los estudiantes, sus nombres y sus notas no salen de la carpeta de
la asignatura.

### 3.1 La instalación

- **uv con versión fijada funcionó**: `uv tool install art-tseries==0.2.2` y
  `uv tool update-shell`. No hace falta instalar Python aparte.
- **El plan B fue pip** (Python de python.org con «Add python.exe to PATH»;
  `pipx` en macOS), para cuando uv falla.
- **Lo que falló en Windows fue PowerShell**, no uv: las políticas de
  ejecución bloquean el `irm … | iex` del instalador. Las salidas fueron
  `winget install astral-sh.uv` u otra terminal.
- **Los Mac con chip Apple necesitan macOS 12** o posterior: scipy no
  publica ruedas para macOS 11 en arm64. Los Intel funcionan desde macOS 11.
  (Nuestro `.dmg` de arm64 ya pide macOS 14, así que no estrecha nada.)
- **El aula de informática no deja instalar nada**: todo es en el portátil
  del estudiante, y antes de la clase.

**Consecuencia para el MTM:** la madre lleva **su propio `uv`** dentro del
paquete y lo llama directamente. No hay script de PowerShell que bloquear ni
PATH que tocar (§6.2).

### 3.2 Los clientes

Funcionaron cuatro, y el analista ha tenido éxito con varios modelos,
Claude incluido:

| cliente | cómo se conecta el MCP | instrucciones | nota |
|---|---|---|---|
| **CodeWhale + DeepSeek** | `~/.codewhale/mcp.json` (global) | lee `AGENTS.md` | **el camino probado**. Clave personal `sk-…` que envía el profesor y se cobra en su cuenta |
| **Claude Code** | `claude mcp add --scope user … --env …` | `CLAUDE.md`, que carga `@AGENTS.md` | |
| **Codex** | `~/.codex/config.toml` (global) | lee `AGENTS.md` | |
| **Gemini CLI** | `~/.gemini/settings.json`, con `timeout: 600000` | `.gemini/settings.json` del proyecto con `context.fileName: AGENTS.md` | el único sin confirmar en clase; los estudiantes tienen Gemini Pro |

**El sistema de la asignatura** (decisión 3, sin acuerdo con la
universidad): CodeWhale con DeepSeek y la clave del profesor por defecto, y
quien ya tenga Claude Code, Codex o Gemini, el suyo.

### 3.3 Las instrucciones, por cliente

- **El procedimiento de clase va en `AGENTS.md`**, y cada cliente llega a él
  por su camino: `CLAUDE.md` con `@AGENTS.md` para Claude Code,
  `.gemini/settings.json` para Gemini, y CodeWhale y Codex lo leen solos.
- **Hay un límite de ~50 KB**: CodeWhale descarta **entero** un fichero de
  instrucciones más grande. Las instrucciones del servidor de art son 51 KB.
- **Hay clientes que no pasan las instrucciones del servidor al modelo.**
  Por eso iba `instrucciones_art_servidor.md`, una copia íntegra de reserva.
- **Para comprobar que se ha cargado**: «cita el título del procedimiento de
  clase».

**Consecuencia para el MTM:** las instrucciones de cada servidor tienen que
caber holgadamente (objetivo: menos de 20 KB), y la madre genera todo este
andamiaje en cada proyecto (§7.1).

### 3.4 Los gráficos

- **DeepSeek no ve imágenes.** Los gráficos los ve sólo el estudiante, y el
  procedimiento obligaba al asistente a decirlo: «No puedo ver los gráficos;
  míralos tú antes de decidir.»
- **`ART_VIEWER=1` abre cada gráfico en una ventana** (en Windows, sin la
  variable, no se abre ninguna), y `ART_FIG_DIR` guarda los PNG en la carpeta
  de la práctica.
- WezTerm y kitty, con `imgcat`, eran opcionales.

**Consecuencia para el MTM:** con un modelo que no ve, **la ventana de
gráficos es la mitad del análisis**. Es la razón de que la decisión 10 tenga
una parte que no espera al final (§7.4).

### 3.5 El modo guiado no se cumplió solo

El enunciado pedía modo guiado, y el `AGENTS.md` lo imponía con todo
detalle: una iteración por turno, parar en «Tu decisión», «el decisor es el
estudiante». Aun así, **varios grupos delegaron las decisiones**: tras
declarar el dominio, un «hazlo tú todo», o nodos enteros anotados como
«IA autorizada» o «LLM». En algunos casos, en el país principal.

La lección: **una instrucción no impide nada si quien la incumple es el
propio estudiante**, que se la levanta al asistente con una frase. Lo que sí
funciona:

- **quitar del servidor lo que no se debe usar**, en vez de prohibirlo
  (decisión 6: `build_model` no está);
- **que quede a la vista quién decidió**, para que se pueda corregir: el
  `por: asistente` del linaje (decisión 8, §7.2);
- **que la decisión del estudiante sea un gesto suyo en la madre**: elegir
  el modelo y escribir la razón con «Elegir…» y «Razón…», que ya están en el
  menú US.

### 3.6 Las entregas

Se pedía el guion en HTML, los `.inp`, `.out` y `.pre` de los dos modelos y
los ficheros de la comparación. **Varios grupos no subieron los ficheros de
art**, y sin los `.pre` no se puede reproducir nada. En la madre la entrega
ya está hecha: **el proyecto entero** (manifiesto con el linaje, modelos,
`.out`) **y el cuaderno**. Falta sólo un gesto que lo empaquete
(«Preparar la entrega…», §7.6).

---

## 4. El cliente

| cliente | Windows | macOS | cuenta | sin red |
|---|---|---|---|---|
| **CodeWhale + DeepSeek** | sí (`winget`) | sí | clave de DeepSeek; en la asignatura, la personal que da el profesor | no |
| **Claude Code** | sí, nativo desde Windows 10 1809, sin administrador; Git for Windows opcional | sí, macOS 13+ | Pro, Max, Team, Enterprise o Console; **el plan gratuito no lo incluye** | no |
| **Codex** | sí | sí | ChatGPT | no |
| **Gemini CLI** | sí (Node.js 20+) | sí | cuenta de Google; los estudiantes tienen Gemini Pro | no |
| Claude Desktop | sí | sí | también el gratuito; los MCP locales en el gratuito, por comprobar | no |

**Optimizado para Claude, pero no atado a él** (decisión 2). En la práctica
eso quiere decir:

- **todo en texto**: cada herramienta devuelve los números que sostienen la
  decisión, y el gráfico va aparte, en su ventana (§7.4). Un modelo que no ve
  imágenes no pierde nada que no pueda pedir;
- **nada que sólo entienda un cliente**: ni sintaxis propia de Claude en las
  instrucciones, ni herramientas que dependan de leer imágenes;
- **tiempos de espera largos**: las estimaciones largas necesitan los 10
  minutos que la P05 puso en Gemini;
- **instrucciones cortas**, por el límite de 50 KB (§3.3);
- **la prueba de CI con un cliente que no sea Claude**, por lo menos el
  protocolo MCP a pelo (§8).

**Sin red no hay asistente**, con ningún cliente. La madre funciona igual sin
él, y así tiene que seguir: el asistente es un añadido, nunca un requisito.

---

## 5. La terminal

**Decidido: la del sistema** (decisión 1). La madre la abre en la carpeta del
proyecto:

| | cómo |
|---|---|
| Windows | Windows Terminal (`wt -d DIR`) si está, que en Windows 11 viene de serie; si no, `cmd /k` en DIR. **Nunca un script de PowerShell**, por las políticas (§3.1) |
| macOS | Terminal.app (`open -a Terminal DIR`) |
| Linux | `x-terminal-emulator`, o la que diga `$TERMINAL` |

No lanza el cliente: abre la terminal **con un aviso de una línea** sobre qué
escribir (`codewhale`, `claude`, `codex` o `gemini`, según el que se haya
configurado).

Por qué no empotrada, comprobado otra vez:

| opción | Windows | macOS | Linux | coste |
|---|---|---|---|---|
| **VTE** (`vte-2.91`) | **no** (depende de los pty de Unix) | no oficial (un port de MacPorts, 0.82, sin soporte del proyecto) | sí | bajo, sólo en Linux |
| **terminal propia** con `libvterm` | ConPTY (Windows 10 1809+) | `forkpty` | `forkpty` | **semanas**, y mantenerla: libvterm (MIT, C, lo usa Neovim) da el parser y la pantalla; el dibujo, el teclado, el ancho Unicode y el redimensionado son nuestros. Los clientes son TUI exigentes |
| **panel de conversación propio** | sí | sí | sí | medio-alto: es un cliente de LLM, con su autenticación y su bucle. `DISENO-madre.md` §10 ya lo descartaba |

La terminal empotrada queda, como mucho, para el final, y sólo si la externa
se queda corta.

---

## 6. El empaquetado

### 6.1 Lo que hay hoy

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

### 6.2 Python bajo demanda, con el uv de la madre

**Decidido: bajo demanda con uv** (decisión 4). Con lo aprendido en la P05:

- **El paquete lleva el binario de `uv`** (uno por plataforma, junto a los
  motores, en `programas.sh`). Es un ejecutable suelto; no instala nada al
  instalar ATSW.
- **Ayuda ▸ «Asistente» ▸ «Instalar el asistente…»** llama a ese `uv`
  **directamente**, sin PowerShell, sin `irm`, sin tocar el PATH. Instala un
  Python y `atsw-gui-mcp` con su `uv.lock` en
  `%LOCALAPPDATA%\ATSW\asistente` o
  `~/Library/Application Support/ATSW/asistente`.
- **El plan B, pip**, queda documentado en el manual para quien no pueda
  descargar (proxy, red del centro), como en la P05.
- Necesita red la primera vez, que el asistente necesita igual. Quien no lo
  use sigue con una madre de 22 MB.

**Lo que hay que comprobar en el ensayo (fase 0):**

- que hay **ruedas binarias** para Windows x64, macOS arm64 y macOS x86_64 de
  fue, drvarma, drvec y pyfug (drvec no está publicado: §2.5);
- que el C compilado de fue y la verosimilitud de drvarma **cargan** desde
  ese Python (ya pasó una vez que las ruedas instalaban sin poder cargar el
  motor: faltaba cffi);
- el tamaño real descargado, y el del `uv` que va en el paquete;
- macOS 11 en Intel, donde la P05 dice que funciona.

### 6.3 La comprobación de arranque

El TODO «Después del diseño de la madre» ya pide comprobar al arrancar que
todo está instalado. El asistente entra en esa lista como **opcional**:
«asistente: no instalado (Instalar…)», «asistente: `atsw-gui-mcp` 0.1.6;
clientes encontrados: codewhale, claude». Lo opcional no se pinta como
error.

---

## 7. El lado de la madre

### 7.1 El menú Asistente y el andamiaje del proyecto

**Menú propio** (decisión 7), después de las clases: `Proyecto · Datos · US
· MT · Asistente · Ayuda`, y con VARMA y VECM, `… · VECM · Asistente ·
Ayuda`. Se apaga entero, con su globo, si el asistente no está instalado.

«Preparar el proyecto para el asistente…» pregunta el cliente (o varios) y
escribe en la carpeta del proyecto:

| fichero | para quién | qué lleva |
|---|---|---|
| `AGENTS.md` | todos | el procedimiento: el modo (de clase o libre, §7.2), las reglas que no se rompen, la plantilla de cada iteración. **Menos de 50 KB, con margen** |
| `CLAUDE.md` | Claude Code | `@AGENTS.md` |
| `.gemini/settings.json` | Gemini CLI | `context.fileName: AGENTS.md`, y los servidores con `timeout: 600000` |
| `.mcp.json` | Claude Code | los servidores del proyecto (la primera vez Claude Code pide aprobarlos) |
| `instrucciones_servidor.md` | los clientes que no pasan las instrucciones del servidor | la copia íntegra, generada con `atsw-us --instrucciones` y las de los otros |

Para **CodeWhale y Codex**, cuya configuración es global (`~/.codewhale/
mcp.json`, `~/.codex/config.toml`), la madre **enseña el bloque** y se ofrece
a añadirlo, con confirmación; nunca reescribe el fichero entero. Como esa
configuración no sabe de proyectos, el servidor **busca el proyecto desde la
carpeta en la que arranca**, subiendo hasta encontrar `proyecto.yaml`. Por
eso la terminal se abre en la carpeta del proyecto. Que cada cliente arranque
los servidores en esa carpeta se comprueba en la fase 0.

La configuración lleva además `ATSW_GUI`, la ruta de la propia madre, para
que `ver` la encuentre sin buscarla (§7.4).

**Las claves, nunca en el proyecto.** La clave de DeepSeek se queda en
`~/.codewhale/config.toml`, donde la pone `codewhale auth set`. El proyecto se
entrega comprimido (§7.6): nada de lo que la madre escribe en él lleva una
clave, y «Preparar la entrega…» comprueba que no hay ninguna.

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

**Todo lo que escribe el MTM lleva `por: asistente`.** El servidor no puede
saber quién tecleó la orden, y no lo finge. Lo que es del estudiante es lo
que hace **en la madre**: elegir un modelo y escribir su razón con «Elegir…»
y «Razón…». La ventana del linaje enseña los dos, y un profesor ve a primera
vista una cadena decidida por el asistente sin razones del estudiante.

**El modo de clase.** El proyecto lleva, en sus propiedades, el modo del
asistente:

- **de clase** (por defecto): sólo el carril guiado; el `AGENTS.md` no
  ofrece el autónomo, y los servidores arrancan con `--guiado`, que quita el
  autónomo de sus instrucciones;
- **libre**: guiado y autónomo, para el investigador.

Es lo que la P05 enseñó (§3.5): no se puede impedir que un estudiante
delegue, pero sí que no tenga a mano lo que no debe usar y que la delegación
**se vea y se pueda corregir**.

### 7.3 Ver lo que hace el asistente, en vivo

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

### 7.4 El asistente abre las ventanas de gráficos

Decisión 10: el MCP de la madre va al final, **pero el asistente tiene que
poder abrir las ventanas de gráficos ya**. El camino mínimo:

**Tres opciones:**

| | qué es | a favor | en contra |
|---|---|---|---|
| (a) `ART_VIEWER`, como los MCP de Python | el MTM dibuja un PNG con matplotlib y lo abre con el visor del sistema | ya existe; no toca la madre | **es otro dibujo**: el analista compararía el gráfico del asistente con el de la madre creyendo que compara dos modelos. Es justo lo que `plothost.h` explica que la madre evita, enlazando el lienzo de fue en vez de redibujarlo |
| (b) `atsw_gui --ver`, una ventana suelta | el MTM lanza la madre con argumentos y ésta abre **sólo esa ventana**, en un proceso aparte | el mismo gráfico de la madre; el código ya está hecho para esto (abajo); no hace falta canal | un proceso por ventana; no sabe de la madre abierta |
| (c) el canal con la madre abierta | un socket local; la madre abierta abre la ventana | la ventana sale en la madre del analista | es el MCP de la madre entero, que va al final. `GtkApplication` único no sirve: en Windows y macOS no hay bus de sesión de D-Bus |

**Recomiendo (b).** Es pequeño porque las ventanas ya están desacopladas:
las de `lib/analisis` piden a quien las abre un `AnHost`, y todos sus
punteros a función pueden ser NULL («entonces ese gesto no se ofrece»).
fue_gui y la madre ya son dos anfitriones distintos. `--ver` es un tercero,
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
  deriva, no escribe el manifiesto, no lanza nada. Sí tiene «Al cuaderno»,
  que es del estudiante: lo que va al cuaderno lo decide él.
- **Un proceso aparte.** La madre ya es `G_APPLICATION_NON_UNIQUE`, así que
  una segunda copia no molesta a la abierta. Desde Python se lanza separado
  (en Windows con `DETACHED_PROCESS`), y la herramienta vuelve en seguida.
- **Lo que hay que mover:** el vistazo vive hoy en `gui/atsw/src/vistazo.c`
  y recibe la `Atsw` entera. Pasa a `lib/analisis` con un anfitrión como
  `AnHost`, igual que las demás ventanas. Es el único cambio de fondo.
- **La herramienta `ver(que, serie, modelo)`** del MTM lanza
  `$ATSW_GUI --ver …` y **devuelve en texto** los números que el gráfico
  enseña: las ACF y PACF con sus bandas, la Q, los anómalos con su fecha.
  Así un modelo que no ve imágenes tiene la evidencia, y el estudiante, la
  ventana.
- **MT**, de momento, abre drtran_gui con `--caso`, que ya existe. Sus
  gráficos sueltos (la CCF, la respuesta al impulso) van con el MCP de la
  madre.
- **Si no encuentra la madre** (alguien usa el MTM sin ATSW), cae en (a),
  avisando de que es otro dibujo.

**Es el embrión del MCP de la madre.** El vocabulario de `--ver` (qué
ventana, de qué serie y modelo) es el de las órdenes que el socket aceptará
después. Cuando llegue (c), cambia el transporte, no las órdenes.

### 7.5 La regla de seguridad: sólo dentro del proyecto

**El asistente sólo escribe dentro de la carpeta del proyecto.** Se hace
cumplir en el MTM, no en el cliente, porque los clientes son cuatro y no
todos tienen permisos (en Windows nativo, Claude Code no tiene sandbox):

- toda ruta se resuelve contra la raíz (`realpath`) y se rechaza si sale de
  ella, con enlaces simbólicos incluidos;
- `m00` y los datos son de sólo lectura (`DISENO-madre.md`: «nadie los
  edita»);
- el MTM no ofrece órdenes de sistema. La única orden que lanza es
  `atsw_gui --ver` (y los motores, si se decide B), con argumentos que él
  mismo construye;
- ninguna clave en el proyecto (§7.1).

Donde el cliente lo permite, una segunda capa: el `.claude/settings.json` que
la madre escribe con el `.mcp.json` permite las herramientas de los
servidores y niega las de escribir y ejecutar.

### 7.6 Preparar la entrega

Proyecto ▸ «Preparar la entrega…» comprime la carpeta del proyecto: el
manifiesto con el linaje, los `.inp`, `.out` y `.pre`, los casos y el
cuaderno con sus imágenes. Deja fuera la caché y lo que no es del proyecto,
y comprueba que no hay claves. Es lo que en la P05 faltó a varios grupos
(§3.6), y sirve igual sin asistente.

### 7.7 La maqueta

```
┌ ATSW — P06 ────────────────────────────────────────────────────────────┐
│ Proyecto  Datos  US  MT  Asistente  Ayuda                              │
│                          ┌──────────────────────────────────────────┐  │
│                          │ Abrir la terminal del proyecto      ⌃⇧A  │  │
│                          │ Preparar el proyecto para el asistente…  │  │
│                          │ ──────────────────────────────────────── │  │
│                          │ Qué ha hecho el asistente…               │  │
│                          │ Instalar el asistente…                   │  │
│                          └──────────────────────────────────────────┘  │
│ Series — US       │ IPC  muestra completa                              │
│  IPC        ●     │  m00  datos                                        │
│  WTI              │  m01  ARIMA(0,1,1)(0,1,1)12     σ 0,412            │
│                   │  m02  + AI 2020-03              σ 0,398   elegido  │
│ Casos — MT        │  m03  + MA(12) libre  ✦ asist.  σ 0,397            │
│  C1  IPC ← WTI    │  m04  − MA(12)        ✦ asist.  σ 0,398            │
├───────────────────┴────────────────────────────────────────────────────┤
│ m04 de IPC, del asistente: «el MA(12) sobra: t = 0,4»                  │
└────────────────────────────────────────────────────────────────────────┘

┌ Windows Terminal — C:\Users\…\ATSW\P06 ──┐  ┌ IPC/m04 · residuos ─────┐
│ > ¿el MA(12) de m03 se gana su sitio?    │  │ ▁▃▂▅▁▂▇▁▃▂▁▄▂▁▃          │
│ ● atsw-us · overparameterization IPC/m03 │  │ ACF ▌▖ ▗ ▖  ▖           │
│   θ12 = 0,031  t = 0,41 → no             │  │ PACF▌▗ ▖ ▗              │
│ ● atsw-us · derivar IPC m03 → m04        │  │ Q(24) = 19,3            │
│ ● atsw-us · estimar IPC/m04 (fue)        │  │          [Al cuaderno]  │
│ ● atsw-us · ver residuos IPC/m04         │  └─────────────────────────┘
│   Q(24) = 19,3 (p 0,63). No veo el       │
│   gráfico: míralo tú. ¿Eliges m04?       │
└──────────────────────────────────────────┘
```

La decisión de quedarse con m04 la toma el estudiante en la madre, con
«Elegir…» y su razón; entonces la fila dice «elegido» sin la marca del
asistente.

---

## 8. El plan, por fases

Cada fase en su PR, con CI en las tres plataformas.

**Fase 0: ensayo del empaquetado y de los clientes** (en paralelo, ya).
- Un job de CI en Windows y macOS que, con el binario de `uv`, instala un
  Python y art-tseries, drtran y sima-tseries con versiones fijadas, y corre
  una prueba de humo: cargar un `.pre`, estimar, sin red después de
  instalar.
- Mide el tamaño y comprueba las ruedas (§6.2).
- A mano, en un Windows y un Mac: que CodeWhale, Claude Code, Codex y Gemini
  CLI arrancan los servidores en la carpeta desde la que se lanzan (§7.1).
- No toca la madre.

**Fase 1: `atsw-proyecto`, `--ver` y el MTM de US y MT.**
- `atsw-proyecto`, compilado de `lib/proyecto`, con sus pruebas, en
  `programas.sh`.
- `atsw_gui --ver`, con el vistazo movido a `lib/analisis` (§7.4).
- `mcp/atsw_mcp/` con `atsw-us` y `atsw-mt`, las herramientas del proyecto y
  `ver` (§2.2), `--guiado` e `--instrucciones`, y la estimación con el
  binario de la madre si se decide B.
- **Pruebas** (pytest en los tres sistemas): sobre el proyecto de ejemplo
  IPC ← WTI, el MTM deriva, estima y abre un caso; `test_proyecto` y la madre
  leen el manifiesto; el `.pre` del asistente no se mueve en fue; rutas fuera
  de la raíz rechazadas; las instrucciones caben en 20 KB; una sesión MCP a
  pelo, sin cliente de LLM, recorre las herramientas. En `test_gui`, `--ver`
  abre cada ventana sin escribir nada en el proyecto.

**Fase 2: la madre y el asistente.**
- El menú Asistente; «Preparar el proyecto para el asistente…» con el
  andamiaje de §7.1; abrir la terminal de cada sistema (la prueba comprueba
  la orden construida, no abre ventanas en CI); «Instalar el asistente…» con
  el `uv` del paquete.
- `GFileMonitor`, el campo `por: asistente` y el modo de clase en las
  propiedades del proyecto.
- «Preparar la entrega…».
- **Pruebas**: en `test_gui`, una fase `asistente` que escribe un modelo con
  `atsw-proyecto` por fuera y comprueba que la rejilla lo enseña sin foco y
  con la marca; el andamiaje generado (los cinco ficheros, sin claves, el
  `AGENTS.md` por debajo de 50 KB).

**Hito drvarma_gui → `atsw-sis`, VARMA.** Las herramientas de sima para
VARMA, con los casos `motor: drvarma`, cuando la madre ya los enseña.

**Hito drvec_gui → las del VEC en `atsw-sis`.** Igual para el VECM.

**Al final:** el MCP de la madre, con el canal (§7.4 c) y el vocabulario de
`--ver` ampliado (abrir en fue_gui, marcar un caso, los gráficos de MT). La
terminal empotrada, sólo si la externa se queda corta.

---

## 9. Cuándo: antes o después de drvarma_gui y drvec_gui

**Decidido: el MCP crece con la escalera** (decisión 9). Las razones, que
siguen valiendo para cuando haya que repasarlo:

1. **El contrato de MT no depende de los hitos.** El caso MT está fijado
   desde `DISENO-casos.md` (`schema_version: 2`, `motor`, `entradas` con
   hash, `_casos/C1/work/`), y la «Clase» que añade el hito drvarma_gui se
   deduce de `motor`: la fase 2 de la madre ya tiene `atsw_clase_de()`. El de
   VARMA y VECM **sí** depende: drvarma_gui todavía no acepta `--proyecto` y
   `--caso`, y la disposición de sus corridas no está escrita. Escribir casos
   VARMA ahora sería cambiar el contrato dos veces.
2. **US y MT están estables** después de la fase 3, y son lo que el
   estudiante usa primero.
3. **El asistente no llega más lejos que la madre.** Sin el hito, sima
   produciría sistemas que la madre no sabe enseñar, ficheros sueltos fuera
   del manifiesto. Hasta entonces, quien quiera VARMA usa sima tal cual,
   fuera del proyecto, como en las prácticas.
4. **El empaquetado no depende de nada**, y es lo que más puede sorprender.

```
fase 3 de la madre ─→ fases 1 y 2 (US + MT) ─→ hito drvarma_gui + VARMA ─→ hito drvec_gui + VECM
fase 0 (ensayo de empaquetado y clientes), en paralelo desde ya ──┘
```

---

## 10. Riesgos

- **El coste de la clave de DeepSeek**, que paga el profesor. El MTM ayuda
  con instrucciones cortas y herramientas que devuelven lo justo, que es
  también lo que piden los límites de los clientes.
- **Los clientes cambian cada semana**, y son cuatro. La integración se
  limita a lo estable: la configuración MCP de cada uno, el fichero de
  instrucciones, lanzar la terminal. Nada de depender de detalles de su
  interfaz. La fase 0 se repite antes de cada curso.
- **El método en manos del asistente.** El MTM hereda las instrucciones de
  los MCP, quita lo que no se debe usar y marca lo que hace (§7.2). No puede
  impedir que un estudiante delegue; sí que pase desapercibido.
- **Dos encarnaciones.** El MTM respeta P4: escribe el proyecto de la madre
  con la herramienta de la madre y deja el `guion.json` a ATSW Python.
- **Dos órdenes parecidas.** `atsw-mcp` (de la suite) y `atsw-us`/`atsw-mt`
  (del MTM) pueden convivir en el mismo ordenador. El manual dice cuál es
  cuál, y la comprobación de arranque enseña las dos si las encuentra.

---

## 11. Decisiones (19-oct-2026)

1. **La terminal, externa**: la del sistema, abierta por la madre en la
   carpeta del proyecto (§5). La empotrada, sólo al final y si hace falta.
2. **El cliente: Claude Code o los alternativos.** El MTM se optimiza para
   Claude pero tiene que seguir funcionando con otros clientes (§4).
3. **Sin acuerdo con la universidad: el sistema de la P05.** CodeWhale con
   DeepSeek y la clave personal del profesor por defecto; Claude Code, Codex
   o Gemini CLI para quien ya los tenga (§3.2).
4. **Python bajo demanda con uv**, con el `uv` dentro del paquete (§6.2).
5. **En el monorepo, `mcp/atsw_mcp/`.** La distribución, `atsw-gui-mcp`, y
   las órdenes, `atsw-us`, `atsw-mt` y `atsw-sis`, para no chocar con el
   `atsw-mcp` de la suite (§2.1).
6. **El atajo auto-ARIMA no está en el MTM**: `build_model` y `batch_build`
   no existen en ningún carril. No se prohíben: no están (§2.2).
7. **Menú «Asistente» propio**, después de las clases (§7.1).
8. **`por: asistente` en el linaje**, sólo cuando no es lo normal. Todo lo
   que escribe el MTM lo lleva; lo del estudiante es lo que hace en la madre
   (§7.2).
9. **El MCP crece con la escalera**: US y MT después de la fase 3; VARMA con
   drvarma_gui; VECM con drvec_gui; el ensayo de empaquetado, ya (§9).
10. **El MCP de la madre, al final; las ventanas de gráficos, ya**, con
    `atsw_gui --ver` (§7.4), que es su embrión.

### Preguntas que quedan

- **P1. Los motores del MTM** (§2.3): análisis en Python y estimación con los
  binarios de la madre (B, recomendado), o todo en Python (A). Era la 3 de la
  primera versión y no se contestó.
- **P2. El modo de clase por defecto** (§7.2): sólo guiado en los proyectos
  nuevos, y el libre a mano desde las propiedades. ¿De acuerdo?
- **P3. «Preparar la entrega…»** (§7.6), en el menú Proyecto, en la fase 2.
  ¿De acuerdo, o va aparte?

---

**Estado: decidido salvo P1-P3.**

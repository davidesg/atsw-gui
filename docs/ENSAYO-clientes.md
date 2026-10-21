# Ensayo de los clientes (fase 0 del asistente)

Lista de comprobación **a mano**, para un Windows y un Mac, de la fase 0 de
`docs/ESTUDIO-terminal-mcp.md` (§8). Lo que hay que averiguar es una sola
cosa: **que cada cliente arranca los servidores de `atsw-gui-mcp` y que estos
encuentran el proyecto desde la carpeta en la que se abrió el cliente.**

La instalación y la sesión MCP sin cliente ya las prueba la CI
(`.github/workflows/asistente.yml`). Aquí falta lo que la CI no puede hacer:
un cliente de verdad, con su cuenta.

Hay cuatro clientes. Basta con los que tenga quien hace la prueba, pero se
anota cuáles se probaron y cuáles no.

## 0. Preparar

**uv.** En Windows: `winget install astral-sh.uv`. En macOS:
`curl -LsSf https://astral.sh/uv/install.sh | sh`. Después hay que cerrar y
abrir la terminal.

**atsw-gui-mcp**, desde el repositorio. Todavía no está en PyPI.

```sh
uv tool install --python 3.12 "atsw-gui-mcp @ git+https://github.com/davidesg/atsw-gui@main#subdirectory=mcp/atsw_mcp"
uv tool update-shell
```

Mientras la fase 0 no esté fusionada, cambia `@main` por `@asistente-fase0`.

Cierra y abre la terminal, y comprueba:

```sh
atsw-us --version        # 0.1.5
atsw-us --instrucciones  # una línea: «Servidor atsw-us de ATSW, clase US…»
```

`uv tool install` no lee el `uv.lock`; para este ensayo da igual, porque lo
que se prueba es el cliente, no las versiones.

**ATSW instalado**, con su paquete de la v0.1.4 o posterior. La carpeta de
los motores, que va en `ATSW_BIN`, es:

| sistema | `ATSW_BIN` |
|---|---|
| Windows, instalación para el usuario | `C:\Users\<usuario>\AppData\Local\Programs\ATSW\bin` |
| Windows, para todos los usuarios | `C:\Program Files\ATSW\bin` |
| macOS | `/Applications/ATSW.app/Contents/Resources/bin` |

**Un proyecto de prueba**: una carpeta con un `proyecto.yaml` (vale el de un
proyecto de la madre, o uno que diga sólo `formato: 1`) y dentro una
subcarpeta, por ejemplo `prueba/series/ipc`.

## 1. Qué es un éxito

En cada cliente, abierto **en la subcarpeta** `prueba/series/ipc`, se pide:

> Llama a la herramienta `estado` de atsw-us y enséñame su salida tal cual.

Es un éxito si la salida dice:

```
atsw-us 0.1.5
proyecto: <ruta>/prueba (subiendo desde <ruta>/prueba/series/ipc)
motores:
  fue: <ATSW_BIN>/fue (ATSW_BIN)
  ...
```

Las tres cosas que hay que mirar:

1. **El servidor arranca** y el cliente ve sus dos herramientas, `version` y
   `estado`.
2. **`proyecto:` es la carpeta de prueba**, y «subiendo desde» es la carpeta
   en la que se abrió el cliente. Si dice otra carpeta, como el directorio
   personal o la del programa, **el cliente no lanza el servidor en su
   carpeta de trabajo**: se anota, porque cambia el diseño de §7.1.
3. **`fue:` sale de `ATSW_BIN`** y no de «PATH (no es el del paquete)».

Repite lo mismo con `atsw-mt` (su `estado` lista `drtran` y `fue`). `atsw-sis`
basta con que arranque: drvarma y drvec no van todavía en el paquete.

## 2. Los clientes

En los ejemplos, `RUTA_BIN` es la carpeta de `ATSW_BIN` del cuadro de arriba.
En los JSON de Windows las barras van dobles (`C:\\Users\\…`).

### Claude Code

Por proyecto: un `.mcp.json` en la carpeta de prueba.

```json
{
  "mcpServers": {
    "atsw-us": { "command": "atsw-us", "args": [], "env": { "ATSW_BIN": "RUTA_BIN" } },
    "atsw-mt": { "command": "atsw-mt", "args": [], "env": { "ATSW_BIN": "RUTA_BIN" } }
  }
}
```

Abre `claude` en `prueba/series/ipc`. La primera vez pide aprobar los
servidores del proyecto. `/mcp` dentro de la sesión, o `claude mcp list`
fuera, debe enseñarlos conectados.

Prueba también la configuración global, que es la que usó la P05:

```sh
claude mcp add --scope user atsw-us --env ATSW_BIN=RUTA_BIN -- atsw-us
```

### CodeWhale

Global, en `~/.codewhale/mcp.json` (en Windows,
`C:\Users\<usuario>\.codewhale\mcp.json`):

```json
{
  "servers": {
    "atsw-us": { "command": "atsw-us", "args": [], "env": { "ATSW_BIN": "RUTA_BIN" }, "disabled": false },
    "atsw-mt": { "command": "atsw-mt", "args": [], "env": { "ATSW_BIN": "RUTA_BIN" }, "disabled": false }
  }
}
```

```sh
codewhale mcp connect
codewhale mcp tools atsw-us   # version y estado
```

Después abre `codewhale` en `prueba/series/ipc` y haz la pregunta del §1.
**Es el caso que más interesa**: la configuración es global, así que el
punto 2 del §1 dice si CodeWhale lanza el servidor en su carpeta de trabajo.

### Codex

Global, al final de `~/.codex/config.toml` (en Windows,
`C:\Users\<usuario>\.codex\config.toml`):

```toml
[mcp_servers.atsw-us]
command = "atsw-us"
env = { ATSW_BIN = 'RUTA_BIN' }

[mcp_servers.atsw-mt]
command = "atsw-mt"
env = { ATSW_BIN = 'RUTA_BIN' }
```

Las comillas simples dejan escribir la ruta de Windows con una sola barra.
`codex mcp list` debe listarlos. Abre `codex` en `prueba/series/ipc`. La
configuración también es global, así que el punto 2 importa igual que con
CodeWhale.

### Gemini CLI

Global, en `~/.gemini/settings.json` (en Windows,
`C:\Users\<usuario>\.gemini\settings.json`), o por proyecto, en
`prueba/.gemini/settings.json`:

```json
{
  "mcpServers": {
    "atsw-us": { "command": "atsw-us", "env": { "ATSW_BIN": "RUTA_BIN" }, "timeout": 600000 },
    "atsw-mt": { "command": "atsw-mt", "env": { "ATSW_BIN": "RUTA_BIN" }, "timeout": 600000 }
  }
}
```

`gemini mcp list` debe enseñarlos conectados. Abre `gemini` en
`prueba/series/ipc`. Fue el único cliente sin confirmar en la P05: si la
cuenta de estudiante no entra, vale una cuenta personal de Google.

## 3. Lo que se anota

Una fila por cliente y sistema. Va en el PR de la fase 1b, o en un comentario
del de la fase 0:

| cliente | sistema | versión | arranca | `proyecto:` correcto | `fue` de `ATSW_BIN` | notas |
|---|---|---|---|---|---|---|
| Claude Code (proyecto) | Windows 11 | | | | | |
| Claude Code (global) | Windows 11 | | | | | |
| CodeWhale | Windows 11 | | | | | |
| Codex | Windows 11 | | | | | |
| Gemini CLI | Windows 11 | | | | | |
| (lo mismo) | macOS | | | | | |

En «notas» va todo lo raro: mensajes de error completos, si hubo que
reiniciar el cliente, si el cliente no pasa las instrucciones del servidor
(pregúntale «¿qué instrucciones te ha dado el servidor atsw-us?»), o si
recortó la salida de `estado`.

# atsw-gui-mcp

Los MCP a medida de ATSW: el asistente sobre los motores en C de la madre.
El diseño está en `docs/ESTUDIO-terminal-mcp.md`.

**Fase 0.** Tres servidores, `atsw-us`, `atsw-mt` y `atsw-sis`, que arrancan
y dicen su versión (`version`) y dónde han encontrado el proyecto y los
motores (`estado`). Todavía no analizan ni estiman.

- `ATSW_PROYECTO`: el proyecto. Si no está, se busca `proyecto.yaml` desde la
  carpeta de arranque hacia arriba.
- `ATSW_BIN`: la carpeta de los motores del paquete. Si no está, la de
  `ATSW_GUI`; y sólo al final, el PATH.

Instalar, con las versiones de `uv.lock`:

    uv sync --frozen --no-dev --no-editable                 # el núcleo
    uv sync --frozen --no-dev --no-editable --extra python  # con lo que queda en Python

Prueba de humo, sin cliente de LLM: `python tests/humo.py .venv/bin`.
La prueba a mano con los clientes, en `docs/ENSAYO-clientes.md`.

## Para la fase 1b: los procesos, con la entrada cerrada

La CI de la fase 0 lo ha medido. **En Windows, un proceso lanzado desde
dentro del servidor que hereda la entrada estándar se cuelga**: la entrada es
la tubería del cliente MCP. Con `stdin=subprocess.DEVNULL` no pasa. En Linux
y macOS no pasa de ninguna de las dos maneras.

Importar `art` dentro del servidor se cuelga igual en Windows, aunque fuera
del servidor tarda entre 1 y 19 s, también en un hilo y con una tubería
abierta. Todo apunta a que ese import lanza un proceso.

Así que en el MTM:

- toda llamada a un motor en C va con `stdin=subprocess.DEVNULL`;
- antes de importar el extra dentro del servidor hay que averiguar qué
  proceso lanza `art` al importarse, o importarlo en un proceso aparte.

Los pasos «diagnóstico» de `.github/workflows/asistente.yml`
(`ATSW_MCP_DIAG=import|sub|sub-devnull`) lo repiten en cada vuelta.

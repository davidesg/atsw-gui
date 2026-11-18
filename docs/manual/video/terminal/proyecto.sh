#!/bin/bash
# EL PROYECTO DE PARTIDA DE LA PARTE DE LA TERMINAL del video del asistente
# (LEEME.md): el mismo proyecto en que acaba la parte de la madre
# (escenas-asistente/): subyacente 2002-2019, m01 y m02 del asistente, m02
# elegido, y el proyecto preparado para Claude Code.
#
#   bash docs/manual/video/terminal/proyecto.sh      (necesita la red: instala el asistente)
#
# Corre las escenas de la madre con capturas.sh, sin grabar, y CONSERVA lo
# que hicieron en $CURSO (por defecto /tmp/curso-terminal): el proyecto en
# $CURSO/asistente, ATSW en $CURSO/ATSW y el asistente instalado en
# $CURSO/data/ATSW/asistente. El .mcp.json del proyecto apunta ahi. La ruta es
# neutra a proposito: la terminal la ensena y no debe salir la carpeta del
# usuario.
set -euo pipefail
R=$(cd "$(dirname "$0")/../../../.." && pwd)
export CURSO=${CURSO:-/tmp/curso-terminal} PROYECTO_NOMBRE=asistente
P=$CURSO/asistente
F=$(mktemp -d "${TMPDIR:-/tmp}/atsw-terminal.XXXXXX")
trap 'rm -rf "$F"' EXIT

ESCENAS=$R/docs/manual/video/escenas-asistente CONSERVA=1 FOTOS=$F \
    PANTALLA=${PANTALLA:-40} PASO_MS=${PASO_MS:-300} LIMITE=${LIMITE:-900} \
    bash "$R/docs/manual/capturas/capturas.sh" > "$F/capturas.log" 2>&1 ||
    { tail -20 "$F/capturas.log" >&2; exit 1; }

# EL EXTRA PYTHON (art-python), que «Instalar el asistente…» no pone: sin
# el, boxcox_analysis, intervention_ladder y suggest_intervention_form lo
# dicen y no hacen nada. La misma orden que la madre, con --extra python.
A=$CURSO/data/ATSW/asistente
UV_PROJECT_ENVIRONMENT=$A/.venv UV_PYTHON_INSTALL_DIR=$A/python UV_CACHE_DIR=$A/cache \
    "$CURSO/ATSW/bin/uv" sync -q --frozen --no-dev --no-editable --managed-python \
    --python 3.12 --extra python --project "$A/fuente"

# La nota del guion (asistente.py) no es del proyecto: el LLM no debe verla.
rm -f "$P/.asistente-video"

cat <<FIN
hecho: $P

La madre, con el asistente que acaba de instalarse:

  XDG_DATA_HOME=$CURSO/data $CURSO/ATSW/bin/atsw_gui --proyecto $P/proyecto.yaml

y en la terminal:

  cd $P && claude

Las preguntas: docs/manual/video/terminal/LEEME.md; la grabacion: graba.sh
FIN

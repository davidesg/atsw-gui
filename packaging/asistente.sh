# EL ASISTENTE EN EL PAQUETE (docs/ESTUDIO-terminal-mcp.md §6.2). Lo cargan
# los guiones de cada plataforma con «. packaging/asistente.sh»:
#
#   asistente_uv TRIPLE BIN       el binario de uv de esa plataforma, en BIN
#   asistente_fuente RAIZ DESTINO la fuente de atsw-gui-mcp (de la raiz del
#                                 arbol), con su uv.lock
#
# El paquete NO INSTALA EL ASISTENTE: lleva lo necesario para que la madre lo
# instale cuando se le pida (Asistente ▸ Instalar el asistente…), con este uv
# y sin PowerShell ni el PATH. Lo que se descarga entonces --un Python y las
# dependencias del uv.lock-- va a la carpeta del usuario, no aqui.
#
# DONDE VA, LO MISMO EN LOS TRES: uv al lado de los demas programas (bin/) y
# la fuente en bin/../asistente/atsw_mcp, que es donde la madre la busca
# (gui/atsw/src/asistente.c, fuente_mcp):
#
#   Linux    /usr/lib/atsw/bin/uv          /usr/lib/atsw/asistente/atsw_mcp
#   Windows  ATSW\bin\uv.exe               ATSW\asistente\atsw_mcp
#   macOS    Contents/Resources/bin/uv     Contents/Resources/asistente/atsw_mcp
#
# LA VERSION DE uv ES LA DEL ENSAYO (.github/workflows/asistente.yml): se
# suben juntas. El archivo se comprueba con su sha256 antes de usarlo.

ASISTENTE_UV_VERSION=0.12.24

asistente_uv() {
    local triple=$1 bin=$2 f base d sum visto uv
    case "$triple" in
        *windows*) f=uv-$triple.zip ;;
        *)         f=uv-$triple.tar.gz ;;
    esac
    base=https://github.com/astral-sh/uv/releases/download/$ASISTENTE_UV_VERSION
    d=$(mktemp -d)
    curl -sSLf -o "$d/$f" "$base/$f"
    curl -sSLf -o "$d/sum" "$base/$f.sha256"
    sum=$(cut -d' ' -f1 < "$d/sum")
    visto=$( (sha256sum "$d/$f" 2>/dev/null || shasum -a 256 "$d/$f") | cut -d' ' -f1)
    if [ "$sum" != "$visto" ]; then
        echo "uv $ASISTENTE_UV_VERSION ($f): el sha256 no coincide" >&2
        rm -rf "$d"; return 1
    fi
    case "$f" in
        *.zip) ( cd "$d" && unzip -q "$f" ) ;;
        *)     ( cd "$d" && tar xzf "$f" ) ;;
    esac
    uv=$(find "$d" -type f \( -name uv -o -name uv.exe \) | head -1)
    [ -n "$uv" ] || { echo "no encuentro uv en $f" >&2; rm -rf "$d"; return 1; }
    mkdir -p "$bin"
    cp "$uv" "$bin/"
    chmod 755 "$bin/$(basename "$uv")"
    rm -rf "$d"
    echo "uv $ASISTENTE_UV_VERSION -> $bin/$(basename "$uv")"
}

# Solo lo que hace falta para construir e instalar: sin pruebas, sin entornos
# ni caches de quien empaqueta.
asistente_fuente() {
    local R=$1 dest=$2
    rm -rf "$dest"
    mkdir -p "$dest"
    cp "$R/mcp/atsw_mcp/pyproject.toml" "$R/mcp/atsw_mcp/uv.lock" "$R/mcp/atsw_mcp/README.md" "$dest/"
    cp -R "$R/mcp/atsw_mcp/src" "$dest/src"
    find "$dest" \( -name __pycache__ -o -name '*.egg-info' \) -prune -exec rm -rf {} +
    echo "atsw-gui-mcp $(sed -n 's/^version = "\(.*\)"/\1/p' "$dest/pyproject.toml") -> $dest"
}

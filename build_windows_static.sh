#!/bin/bash
# build_windows_static.sh - Compila drvarma y drvarma_gui estáticamente para Windows usando MXE
# Uso: ./build_windows_static.sh [target]
#   target: i686-w64-mingw32.static (32 bits) o x86_64-w64-mingw32.static (64 bits, por defecto)

set -e

MXE_PATH="${MXE_PATH:-$HOME/mxe}"
TARGET="${1:-x86_64-w64-mingw32.static}"
CROSS_PREFIX="$MXE_PATH/usr/bin/$TARGET-"

if [ ! -f "${CROSS_PREFIX}gcc" ]; then
    echo "❌ Error: No se encuentra el compilador cruzado ${CROSS_PREFIX}gcc"
    echo "   Asegúrate de que MXE está instalado en $MXE_PATH"
    exit 1
fi

# Añadir directorios al PATH y configurar pkg-config
export PATH="$MXE_PATH/usr/bin:$PATH"
export PKG_CONFIG_PATH="$MXE_PATH/usr/$TARGET/lib/pkgconfig"

echo "🔧 Compilando para target: $TARGET"
make distclean >/dev/null 2>&1 || true
make CROSS="$CROSS_PREFIX" all

if command -v "${CROSS_PREFIX}strip" >/dev/null 2>&1; then
    echo "🔪 Stripping ejecutables..."
    "${CROSS_PREFIX}strip" bin/drvarma.exe bin/drvarma_gui.exe
fi

# ---------- Copiar recursos necesarios ----------
echo "📦 Copiando recursos GTK..."

# Crear estructura de directorios
mkdir -p bin/lib/gdk-pixbuf-2.0
mkdir -p bin/share/glib-2.0/schemas
mkdir -p bin/share/icons
mkdir -p bin/share/themes

# Obtener versión de gdk-pixbuf usando pkg-config del target
GDK_PIXBUF_VERSION=$(pkg-config --modversion gdk-pixbuf-2.0 2>/dev/null | cut -d. -f1,2)
if [ -z "$GDK_PIXBUF_VERSION" ]; then
    # Fallback: buscar directorios
    PIXBUF_DIR="$MXE_PATH/usr/$TARGET/lib/gdk-pixbuf-2.0"
    if [ -d "$PIXBUF_DIR" ]; then
        GDK_PIXBUF_VERSION=$(basename $(ls -d "$PIXBUF_DIR"/*/ 2>/dev/null | head -n1) | cut -d. -f1,2)
    fi
fi
if [ -z "$GDK_PIXBUF_VERSION" ]; then
    GDK_PIXBUF_VERSION="2.10"
    echo "⚠️  No se pudo detectar la versión de gdk-pixbuf, se usará 2.10"
fi
VERSION_DIR="$GDK_PIXBUF_VERSION.0"  # asumimos que es X.Y.0
mkdir -p "bin/lib/gdk-pixbuf-2.0/$VERSION_DIR/loaders"

# Buscar el ejecutable gdk-pixbuf-query-loaders (está en el directorio bin del target)
QUERY_LOADERS_EXE="$MXE_PATH/usr/$TARGET/bin/gdk-pixbuf-query-loaders.exe"
if [ ! -f "$QUERY_LOADERS_EXE" ]; then
    # Intentar sin .exe (podría ser un script)
    QUERY_LOADERS_EXE="$MXE_PATH/usr/$TARGET/bin/gdk-pixbuf-query-loaders"
fi

if [ -f "$QUERY_LOADERS_EXE" ]; then
    echo "   Encontrado: $QUERY_LOADERS_EXE"
    # Intentar generar loaders.cache con Wine
    if command -v wine >/dev/null 2>&1; then
        echo "   Generando loaders.cache con Wine..."
        wine "$QUERY_LOADERS_EXE" > "bin/lib/gdk-pixbuf-2.0/$VERSION_DIR/loaders.cache" 2>/dev/null
        if [ $? -ne 0 ] || [ ! -s "bin/lib/gdk-pixbuf-2.0/$VERSION_DIR/loaders.cache" ]; then
            echo "   ⚠️  Falló la generación con Wine. Copiando ejecutable para generación manual."
            cp "$QUERY_LOADERS_EXE" bin/
            echo "   En Windows, ejecute desde la carpeta bin:"
            echo "     gdk-pixbuf-query-loaders.exe > lib/gdk-pixbuf-2.0/$VERSION_DIR/loaders.cache"
        else
            echo "   loaders.cache generado correctamente."
        fi
    else
        echo "   ⚠️  Wine no instalado. Copiando ejecutable para generación manual en Windows."
        cp "$QUERY_LOADERS_EXE" bin/
        echo "   En Windows, ejecute desde la carpeta bin:"
        echo "     gdk-pixbuf-query-loaders.exe > lib/gdk-pixbuf-2.0/$VERSION_DIR/loaders.cache"
    fi
else
    echo "⚠️  gdk-pixbuf-query-loaders no encontrado en $MXE_PATH/usr/$TARGET/bin/"
    echo "   El cache deberá generarse manualmente con una instalación de GTK en Windows."
fi

# 2. Esquemas GSettings
if [ -d "$MXE_PATH/usr/$TARGET/share/glib-2.0/schemas" ]; then
    echo "   Copiando esquemas GSettings..."
    cp "$MXE_PATH/usr/$TARGET/share/glib-2.0/schemas/"*.xml bin/share/glib-2.0/schemas/ 2>/dev/null || true
    # Compilar esquemas (requiere glib-compile-schemas en el host)
    if command -v glib-compile-schemas >/dev/null 2>&1; then
        echo "   Compilando esquemas..."
        glib-compile-schemas bin/share/glib-2.0/schemas/
    else
        echo "⚠️  glib-compile-schemas no encontrado. Los esquemas pueden no funcionar."
    fi
fi

# 3. Tema de iconos Adwaita
if [ -d "$MXE_PATH/usr/$TARGET/share/icons/Adwaita" ]; then
    echo "   Copiando tema de iconos Adwaita..."
    cp -r "$MXE_PATH/usr/$TARGET/share/icons/Adwaita" bin/share/icons/ 2>/dev/null || true
fi

# 4. Tema de iconos hicolor (base)
if [ -d "$MXE_PATH/usr/$TARGET/share/icons/hicolor" ]; then
    echo "   Copiando tema de iconos hicolor..."
    cp -r "$MXE_PATH/usr/$TARGET/share/icons/hicolor" bin/share/icons/ 2>/dev/null || true
fi

# 5. Tema GTK Adwaita
if [ -d "$MXE_PATH/usr/$TARGET/share/themes/Adwaita" ]; then
    echo "   Copiando tema GTK Adwaita..."
    cp -r "$MXE_PATH/usr/$TARGET/share/themes/Adwaita" bin/share/themes/ 2>/dev/null || true
fi

echo "✅ Copia de recursos completada."
ls -lh bin/

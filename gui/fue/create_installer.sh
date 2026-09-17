#!/bin/bash
# create_installer.sh – Genera un instalador NSIS para Windows
# Uso: ./create_installer.sh [versión]

set -e

# Configuración
NAME="FUE_GUI"
EXE_NAME="fue_gui.exe"
PUBLISHER="FUE Project"
VERSION="${1:-1.0.0}"
OUTPUT_INSTALLER="FUE_GUI_Setup_${VERSION}.exe"
NSIS_SCRIPT="fue_gui.nsi"

# Verificar que la carpeta bin/ existe
if [ ! -d "bin" ]; then
    echo "❌ Error: No se encuentra la carpeta bin/. Ejecute primero ./build_windows_static_fue.sh"
    exit 1
fi

# Verificar que makensis está instalado
if ! command -v makensis >/dev/null 2>&1; then
    echo "❌ Error: NSIS (makensis) no está instalado."
    echo "   En Ubuntu/Debian: sudo apt install nsis"
    echo "   En otros sistemas: https://nsis.sourceforge.io/Download"
    exit 1
fi

# Crear el script NSIS temporal
cat > "$NSIS_SCRIPT" <<EOF
;--------------------------------
; Configuración general
!define VERSION "${VERSION}"
!define NAME "${NAME}"
!define PUBLISHER "${PUBLISHER}"
!define EXE_NAME "${EXE_NAME}"

Name "\${NAME}"
OutFile "${OUTPUT_INSTALLER}"
InstallDir "\$PROGRAMFILES64\${NAME}"
InstallDirRegKey HKLM "Software\${NAME}" "Install_Dir"
RequestExecutionLevel admin

;--------------------------------
; Páginas del instalador
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
; Sección principal (obligatoria)
Section "Aplicación principal" SecApp
    SectionIn RO
    SetOutPath "\$INSTDIR"

    # Copiar todo el contenido de bin/ (incluye subdirectorios)
    File /r "bin\*.*"

    # Crear desinstalador
    WriteUninstaller "\$INSTDIR\uninstall.exe"

    # Entradas de registro
    WriteRegStr HKLM "Software\${NAME}" "Install_Dir" "\$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" \
                     "DisplayName" "\${NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" \
                     "UninstallString" '"\$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" \
                     "DisplayVersion" "\${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" \
                     "Publisher" "\${PUBLISHER}"

    # Asociación opcional con archivos .inp
    WriteRegStr HKCR ".inp" "" "FUE.Input"
    WriteRegStr HKCR "FUE.Input\DefaultIcon" "" "\$INSTDIR\${EXE_NAME},0"
    WriteRegStr HKCR "FUE.Input\shell\open\command" "" '"\$INSTDIR\${EXE_NAME}" "%1"'
SectionEnd

;--------------------------------
; Accesos directos
Section "Accesos directos" SecShortcuts
    SetOutPath "\$INSTDIR"
    CreateDirectory "\$SMPROGRAMS\${NAME}"
    CreateShortcut "\$SMPROGRAMS\${NAME}\${NAME}.lnk" "\$INSTDIR\${EXE_NAME}" \
                   "" "\$INSTDIR\${EXE_NAME}" 0
    CreateShortcut "\$SMPROGRAMS\${NAME}\Uninstall.lnk" "\$INSTDIR\uninstall.exe" \
                   "" "\$INSTDIR\uninstall.exe" 0
SectionEnd

;--------------------------------
; Desinstalación
Section "Uninstall"
    Delete "\$INSTDIR\*.*"
    RMDir /r "\$INSTDIR\lib"
    RMDir /r "\$INSTDIR\share"
    Delete "\$INSTDIR\uninstall.exe"
    RMDir "\$INSTDIR"

    RMDir /r "\$SMPROGRAMS\${NAME}"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"
    DeleteRegKey HKLM "Software\${NAME}"
    DeleteRegKey HKCR ".inp"
    DeleteRegKey HKCR "FUE.Input"
SectionEnd
EOF

# Ejecutar NSIS
echo "🔧 Generando instalador NSIS..."
makensis "$NSIS_SCRIPT"

# Limpiar script temporal (opcional)
rm -f "$NSIS_SCRIPT"

# Verificar resultado
if [ -f "$OUTPUT_INSTALLER" ]; then
    echo "✅ Instalador creado: $OUTPUT_INSTALLER"
    ls -lh "$OUTPUT_INSTALLER"
else
    echo "❌ Falló la generación del instalador."
    exit 1
fi

; EL INSTALADOR DE WINDOWS (Inno Setup 6). Empaqueta lo que deja bundle.sh:
; (Va en UTF-8 CON BOM: sin el, Inno Setup lo lee como ANSI y rompe los acentos.)
;
;   iscc /DVersion=0.1 /DFuente=..\..\dist\ATSW packaging\windows\atsw.iss
;
; Se instala POR USUARIO, en %LOCALAPPDATA%\Programs\ATSW, y sin pedir
; permisos de administrador: para probarlo en una maquina cualquiera basta
; con el usuario de siempre. No esta firmado, asi que SmartScreen avisa la
; primera vez («Mas informacion» -> «Ejecutar de todas formas»).

#ifndef Version
  #define Version "0.0"
#endif
#ifndef Fuente
  #define Fuente "..\..\dist\ATSW"
#endif

[Setup]
AppId={{6B0E9B3C-6C0F-4F2B-9E55-0A75C3A1D7E1}
AppName=ATSW
AppVersion={#Version}
AppPublisher=ATSW
DefaultDirName={autopf}\ATSW
DefaultGroupName=ATSW
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=ATSW-{#Version}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\atsw_gui.exe

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "escritorio"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Fuente}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ATSW"; Filename: "{app}\bin\atsw_gui.exe"; WorkingDir: "{userdocs}"
Name: "{group}\Editor de modelos (fue)"; Filename: "{app}\bin\fue_gui.exe"; WorkingDir: "{userdocs}"
Name: "{group}\Redes de transferencia (drtran)"; Filename: "{app}\bin\drtran_gui.exe"; WorkingDir: "{userdocs}"
Name: "{group}\VARMA (drvarma)"; Filename: "{app}\bin\drvarma_gui.exe"; WorkingDir: "{userdocs}"
Name: "{group}\Gráficos (fug)"; Filename: "{app}\bin\gtk_fmg.exe"; WorkingDir: "{userdocs}"
Name: "{group}\Léeme"; Filename: "{app}\LEEME.txt"
Name: "{group}\{cm:UninstallProgram,ATSW}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\ATSW"; Filename: "{app}\bin\atsw_gui.exe"; WorkingDir: "{userdocs}"; Tasks: escritorio

[Run]
Filename: "{app}\bin\atsw_gui.exe"; Description: "{cm:LaunchProgram,ATSW}"; WorkingDir: "{userdocs}"; Flags: nowait postinstall skipifsilent

; SPDX-License-Identifier: AGPL-3.0-or-later
; Inno Setup script. Makefile passes /DMyAppName= /DMyAppVersion= /DMyStage=

#ifndef MyAppName
  #define MyAppName "pidload"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef MyStage
  #define MyStage "stage"
#endif

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Lenik
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=out
OutputBaseFilename={#MyAppName}-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\bin\{#MyAppName}.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MyStage}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppName}.exe"; Flags: createonlyiffileexists
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppName}.exe"; Tasks: desktopicon; Flags: createonlyiffileexists

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\bin\{#MyAppName}.exe"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent skipifdoesntexist

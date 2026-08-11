#ifndef MyAppVersion
  #define MyAppVersion "2.0.0"
#endif
[Setup]
AppId={{FDB03BE8-664E-4505-8DE2-05CBE80FBC1F}
AppName=Reactor Drone
AppVersion={#MyAppVersion}
AppPublisher=Conrad Miszczak
DefaultDirName={autopf}\Reactor Drone
DefaultGroupName=Reactor Drone
OutputBaseFilename=ReactorDrone-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\ReactorDrone.exe

[Files]
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\Reactor Drone"; Filename: "{app}\ReactorDrone.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall Reactor Drone"; Filename: "{uninstallexe}"

[Run]
; Interactive install: offer launch checkbox.
Filename: "{app}\ReactorDrone.exe"; Description: "Launch Reactor Drone"; \
  WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
; Silent update (Task 10): relaunch the game automatically.
Filename: "{app}\ReactorDrone.exe"; WorkingDir: "{app}"; Flags: nowait; Check: WizardSilent

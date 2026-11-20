; VICE SYNTH X - Windows x64 installer (Inno Setup 6)
; Build:  ISCC.exe /DAppVersion=1.0.0 /DStageDir=<folder with "VICE SYNTH X.vst3" and "VICE SYNTH X.exe"> ViceSynthX.iss
; Installs the VST3 bundle into the system VST3 folder (Common Files\VST3) and the standalone
; application into Program Files. The uninstaller removes both. User presets, AI presets and
; settings in Documents / AppData are intentionally left in place.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
AppId={{6B1E5C2A-4C5E-4F7B-9E38-0F3D2B7A9C11}
AppName=VICE SYNTH X
AppVersion={#AppVersion}
AppVerName=VICE SYNTH X {#AppVersion}
AppPublisher=Vice Synth Labs
DefaultDirName={autopf}\VICE SYNTH X
DefaultGroupName=VICE SYNTH X
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=VICE-SYNTH-X-Setup-{#AppVersion}-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=VICE SYNTH X {#AppVersion}
UninstallDisplayIcon={app}\VICE SYNTH X.exe
CloseApplications=yes

[Types]
Name: "full"; Description: "VST3 plug-in and standalone application"
Name: "vst3"; Description: "VST3 plug-in only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#StageDir}\VICE SYNTH X.vst3\*"; DestDir: "{commoncf64}\VST3\VICE SYNTH X.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\VICE SYNTH X.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#StageDir}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\VICE SYNTH X"; Filename: "{app}\VICE SYNTH X.exe"; Components: standalone
Name: "{group}\Uninstall VICE SYNTH X"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\VICE SYNTH X.exe"; Description: "Launch VICE SYNTH X"; Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\VICE SYNTH X.vst3"
Type: dirifempty; Name: "{app}"

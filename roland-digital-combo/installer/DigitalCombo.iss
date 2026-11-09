; Inno Setup script for the Digital Combo VST3 (Windows 10/11 x64).
; Built and tested on CI: silent install -> files and SHA256 checked -> Steinberg validator on the
; installed copy -> silent uninstall -> removal checked. Inno Setup itself is free software used
; only as a build tool; nothing from it is linked into the plug-in.
;
; Usage: ISCC.exe /DBundleDir="<path to Digital Combo.vst3>" /DAppVersion=0.1.0 DigitalCombo.iss

#ifndef BundleDir
  #error BundleDir must be defined (/DBundleDir=...)
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6F1D2B7C-3E54-4C1A-9B0E-5D2C8A7F4E31}
AppName=Digital Combo VST3
AppVersion={#AppVersion}
AppPublisher=Digital Combo (independent project, not affiliated with Roland)
DefaultDirName={commonpf64}\Digital Combo
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0
OutputBaseFilename=DigitalCombo-Setup-Windows-x64
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Digital Combo VST3 {#AppVersion}
WizardStyle=modern

[Files]
; The plug-in bundle goes to the standard VST3 folder that every host scans.
Source: "{#BundleDir}\*"; DestDir: "{commoncf64}\VST3\Digital Combo.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
; Notes stay next to the uninstaller, outside the VST3 folder.
Source: "..\INSTALL-WINDOWS.txt"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Digital Combo.vst3"

[Messages]
FinishedLabel=Digital Combo was installed to the VST3 folder:%n%nC:\Program Files\Common Files\VST3\Digital Combo.vst3%n%nOpen your music program and let it scan for new plug-ins.

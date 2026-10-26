; Inno Setup script for the 8cee Golden Era Bundle (built by GitHub Actions)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6E2B8C0A-90B0-4B7A-9E5A-8CEE19900001}
AppName=8cee Golden Era Bundle
AppVersion={#AppVersion}
AppPublisher=8cee
DefaultDirName={commoncf64}\VST3
DirExistsWarning=no
DisableDirPage=no
DisableProgramGroupPage=yes
OutputBaseFilename=8cee-Golden-Era-Bundle-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName=8cee Golden Era Bundle
WizardStyle=modern

[Components]
Name: "eraq"; Description: "Era-Q  (Equalizer)"; Types: full custom
Name: "erac"; Description: "Era-C  (Compressor)"; Types: full custom
Name: "eras"; Description: "Era-S  (Saturation / Sampler Grit)"; Types: full custom
Name: "eral"; Description: "Era-L  (Limiter)"; Types: full custom

[Files]
Source: "..\dist\VST3\8cee Era-Q.vst3\*"; DestDir: "{app}\8cee Era-Q.vst3"; Components: eraq; Flags: recursesubdirs ignoreversion
Source: "..\dist\VST3\8cee Era-C.vst3\*"; DestDir: "{app}\8cee Era-C.vst3"; Components: erac; Flags: recursesubdirs ignoreversion
Source: "..\dist\VST3\8cee Era-S.vst3\*"; DestDir: "{app}\8cee Era-S.vst3"; Components: eras; Flags: recursesubdirs ignoreversion
Source: "..\dist\VST3\8cee Era-L.vst3\*"; DestDir: "{app}\8cee Era-L.vst3"; Components: eral; Flags: recursesubdirs ignoreversion

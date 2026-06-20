; Inno Setup script — M3K Normalizator (VST3 + Standalone)
; Build:  ISCC.exe M3KNormalizator.iss   (from this folder)
; Paths are relative to this .iss file (installer/ -> ../build/...).

#define AppName    "M3K Normalizator"
#define AppVersion "0.10.15"
#define Publisher  "Milan Knor"
#define RepoUrl    "https://github.com/milanknor/m3k-normalizator"
#define BuildDir   "..\build\M3KNormalizator_artefacts\Release"
#define StandaloneExe BuildDir + "\Standalone\M3K Normalizator.exe"
#define Vst3Bundle    BuildDir + "\VST3\M3K Normalizator.vst3"

[Setup]
AppId={{8F3C2A10-9B4D-4E21-A3F7-1B2C3D4E5F60}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=.
OutputBaseFilename=M3K_Normalizator_Setup_v{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName} {#AppVersion}

[Languages]
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Standalone application
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion
; VST3 plug-in (folder bundle) into the shared system VST3 folder
Source: "{#Vst3Bundle}\*"; DestDir: "{commoncf}\VST3\{#AppName}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs uninsremovereadonly
; Docs
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE";      DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\README.md";    DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";            Filename: "{app}\M3K Normalizator.exe"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";      Filename: "{app}\M3K Normalizator.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\M3K Normalizator.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; \
    Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\{#AppName}.vst3"

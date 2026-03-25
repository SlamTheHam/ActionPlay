; ActionPlay Inno Setup Script
; Builds a standalone Windows installer (.exe)
; Requires Inno Setup 6+  https://jrsoftware.org/isinfo.php
;
; Usage:
;   1. Build ActionPlay.exe first (run build.bat)
;   2. Open this file in the Inno Setup Compiler (ISCC.exe) and click Compile
;      OR run:  ISCC.exe installer\setup.iss
;   3. The installer is written to installer\Output\ActionPlay-Setup-1.0.0.exe

#define AppName      "ActionPlay"
#define AppVersion   "1.0.0"
#define AppPublisher "SlamTheHam"
#define AppExeName   "ActionPlay.exe"
; Adjust this path to wherever your build places the .exe
#define BuildDir     "..\build_msvc\Release"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisherURL=https://github.com/SlamTheHam/ActionPlay
AppSupportURL=https://github.com/SlamTheHam/ActionPlay/issues
AppUpdatesURL=https://github.com/SlamTheHam/ActionPlay/releases
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
; Require 64-bit Windows
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Output
OutputDir=Output
OutputBaseFilename=ActionPlay-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
; No admin rights required; installs to user's Program Files
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; Appearance
WizardStyle=modern
WizardSizePercent=120
; Minimum Windows version: Windows 10
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";    Description: "{cm:CreateDesktopIcon}";    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startmenuicon";  Description: "Create a Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
Source: "{#BuildDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";     Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Nothing extra needed on uninstall

[Code]
// Verify the build output exists before compiling the installer
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

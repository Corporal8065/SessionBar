; SessionBar installer — Inno Setup 6 script (free: https://jrsoftware.org/isdl.php)
; Build via build-release.bat, or manually: ISCC SessionBar.iss
; Produces dist\SessionBar-Setup.exe — the single file to send a friend.
; The app is built with a STATIC MSVC runtime (/MT), so normally the exe
; needs nothing else. If dist\vc_redist.x64.exe is present at compile time
; (dynamic-link fallback), it is bundled and installed silently instead.

#define MyAppName "SessionBar"
; Version is SINGLE-SOURCED from the root CMakeLists.txt project(VERSION);
; build-release.bat and CI pass ISCC /DMyAppVersion=<version>. The literal
; below is only the fallback for a bare manual ISCC run.
#ifndef MyAppVersion
#define MyAppVersion "0.2.0"
#endif
#define MyAppExeName "SessionBar.exe"

[Setup]
AppId={{7D2B41F0-6C8A-4E53-B19D-3A5E8C2F47A6}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=SessionBar-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
; NOTE: the exe is unsigned, so Windows SmartScreen will warn recipients
; on first run ("More info" -> "Run anyway"). That is expected for free
; unsigned software and is explained in README_TEST.md.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
#if FileExists("dist\vc_redist.x64.exe")
Source: "dist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#endif

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
#if FileExists("dist\vc_redist.x64.exe")
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Visual C++ runtime..."; Flags: waituntilterminated
#endif
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

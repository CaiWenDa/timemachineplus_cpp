; installer.iss - Time Machine+ QML 版安装包脚本（Inno Setup 6）
; 由 packaging\package.ps1 调用：iscc /DAppVersion=1.0.0 installer.iss
; 也可以直接双击/命令行编译：iscc installer.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{8C4F1E2A-TIMEMACHINEPLUS-QML}}
AppName=Time Machine+
AppVersion={#AppVersion}
AppPublisher=Time Machine+
DefaultDirName={autopf}\TimeMachinePlus
DefaultGroupName=Time Machine+
OutputDir=..\dist-installer
OutputBaseFilename=TimeMachinePlus-{#AppVersion}-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; current-user 安装（装到 %LOCALAPPDATA%\Programs），规避 UAC 与 Program Files 写权限问题
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\timemachineplus_qml.exe
DisableProgramGroupPage=yes
; 目标机器若无 MSVC 运行时需人工安装 VC redist；windeployqt --compiler-runtime 已随包带 CRT

[Languages]
; 简体中文语言文件不在 Inno Setup 默认安装内，已随本仓库放在同目录（ChineseSimplified.isl）
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\Time Machine+"; Filename: "{app}\timemachineplus_qml.exe"
Name: "{autodesktop}\Time Machine+"; Filename: "{app}\timemachineplus_qml.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\timemachineplus_qml.exe"; Description: "启动 Time Machine+"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 数据目录独立于安装目录，卸载时不删（用户备份历史保留在 %LOCALAPPDATA%\TimeMachinePlus）

# package.ps1 - Time Machine+ QML 版一键打包脚本
# 用法：powershell -ExecutionPolicy Bypass -File packaging\package.ps1
#       powershell -ExecutionPolicy Bypass -File packaging\package.ps1 -Version 1.0.1
#       powershell -ExecutionPolicy Bypass -File packaging\package.ps1 -SkipInstaller   # 只产出 dist\ 绿色目录
#
# 产物：
#   dist\                    可直接双击运行的绿色目录（exe + Qt DLL + QML 模块 + OpenSSL + schema）
#   dist-installer\          安装包 setup.exe（需本机安装 Inno Setup 6）
param(
    [string]$Version = "1.0.0",
    [string]$QtDir = "C:\Qt\5.15.2\msvc2019_64",
    [string]$BuildDir = "build-release",
    [string]$DistDir = "dist",
    [string]$OpenSslDll = "",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
Write-Host "== Time Machine+ QML 打包 ==  版本 $Version" -ForegroundColor Cyan

function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

# ---------- 1. 定位工具链 ----------
# CMake：优先用 VS 自带版本（Qt 自带的 3.23 不认识 VS 2022+/2026 的生成器）
$cmake = $null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -property installationPath
    if ($vsPath) {
        $candidate = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $candidate) { $cmake = $candidate }
    }
}
if (-not $cmake) {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { $cmake = $cmd.Source }
}
if (-not $cmake) { Fail "未找到 cmake，请安装 Visual Studio 或 CMake 并加入 PATH" }

if (-not (Test-Path $QtDir)) { Fail "Qt 目录不存在：$QtDir（用 -QtDir 指定）" }
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) { Fail "未找到 windeployqt：$windeployqt" }

Write-Host "CMake      : $cmake"
Write-Host "Qt         : $QtDir"

# ---------- 2. Release 构建 ----------
Write-Host "`n[1/5] 配置 Release..." -ForegroundColor Yellow
# 强制锁定 Qt 来源：环境变量 CMAKE_PREFIX_PATH 的优先级高于 -DCMAKE_PREFIX_PATH，
# 若机器上装有 Anaconda（自带 Qt5），会抢先命中 conda 的 Qt（产物依赖 *_conda.dll）。
# 因此显式指定 Qt5_DIR 并移除该环境变量。
$qtCmakeDir = Join-Path $QtDir "lib\cmake\Qt5"
if (Test-Path $qtCmakeDir) {
    if ($env:CMAKE_PREFIX_PATH) {
        Write-Host "  忽略环境变量 CMAKE_PREFIX_PATH=$env:CMAKE_PREFIX_PATH" -ForegroundColor DarkYellow
        Remove-Item Env:CMAKE_PREFIX_PATH -ErrorAction SilentlyContinue
    }
} else {
    Fail "未找到 Qt5 的 CMake 配置：$qtCmakeDir"
}

# 旧缓存若指向别的 Qt（如 conda），必须清理后重配，否则缓存里的 Qt5_DIR 会继续生效
$cache = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cache) {
    $m = Select-String -Path $cache -Pattern "^Qt5_DIR:PATH=(.+)$" | Select-Object -First 1
    if ($m) {
        $cachedQt = $m.Matches[0].Groups[1].Value.Replace('/', '\')
        if ($cachedQt -ne $qtCmakeDir) {
            Write-Host "  旧缓存指向 $cachedQt，清理后重新配置" -ForegroundColor DarkYellow
            Remove-Item $BuildDir -Recurse -Force
        }
    }
}

& $cmake -S . -B $BuildDir -DCMAKE_PREFIX_PATH=$QtDir "-DQt5_DIR=$qtCmakeDir" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { Fail "CMake 配置失败" }

# 配置后校验：确认找到的 Qt 就是指定的 Qt
$m = Select-String -Path $cache -Pattern "^Qt5_DIR:PATH=(.+)$" | Select-Object -First 1
$qtPrefix = ($QtDir -replace '\\', '/')
if ($m -and ($m.Matches[0].Groups[1].Value -notlike "*$qtPrefix*")) {
    Fail "Qt5 来源不正确：$($m.Matches[0].Groups[1].Value)（应位于 $QtDir）"
}

Write-Host "[2/5] 编译..." -ForegroundColor Yellow
& $cmake --build $BuildDir --config Release --target timemachineplus_qml
if ($LASTEXITCODE -ne 0) { Fail "编译失败" }

# ---------- 3. 安装到 dist ----------
Write-Host "[3/5] 收集程序与 schema..." -ForegroundColor Yellow
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
& $cmake --install $BuildDir --prefix $DistDir --config Release --component qmlui
if ($LASTEXITCODE -ne 0) { Fail "安装到 $DistDir 失败" }

# ---------- 4. 部署运行依赖 ----------
Write-Host "[4/5] windeployqt 部署 Qt 依赖..." -ForegroundColor Yellow
# 关键：把目标 Qt 的 bin 置于 PATH 最前，避免 PATH 中其他 Qt（如 Anaconda 的
# Qt5Core_conda.dll）干扰 windeployqt 对"所需 DLL"的判定，否则会部署整套错误依赖
$qtBin = Join-Path $QtDir "bin"
$env:PATH = "$qtBin;$env:PATH"
& $windeployqt --release --compiler-runtime --qmldir (Join-Path $root "qml-ui\qml") (Join-Path $DistDir "timemachineplus_qml.exe")
if ($LASTEXITCODE -ne 0) { Fail "windeployqt 失败" }

# OpenSSL：项目以动态方式链接 libcrypto（exe 导入表含 libcrypto-3-x64.dll）
$dllName = "libcrypto-3-x64.dll"
if (-not $OpenSslDll) { $OpenSslDll = Join-Path $qtBin $dllName }
if (Test-Path $OpenSslDll) {
    Copy-Item $OpenSslDll (Join-Path $DistDir $dllName) -Force
    Write-Host "  + $dllName"
} else {
    $found = Get-ChildItem -Path $env:PATH.Split(';') -Filter $dllName -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        Copy-Item $found.FullName (Join-Path $DistDir $dllName) -Force
        Write-Host "  + $dllName (来自 $($found.Directory))" -ForegroundColor DarkYellow
        Write-Host "    提示：建议用与 thirdparty\OpenSSL\lib\libcrypto.lib 同版本的 DLL，用 -OpenSslDll 指定" -ForegroundColor DarkYellow
    } else {
        Fail "找不到 $dllName，请用 -OpenSslDll 指定路径（exe 运行必需）"
    }
}

# qt.conf：把 Qt 的插件/翻译/QML 模块路径锚定到 exe 同目录。
# 缺少它时，官方安装版 Qt5Core.dll 内嵌的 qt_prfxpath（C:\Qt\...）会被用于查找
# QML 模块，目标机器未装 Qt 时会报 "module QtQuick.Controls is not installed"。
$qtConf = @"
[Paths]
Prefix = .
Plugins = .
Imports = .
Translations = translations
"@
Set-Content -Path (Join-Path $DistDir "qt.conf") -Value $qtConf -Encoding ASCII

# ---------- 5. 完整性自检 ----------
Write-Host "[5/5] 产物自检..." -ForegroundColor Yellow
$required = @(
    "timemachineplus_qml.exe",
    "timemachine.sql",
    "Qt5Core.dll",
    "Qt5Quick.dll",
    "Qt5QuickControls2.dll",
    "platforms\qwindows.dll",
    "libcrypto-3-x64.dll",
    "qt.conf"
)
$missing = @()
foreach ($f in $required) {
    if (-not (Test-Path (Join-Path $DistDir $f))) { $missing += $f }
}
# windeployqt 把 QML 模块平铺到 dist 根（QtQuick\Controls.2 而非 qml\QtQuick\Controls.2）
$qmlControls = Join-Path $DistDir "QtQuick\Controls.2"
if (-not (Test-Path $qmlControls)) { $missing += "QtQuick\Controls.2" }
if ($missing.Count -gt 0) { Fail "缺少必要文件：$($missing -join ', ')" }

$sizeMB = [math]::Round(((Get-ChildItem $DistDir -Recurse -File | Measure-Object Length -Sum).Sum / 1MB), 1)
Write-Host "  dist 体积：$sizeMB MB" -ForegroundColor Green

# ---------- 6. 安装包 ----------
if (-not $SkipInstaller) {
    $iscc = $null
    $isccCandidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe")   # 仅当前用户安装
    )
    foreach ($p in $isccCandidates) {
        if (Test-Path $p) { $iscc = $p; break }
    }
    if (-not $iscc) {
        Write-Host "`n未检测到 Inno Setup 6，跳过安装包。dist\ 可直接压缩分发。" -ForegroundColor Yellow
        Write-Host "下载：https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
    } else {
        Write-Host "`n[+] 生成安装包..." -ForegroundColor Yellow
        & $iscc "/DAppVersion=$Version" (Join-Path $PSScriptRoot "installer.iss")
        if ($LASTEXITCODE -ne 0) { Fail "Inno Setup 编译失败" }
    }
}

Write-Host "`n完成。" -ForegroundColor Green
Write-Host "  绿色目录：$root\$DistDir"
if (-not $SkipInstaller) { Write-Host "  安装包  ：$root\dist-installer" }

# BiliHistory Windows build script (PyInstaller + NSIS installer)
# Usage (run from project root):
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
# Output:
#   dist\BiliHistory.exe
#   dist\BiliHistory-1.0.0-setup.exe (if NSIS is installed)

$ErrorActionPreference = "Stop"

# Locate project root (parent of the script directory)
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Choose Python interpreter: prefer py -3.12, then venv, then system python
$py = $null
$candidates = @("py -3.12")
$venvPy = Join-Path $root ".venv\Scripts\python.exe"
if (Test-Path $venvPy) { $candidates = @($venvPy) + $candidates }
$candidates += "python"

foreach ($c in $candidates) {
    try {
        $parts = $c -split ' ', 2
        & $parts[0] $parts[1] --version | Out-Null
        $py = $c
        break
    } catch {}
}

if (-not $py) {
    Write-Error "No usable Python interpreter found"
    exit 1
}

Write-Host "==> Using interpreter: $py"

# 从 Python 读取统一版本号
$version = & $py -c "import sys; sys.path.insert(0, 'src'); from version import APP_VERSION; print(APP_VERSION)" 2>$null
if (-not $version) { $version = "1.0.0" }
Write-Host "==> Building version: $version"

Write-Host "==> Installing build dependencies..."
& $py -m pip install pyinstaller PyQt6 cryptography requests Pillow -i https://pypi.tuna.tsinghua.edu.cn/simple

Write-Host "==> Building portable executable with PyInstaller..."
& $py -m PyInstaller packaging\bilihistory.spec --noconfirm --clean

$artifact = Join-Path $root 'dist' 'BiliHistory.exe'
Write-Host "==> Portable executable: $artifact"

# Build NSIS installer if available
$nsis = Get-ChildItem -Path "${env:ProgramFiles(x86)}\NSIS" -Filter makensis.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $nsis) { $nsis = Get-Command makensis -ErrorAction SilentlyContinue }
if ($nsis) {
    Write-Host "==> Building installer with NSIS..."
    Get-ChildItem -Path (Join-Path $root 'dist') -Recurse -ErrorAction SilentlyContinue
    $ico = Resolve-Path (Join-Path $root 'favicon.ico') | Select-Object -ExpandProperty Path
    $exe = Resolve-Path (Join-Path $root 'dist' 'BiliHistory.exe') | Select-Object -ExpandProperty Path
    if (-not (Test-Path $exe)) { throw "dist\BiliHistory.exe not found at $exe" }
    $out = Join-Path $root 'dist' "BiliHistory-${version}-setup.exe"
    Write-Host "Using icon: $ico"
    Write-Host "Using source exe: $exe"
    Write-Host "Using output file: $out"
    & $nsis.FullName "/DICON_PATH=$ico" "/DSOURCE_EXE=$exe" "/DOUT_FILE=$out" "/DAPP_VERSION=$version" packaging\installer.nsi
    $setup = Join-Path $root 'dist' "BiliHistory-${version}-setup.exe"
    if (Test-Path $setup) {
        Write-Host "==> Installer: $setup"
    } else {
        Write-Warning "Installer was not generated, check NSIS output above"
    }
} else {
    Write-Host "!! NSIS not found, skipping installer. Install NSIS from https://nsis.sourceforge.io"
}

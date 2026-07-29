# BiliHistory Windows build script (PyInstaller + NSIS installer)
# Usage (run from project root):
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 -Arch x64
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 -Arch x86
# Output:
#   dist\BiliHistory-x64.exe / dist\BiliHistory-x86.exe
#   dist\BiliHistory-1.0.0-x64-setup.exe / dist\BiliHistory-1.0.0-x86-setup.exe (if NSIS is installed)

param(
    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

# Locate project root (parent of the script directory)
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Choose Python interpreter: prefer venv, then py -3.12, then system python
$pyCmd = $null
$candidates = @()
$venvPy = Join-Path $root ".venv\Scripts\python.exe"
if (Test-Path $venvPy) { $candidates += $venvPy }
$candidates += "py -3.12"
$candidates += "python"

foreach ($c in $candidates) {
    try {
        $parts = $c -split ' ', 2
        & $parts[0] $parts[1] --version | Out-Null
        $pyCmd = $c
        break
    } catch {}
}

if (-not $pyCmd) {
    Write-Error "No usable Python interpreter found"
    exit 1
}

$pyParts = $pyCmd -split ' ', 2
function Invoke-Py {
    param([Parameter(ValueFromRemainingArguments=$true)]$ArgsList)
    & $pyParts[0] $pyParts[1] @ArgsList
}

Write-Host "==> Using interpreter: $pyCmd"

# 从 Python 读取统一版本号
$version = Invoke-Py -c "import sys; sys.path.insert(0, 'src'); from version import APP_VERSION; print(APP_VERSION)" 2>$null
if (-not $version) { $version = "1.0.0" }
Write-Host "==> Building version: $version"

Write-Host "==> Installing build dependencies..."
Invoke-Py -m pip install pyinstaller PyQt6==6.7.1 cryptography requests Pillow

Write-Host "==> Building portable executable with PyInstaller..."
Invoke-Py -m PyInstaller packaging\bilihistory.spec --noconfirm --clean

function Join-Paths([string]$base, [string]$child, [string]$leaf) {
    return Join-Path (Join-Path $base $child) $leaf
}

$srcExe = Join-Paths $root 'dist' 'BiliHistory.exe'
$artifact = Join-Paths $root 'dist' "BiliHistory-${Arch}.exe"
Move-Item -Path $srcExe -Destination $artifact -Force
Write-Host "==> Portable executable: $artifact"

# Build NSIS installer if available
$nsis = Get-ChildItem -Path "${env:ProgramFiles(x86)}\NSIS" -Filter makensis.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $nsis) { $nsis = Get-Command makensis -ErrorAction SilentlyContinue }
if ($nsis) {
    Write-Host "==> Building installer with NSIS..."
    Get-ChildItem -Path (Join-Path $root 'dist') -Recurse -ErrorAction SilentlyContinue
    $ico = Resolve-Path (Join-Path $root 'favicon.ico') | Select-Object -ExpandProperty Path
    $exe = Resolve-Path $artifact | Select-Object -ExpandProperty Path
    if (-not (Test-Path $exe)) { throw "dist\BiliHistory-${Arch}.exe not found at $exe" }
    $out = Join-Paths $root 'dist' "BiliHistory-${version}-${Arch}-setup.exe"
    Write-Host "Using icon: $ico"
    Write-Host "Using source exe: $exe"
    Write-Host "Using output file: $out"
    & $nsis.FullName "/DICON_PATH=$ico" "/DSOURCE_EXE=$exe" "/DOUT_FILE=$out" "/DAPP_VERSION=$version" packaging\installer.nsi
    $setup = Join-Paths $root 'dist' "BiliHistory-${version}-${Arch}-setup.exe"
    if (Test-Path $setup) {
        Write-Host "==> Installer: $setup"
    } else {
        Write-Warning "Installer was not generated, check NSIS output above"
    }
} else {
    Write-Host "!! NSIS not found, skipping installer. Install NSIS from https://nsis.sourceforge.io"
}

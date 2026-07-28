# B站历史记录管理工具 - Windows Nuitka 打包脚本
# 用法（在项目根目录）：
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
# 产物：dist\BiliHistory.exe

$ErrorActionPreference = "Stop"

# 定位项目根目录（脚本上一级）
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 选择 Python 解释器：优先 py -3.12，其次带 nuitka 的 .venv，最后系统 python
$py = $null
$pyCandidates = @("py -3.12")
$venvPy = Join-Path $root ".venv\Scripts\python.exe"
if ((Test-Path $venvPy) -and (& $venvPy -m nuitka --version 2>$null)) {
    $pyCandidates = @($venvPy) + $pyCandidates
}
$pyCandidates += "python"

foreach ($c in $pyCandidates) {
    try {
        $parts = $c -split ' ', 2
        & $parts[0] ($parts[1]) --version | Out-Null
        $py = $c
        break
    } catch {}
}

if (-not $py) {
    Write-Error "找不到可用的 Python 解释器"
    exit 1
}

Write-Host "==> 使用解释器: $py"
Write-Host "==> 安装/确认构建依赖..."
& $py -m pip install nuitka PyQt6 cryptography requests -i https://pypi.tuna.tsinghua.edu.cn/simple

Write-Host "==> 开始 Nuitka 单文件打包..."
& $py -m nuitka --standalone --onefile --windows-console-mode=disable `
  --enable-plugin=pyqt6 `
  --windows-icon-from-ico=favicon.ico `
  --include-data-files=favicon.ico=favicon.ico `
  --include-data-files=src/config.json=src/config.json `
  --output-dir=dist --output-filename=BiliHistory --jobs=4 gui_main.py

$artifact = Join-Path $root 'dist' 'BiliHistory.exe'
Write-Host "==> 完成。产物: $artifact"

# 若系统已安装 NSIS，则额外构建安装包
$makensis = Get-Command makensis -ErrorAction SilentlyContinue
if ($makensis) {
    Write-Host "==> 检测到 NSIS，构建安装程序..."
    & $makensis.Source packaging\installer.nsi
    $setup = Join-Path $root 'dist' 'BiliHistory-1.0.0-setup.exe'
    if (Test-Path $setup) {
        Write-Host "==> 安装包: $setup"
    } else {
        Write-Warning "安装包未生成，请检查 NSIS 输出"
    }
} else {
    Write-Host "!! 未检测到 NSIS，跳过安装包构建。如需安装包请先安装 NSIS: https://nsis.sourceforge.io"
}

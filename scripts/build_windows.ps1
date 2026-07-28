# B站历史记录管理工具 - Windows 打包脚本
# 用法（在项目根目录）：  powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
# 产物：dist\BiliHistory.exe

$ErrorActionPreference = "Stop"

# 定位项目根目录（脚本上一级）
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 选择 venv 内的 python（存在则用），否则用系统 python
$py = Join-Path $root ".venv\Scripts\python.exe"
if (-not (Test-Path $py)) { $py = "python" }

Write-Host "==> 使用解释器: $py"
Write-Host "==> 安装/确认构建依赖..."
& $py -m pip install pyinstaller cryptography PyQt6 requests -i https://pypi.tuna.tsinghua.edu.cn/simple

Write-Host "==> 开始打包..."
& $py -m PyInstaller packaging\bilihistory.spec --noconfirm --clean

Write-Host "==> 完成。产物: $root\dist\BiliHistory.exe"

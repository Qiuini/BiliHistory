# B站历史记录管理工具 - Windows Nuitka 打包脚本
# 用法（在项目根目录）：
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
# 产物：dist\BiliHistory.exe

$ErrorActionPreference = "Stop"

# 定位项目根目录（脚本上一级）
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 选择 venv 内的 python（存在则用），否则用 py -3.12 或系统 python
$py = Join-Path $root ".venv\Scripts\python.exe"
if (-not (Test-Path $py)) {
    $pyCandidates = @("py -3.12", "python")
    foreach ($c in $pyCandidates) {
        try { & ($c -split ' ')[0] ($c -split ' ')[1] --version | Out-Null; $py = $c; break } catch {}
    }
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

Write-Host "==> 完成。产物: $root\dist\BiliHistory.exe"

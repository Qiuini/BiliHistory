# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller 打包配置 - B站历史记录管理工具（GUI）

构建：
    pyinstaller packaging/bilihistory.spec --noconfirm
产物：
    dist/BiliHistory.exe（Windows onefile，无控制台窗口）
    在 Linux 上执行同一 spec 会产出对应的单文件可执行程序。

说明：
- 私钥 tools/private_key.pem 不会被打包（仅内置公钥 src/licensing/keys.py）。
- 用户数据（Cookie / CSV / 授权 / 试用）运行时写入用户数据目录，不在安装目录。
"""
import os
from pathlib import Path

ROOT = Path(os.getcwd())
ICON = ROOT / "packaging" / "icon.ico"
VERSION_INFO = ROOT / "packaging" / "version_info.txt"

datas = [
    # config.py 通过 Path(__file__).parent / 'config.json' 读取，onefile 下需置于根
    (str(ROOT / "src" / "config.json"), "."),
]

a = Analysis(
    [str(ROOT / "gui_main.py")],
    pathex=[str(ROOT), str(ROOT / "src")],
    binaries=[],
    datas=datas,
    hiddenimports=["licensing", "licensing.license_manager", "licensing.trial", "licensing.keys"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=["tkinter", "pytest"],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="BiliHistory",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=(str(ICON) if ICON.is_file() else None),
    version=(str(VERSION_INFO) if VERSION_INFO.is_file() else None),
)

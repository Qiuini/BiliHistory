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
:
import sys
from pathlib import Path

ROOT = Path.cwd()
ICON = ROOT / "favicon.ico"

# 动态生成 PyInstaller 版本信息文件，避免硬编码
sys.path.insert(0, str(ROOT / "src"))
from version import APP_NAME, APP_NAME_CN, APP_VERSION, APP_VERSION_TUPLE

VERSION_INFO = ROOT / "packaging" / "version_info.txt"
version_text = f"""# UTF-8
# PyInstaller 版本信息文件（Windows 文件属性）
VSVersionInfo(
  ffi=FixedFileInfo(
    filevers={APP_VERSION_TUPLE + (0,)},
    prodvers={APP_VERSION_TUPLE + (0,)},
    mask=0x3f,
    flags=0x0,
    OS=0x40004,
    fileType=0x1,
    subtype=0x0,
    date=(0, 0)
  ),
  kids=[
    StringFileInfo(
      [
        StringTable(
          u'080404b0',
          [StringStruct(u'CompanyName', u'{APP_NAME}'),
           StringStruct(u'FileDescription', u'{APP_NAME_CN}'),
           StringStruct(u'FileVersion', u'{APP_VERSION}'),
           StringStruct(u'InternalName', u'{APP_NAME}'),
           StringStruct(u'LegalCopyright', u'Copyright (C) 2026'),
           StringStruct(u'OriginalFilename', u'{APP_NAME}.exe'),
           StringStruct(u'ProductName', u'{APP_NAME_CN}'),
           StringStruct(u'ProductVersion', u'{APP_VERSION}')])
      ]),
    VarFileInfo([VarStruct(u'Translation', [2052, 1200])])
  ]
)
"""
VERSION_INFO.write_text(version_text, encoding="utf-8")

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

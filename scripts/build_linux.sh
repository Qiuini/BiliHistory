#!/usr/bin/env bash
# B站历史记录管理工具 - Linux 打包脚本
# 用法（在项目根目录）：  bash scripts/build_linux.sh
# 产物：
#   dist/BiliHistory              PyInstaller 单文件可执行
#   dist/BiliHistory-x86_64.tar.gz  tar.gz 便携包
#   dist/*.deb                    Debian 安装包（需 dpkg-deb）
#   dist/*.AppImage               AppImage（需 appimagetool，可选）
#
# 注意：不能在 Windows 上交叉构建 Linux 包，请在 Linux 环境、WSL 或 GitHub Actions 执行。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PY="${PYTHON:-python3}"
VERSION="1.0.0"
APP="BiliHistory"
APP_LOWER="${APP,,}"

echo "==> 安装构建依赖"
"$PY" -m pip install pyinstaller cryptography PyQt6 requests Pillow

echo "==> 准备图标资源"
"$PY" scripts/prepare_assets.py

echo "==> PyInstaller 打包"
"$PY" -m PyInstaller packaging/bilihistory.spec --noconfirm --clean

echo "==> 生成 tar.gz 便携包"
( cd dist && tar czf "${APP}-x86_64.tar.gz" "${APP}" )

# ---------------- .deb ----------------
if command -v dpkg-deb >/dev/null 2>&1; then
  echo "==> 构建 .deb"
  PKG="dist/deb/${APP_LOWER}_${VERSION}_amd64"
  rm -rf "$PKG"
  mkdir -p "$PKG/DEBIAN" "$PKG/usr/bin" "$PKG/usr/share/applications" "$PKG/usr/share/icons/hicolor/256x256/apps"
  sed "s/@VERSION@/${VERSION}/g" packaging/debian/control > "$PKG/DEBIAN/control"
  install -m 0755 "dist/${APP}" "$PKG/usr/bin/${APP_LOWER}"
  sed "s/@APP@/${APP_LOWER}/g" packaging/debian/bilihistory.desktop > "$PKG/usr/share/applications/${APP_LOWER}.desktop"
  if [ -f packaging/icon.png ]; then
    cp packaging/icon.png "$PKG/usr/share/icons/hicolor/256x256/apps/${APP_LOWER}.png"
  fi
  dpkg-deb --build --root-owner-group "$PKG" "dist/${APP_LOWER}_${VERSION}_amd64.deb"
  echo "    生成: dist/${APP_LOWER}_${VERSION}_amd64.deb"
else
  echo "!! 未检测到 dpkg-deb，跳过 .deb 构建"
fi

# ---------------- AppImage（可选） ----------------
if command -v appimagetool >/dev/null 2>&1; then
  echo "==> 构建 AppImage"
  APPDIR="dist/${APP}.AppDir"
  rm -rf "$APPDIR"
  mkdir -p "$APPDIR/usr/bin"
  install -m 0755 "dist/${APP}" "$APPDIR/usr/bin/${APP_LOWER}"
  sed "s/@APP@/${APP_LOWER}/g" packaging/debian/bilihistory.desktop > "$APPDIR/${APP_LOWER}.desktop"
  if [ -f packaging/icon.png ]; then
    cp packaging/icon.png "$APPDIR/${APP_LOWER}.png"
  fi
  printf '#!/bin/sh\nexec "$(dirname "$0")/usr/bin/%s" "$@"\n' "${APP_LOWER}" > "$APPDIR/AppRun"
  chmod +x "$APPDIR/AppRun"
  appimagetool "$APPDIR" "dist/${APP}-x86_64.AppImage"
else
  echo "!! 未检测到 appimagetool，跳过 AppImage 构建"
fi

echo "==> 完成，产物见 dist/"

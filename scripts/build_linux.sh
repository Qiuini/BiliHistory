#!/usr/bin/env bash
# B站历史记录管理工具 - Linux 多架构/多格式打包脚本
# 用法（在项目根目录）： ARCH=amd64 bash scripts/build_linux.sh
# 环境变量：
#   ARCH       目标架构：amd64 / arm64（默认 amd64）
#              注：Linux i386 / armhf 因 PyQt6 官方未提供 wheel，CI 不构建
#   PYTHON     Python 解释器（默认 python3）
#
# 产物（dist/ 目录，文件名带架构后缀）：
#   BiliHistory-<arch>                  PyInstaller 单文件可执行
#   BiliHistory-<arch>.tar.gz/.bz2/.xz  便携压缩包
#   BiliHistory-<arch>.zip              便携 zip 包
#   bilihistory_<version>_<arch>.deb    Debian/Ubuntu 安装包
#   bilihistory-<version>-1.<arch>.rpm  Fedora/openSUSE/RHEL 安装包
#   bilihistory-<version>-1-<arch>.pkg.tar.zst  Arch Linux 安装包
#   bilihistory-<version>-<arch>.apk    Alpine Linux 安装包
#   bilihistory-<version>-<arch>.sh     自解压 shell 安装包
#   bilihistory-<version>-<arch>.txz    Slackware 安装包
#   BiliHistory-<arch>.AppImage         AppImage（需 appimagetool，可选）
#   bilihistory-<version>.ebuild        Gentoo ebuild 模板
#
# 架构说明：
#   - amd64 / arm64：PyQt6 官方提供 Linux wheel，完整支持
#   - i386 / armhf：PyQt6 官方未提供 Linux wheel，脚本保留映射但 CI 不构建
# 注意：请在 Linux 环境、WSL、Docker 或 GitHub Actions 执行。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ARCH="${ARCH:-amd64}"
PY="${PYTHON:-python3}"
APP="BiliHistory"
APP_LOWER="${APP,,}"

VERSION="$($PY -c "import sys; sys.path.insert(0, 'src'); from version import APP_VERSION; print(APP_VERSION)" 2>/dev/null || echo "1.0.0")"

echo "==> 构建版本: $VERSION  目标架构: $ARCH"

# 安装构建依赖
echo "==> 安装构建依赖"
"$PY" -m pip install pyinstaller cryptography PyQt6==6.7.1 requests Pillow

# 安装 fpm（一次性生成 deb/rpm/pacman 等多格式）
if ! command -v fpm >/dev/null 2>&1; then
  echo "==> 安装 fpm"
  if command -v gem >/dev/null 2>&1; then
    gem install --no-document fpm
  else
    echo "!! 未检测到 gem，跳过 fpm 相关包格式"
  fi
fi

echo "==> 准备图标资源"
"$PY" scripts/prepare_assets.py

echo "==> PyInstaller 打包"
"$PY" -m PyInstaller packaging/bilihistory.spec --noconfirm --clean

BIN="dist/${APP}"
BIN_ARCH="dist/${APP}-${ARCH}"
mv "$BIN" "$BIN_ARCH"
chmod +x "$BIN_ARCH"
BIN="$BIN_ARCH"

echo "==> 生成 tar.* 便携包"
( cd dist && \
  tar czf "${APP}-${ARCH}.tar.gz" "${APP}-${ARCH}" && \
  tar cjf "${APP}-${ARCH}.tar.bz2" "${APP}-${ARCH}" && \
  tar cJf "${APP}-${ARCH}.tar.xz" "${APP}-${ARCH}" )

# ---------------- zip 便携包 ----------------
if command -v zip >/dev/null 2>&1; then
  echo "==> 生成 zip 便携包"
  ( cd dist && zip -q "${APP}-${ARCH}.zip" "${APP}-${ARCH}" )
else
  echo "!! 未检测到 zip，跳过 zip 构建"
fi

# ---------------- 使用 fpm 统一生成 deb/rpm/pacman/apk/sh ----------------
if command -v fpm >/dev/null 2>&1; then
  echo "==> 使用 fpm 生成 deb / rpm / pacman / apk / sh"

  STAGE="dist/fpm-stage"
  rm -rf "$STAGE"
  mkdir -p "$STAGE/usr/bin" "$STAGE/usr/share/applications" "$STAGE/usr/share/icons/hicolor/256x256/apps"
  install -m 0755 "$BIN" "$STAGE/usr/bin/${APP_LOWER}"
  sed "s/@APP@/${APP_LOWER}/g" packaging/debian/bilihistory.desktop > "$STAGE/usr/share/applications/${APP_LOWER}.desktop"
  if [ -f packaging/icon.png ]; then
    cp packaging/icon.png "$STAGE/usr/share/icons/hicolor/256x256/apps/${APP_LOWER}.png"
  fi

  # 不同架构对应的包格式内部命名
  case "$ARCH" in
    amd64)  FPM_ARCH="amd64" ; RPM_ARCH="x86_64" ; PAC_ARCH="x86_64" ; APK_ARCH="x86_64" ; TXZ_ARCH="x86_64" ;;
    arm64)  FPM_ARCH="arm64" ; RPM_ARCH="aarch64" ; PAC_ARCH="aarch64" ; APK_ARCH="aarch64" ; TXZ_ARCH="aarch64" ;;
    i386)   FPM_ARCH="i386"  ; RPM_ARCH="i386" ; PAC_ARCH="i686" ; APK_ARCH="x86" ; TXZ_ARCH="i386" ;;
    *)      FPM_ARCH="$ARCH" ; RPM_ARCH="$ARCH" ; PAC_ARCH="$ARCH" ; APK_ARCH="$ARCH" ; TXZ_ARCH="$ARCH" ;;
  esac

  # .deb
  fpm -s dir -t deb \
    -n "$APP_LOWER" -v "$VERSION" -a "$FPM_ARCH" \
    --depends "libc6" --depends "libglib2.0-0" --depends "libgl1" \
    --description "B站历史记录管理工具" \
    --maintainer "BiliHistory <you@example.com>" \
    --license "Proprietary" --category "utils" \
    -C "$STAGE" -p "dist/${APP_LOWER}_${VERSION}_${FPM_ARCH}.deb" \
    --deb-no-default-config-files

  # .rpm
  fpm -s dir -t rpm \
    -n "$APP_LOWER" -v "$VERSION" -a "$RPM_ARCH" \
    --depends "glibc" --depends "glib2" --depends "mesa-libGL" \
    --description "B站历史记录管理工具" \
    --maintainer "BiliHistory <you@example.com>" \
    --license "Proprietary" --category "utils" \
    -C "$STAGE" -p "dist/${APP_LOWER}-${VERSION}-1.${RPM_ARCH}.rpm"

  # .pkg.tar.zst (Arch Linux)
  fpm -s dir -t pacman \
    -n "$APP_LOWER" -v "$VERSION" -a "$PAC_ARCH" \
    --depends "glibc" --depends "glib2" --depends "mesa" \
    --description "B站历史记录管理工具" \
    --maintainer "BiliHistory <you@example.com>" \
    --license "Proprietary" --category "utils" \
    -C "$STAGE" -p "dist/${APP_LOWER}-${VERSION}-1-${PAC_ARCH}.pkg.tar.zst"

  # .apk (Alpine Linux)
  fpm -s dir -t apk \
    -n "$APP_LOWER" -v "$VERSION" -a "$APK_ARCH" \
    --depends "musl" --depends "glib" --depends "mesa-gl" \
    --description "B站历史记录管理工具" \
    --maintainer "BiliHistory <you@example.com>" \
    --license "Proprietary" --category "utils" \
    -C "$STAGE" -p "dist/${APP_LOWER}-${VERSION}-${APK_ARCH}.apk"

  # .sh (self-extracting archive)
  fpm -s dir -t sh \
    -n "$APP_LOWER" -v "$VERSION" -a "$FPM_ARCH" \
    --description "B站历史记录管理工具" \
    --maintainer "BiliHistory <you@example.com>" \
    --license "Proprietary" --category "utils" \
    -C "$STAGE" -p "dist/${APP_LOWER}-${VERSION}-${FPM_ARCH}.sh"

  # .txz (Slackware)
  echo "==> 生成 Slackware txz 包"
  mkdir -p "$STAGE/install"
  cat > "$STAGE/install/slack-desc" <<EOF
bilihistory: BiliHistory (B站历史记录管理工具)
bilihistory:
bilihistory: 抓取、备份并管理个人 Bilibili 观看历史记录的桌面工具。
bilihistory: 支持游标分页抓取、快照备份、去重、排序与类型筛选。
bilihistory:
bilihistory:
bilihistory:
bilihistory:
bilihistory:
bilihistory:
bilihistory:
EOF
  ( cd "$STAGE" && tar cJf "${ROOT}/dist/${APP_LOWER}-${VERSION}-${TXZ_ARCH}.txz" . )
else
  echo "!! 未安装 fpm，跳过 deb/rpm/pacman/apk/sh/txz 构建"
fi

# ---------------- AppImage（可选） ----------------
if command -v appimagetool >/dev/null 2>&1; then
  echo "==> 构建 AppImage"
  APPDIR="dist/${APP}-${ARCH}.AppDir"
  rm -rf "$APPDIR"
  mkdir -p "$APPDIR/usr/bin"
  install -m 0755 "$BIN" "$APPDIR/usr/bin/${APP_LOWER}"
  sed "s/@APP@/${APP_LOWER}/g" packaging/debian/bilihistory.desktop > "$APPDIR/${APP_LOWER}.desktop"
  if [ -f packaging/icon.png ]; then
    cp packaging/icon.png "$APPDIR/${APP_LOWER}.png"
  fi
  printf '#!/bin/sh\nexec "$(dirname "$0")/usr/bin/%s" "$@"\n' "${APP_LOWER}" > "$APPDIR/AppRun"
  chmod +x "$APPDIR/AppRun"
  appimagetool --appimage-extract-and-run "$APPDIR" "dist/${APP}-${ARCH}.AppImage"
else
  echo "!! 未检测到 appimagetool，跳过 AppImage 构建"
fi

# ---------------- Gentoo ebuild 模板 ----------------
echo "==> 生成 Gentoo ebuild 模板"
cat > "dist/${APP_LOWER}-${VERSION}.ebuild" <<EOF
# Copyright 1999-2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

DESCRIPTION="B站历史记录管理工具"
HOMEPAGE="https://github.com/Qiuini/BiliHistory"
SRC_URI="https://github.com/Qiuini/BiliHistory/releases/download/v\${PV}/BiliHistory-${ARCH}.tar.gz"

LICENSE="all-rights-reserved"
SLOT="0"
KEYWORDS="-${ARCH}"
IUSE=""

RDEPEND="
    dev-libs/glib
    media-libs/mesa
    sys-libs/glibc
"
BDEPEND=""

src_unpack() {
    default
    mv "BiliHistory-${ARCH}" "\${P}" || die
}

src_install() {
    dobin "\${P}/BiliHistory-${ARCH}"
    dosym "/usr/bin/BiliHistory-${ARCH}" "/usr/bin/${APP_LOWER}"
}
EOF

echo "==> 完成，产物见 dist/"
ls -lh dist/

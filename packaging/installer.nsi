; BiliHistory Windows 安装程序（NSIS）
; 用法：makensis packaging\installer.nsi
; 前提：dist\BiliHistory.exe 和 favicon.ico 已存在

!define APP_NAME "BiliHistory"
!define APP_NAME_CN "BiliHistory - B站历史记录管理工具"
!define APP_VERSION "1.0.0"
!define PUBLISHER "Qiuini"
!define APP_URL "https://github.com/Qiuini/BiliHistory"

Unicode True
SetCompressor /SOLID lzma

Name "${APP_NAME_CN}"
OutFile "dist\${APP_NAME}-${APP_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKCU "Software\${APP_NAME}" "InstallDir"
RequestExecutionLevel admin

; 图标路径：默认相对当前工作目录，可通过 /DICON_PATH=... 覆盖
!ifndef ICON_PATH
  !define ICON_PATH "favicon.ico"
!endif

; 界面
!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "${ICON_PATH}"
!define MUI_UNICON "${ICON_PATH}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "安装主程序" SecMain
    SetOutPath "$INSTDIR"
    File "dist\BiliHistory.exe"
    File "favicon.ico"

    ; 写入卸载程序
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; 创建开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\BiliHistory.exe" "" "$INSTDIR\favicon.ico" 0
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\卸载.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0

    ; 创建桌面快捷方式
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\BiliHistory.exe" "" "$INSTDIR\favicon.ico" 0

    ; 注册表：安装路径与卸载信息
    WriteRegStr HKCU "Software\${APP_NAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME_CN}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\favicon.ico"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "URLInfoAbout" "${APP_URL}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\BiliHistory.exe"
    Delete "$INSTDIR\favicon.ico"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\卸载.lnk"
    RMDir "$SMPROGRAMS\${APP_NAME}"

    Delete "$DESKTOP\${APP_NAME}.lnk"

    DeleteRegKey HKCU "Software\${APP_NAME}"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
SectionEnd

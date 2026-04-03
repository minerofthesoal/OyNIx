; OyNIx Browser v2.3 - Windows NSIS Installer Script
; Compile with: makensis oynix-installer.nsi
; Requires: NSIS 3.x (https://nsis.sourceforge.io/)

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ── Installer Settings ──────────────────────────────────────────────
Name "OyNIx Browser"
OutFile "OyNIx-2.3-Setup.exe"
InstallDir "$LOCALAPPDATA\OyNIx"
InstallDirRegKey HKCU "Software\OyNIx" "InstallDir"
RequestExecutionLevel user
Unicode True

; ── UI Settings ─────────────────────────────────────────────────────
!define MUI_ICON "..\..\icons\oynix.ico"
!define MUI_UNICON "..\..\icons\oynix.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to OyNIx Browser Setup"
!define MUI_WELCOMEPAGE_TEXT "OyNIx is a Nyx-themed Chromium-based desktop browser$\r$\nwith local AI, privacy features, and a custom search engine.$\r$\n$\r$\nClick Next to continue."
!define MUI_FINISHPAGE_RUN "$INSTDIR\oynix-launch.vbs"
!define MUI_FINISHPAGE_RUN_TEXT "Launch OyNIx Browser"

; ── Pages ───────────────────────────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install Section ─────────────────────────────────────────────────
Section "OyNIx Browser" SecMain
    SetOutPath "$INSTDIR"

    ; Core Python package
    File /r "..\..\oynix\*.*"
    SetOutPath "$INSTDIR"
    File "..\..\oynix-launch.bat"
    File "..\..\oynix-launch.vbs"
    File "..\..\pyproject.toml"
    File "..\..\run.sh"

    ; Icons
    SetOutPath "$INSTDIR\icons"
    File /nonfatal "..\..\icons\*.*"

    ; Create oynix package marker
    SetOutPath "$INSTDIR\oynix"
    FileOpen $0 "$INSTDIR\oynix\__init__.py" w
    FileWrite $0 ""
    FileClose $0

    ; Create Start Menu shortcuts
    CreateDirectory "$SMPROGRAMS\OyNIx"
    CreateShortcut "$SMPROGRAMS\OyNIx\OyNIx Browser.lnk" \
        "wscript.exe" '"$INSTDIR\oynix-launch.vbs"' \
        "$INSTDIR\icons\oynix.ico" 0
    CreateShortcut "$SMPROGRAMS\OyNIx\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Desktop shortcut
    CreateShortcut "$DESKTOP\OyNIx Browser.lnk" \
        "wscript.exe" '"$INSTDIR\oynix-launch.vbs"' \
        "$INSTDIR\icons\oynix.ico" 0

    ; URL protocol handler (oyn://)
    WriteRegStr HKCU "Software\Classes\oyn" "" "URL:OyNIx Protocol"
    WriteRegStr HKCU "Software\Classes\oyn" "URL Protocol" ""
    WriteRegStr HKCU "Software\Classes\oyn\shell\open\command" "" \
        'wscript.exe "$INSTDIR\oynix-launch.vbs" "%1"'

    ; File associations (.npi, .nydta)
    WriteRegStr HKCU "Software\Classes\.npi" "" "OyNIx.Extension"
    WriteRegStr HKCU "Software\Classes\OyNIx.Extension" "" "OyNIx Extension"
    WriteRegStr HKCU "Software\Classes\OyNIx.Extension\shell\open\command" "" \
        'wscript.exe "$INSTDIR\oynix-launch.vbs" "%1"'

    WriteRegStr HKCU "Software\Classes\.nydta" "" "OyNIx.Profile"
    WriteRegStr HKCU "Software\Classes\OyNIx.Profile" "" "OyNIx Profile"
    WriteRegStr HKCU "Software\Classes\OyNIx.Profile\shell\open\command" "" \
        'wscript.exe "$INSTDIR\oynix-launch.vbs" "%1"'

    ; Registry for uninstall
    WriteRegStr HKCU "Software\OyNIx" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx" \
        "DisplayName" "OyNIx Browser"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx" \
        "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx" \
        "DisplayIcon" "$INSTDIR\icons\oynix.ico"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx" \
        "Publisher" "OyNIx Team"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx" \
        "DisplayVersion" "2.3"

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx" \
        "EstimatedSize" "$0"

    ; Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; ── Uninstall Section ───────────────────────────────────────────────
Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR\oynix"
    RMDir /r "$INSTDIR\icons"
    Delete "$INSTDIR\oynix-launch.bat"
    Delete "$INSTDIR\oynix-launch.vbs"
    Delete "$INSTDIR\pyproject.toml"
    Delete "$INSTDIR\run.sh"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"

    ; Remove shortcuts
    Delete "$SMPROGRAMS\OyNIx\OyNIx Browser.lnk"
    Delete "$SMPROGRAMS\OyNIx\Uninstall.lnk"
    RMDir "$SMPROGRAMS\OyNIx"
    Delete "$DESKTOP\OyNIx Browser.lnk"

    ; Remove registry
    DeleteRegKey HKCU "Software\OyNIx"
    DeleteRegKey HKCU "Software\Classes\oyn"
    DeleteRegKey HKCU "Software\Classes\.npi"
    DeleteRegKey HKCU "Software\Classes\OyNIx.Extension"
    DeleteRegKey HKCU "Software\Classes\.nydta"
    DeleteRegKey HKCU "Software\Classes\OyNIx.Profile"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OyNIx"
SectionEnd

; PowerNapp installer (NSIS 3). Per-user install, no admin rights needed.
;   makensis /DVERSION=1.0.0 installer\powernapp.nsi   ->  PowerNapp-setup.exe
Unicode true
!define MUI_ICON "..\powernapp.ico"
!define MUI_UNICON "..\powernapp.ico"
SetCompressor /SOLID lzma
RequestExecutionLevel user

!ifndef VERSION
  !define VERSION "2.0.0"
!endif
!define APP "PowerNapp"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP}"

Name "${APP}"
OutFile "..\PowerNapp-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\${APP}"
InstallDirRegKey HKCU "${UNINST_KEY}" "InstallLocation"
BrandingText "${APP} ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APP}"
VIAddVersionKey "FileDescription" "${APP} setup"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" ""

!include "MUI2.nsh"
!include "FileFunc.nsh"   ; GetSize
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\powernapp.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open ${APP}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

; A running copy holds the exe open; ask it to go away (tray copy included).
!macro CLOSE_RUNNING
  nsExec::Exec 'taskkill /IM powernapp.exe /F'
  Pop $0
  Sleep 300
!macroend

Section "Install"
  SetShellVarContext current
  !insertmacro CLOSE_RUNNING
  SetOutPath "$INSTDIR"
  File "..\powernapp.exe"
  ; translations: lang\<tag>.txt next to the exe (English is built in)
  SetOutPath "$INSTDIR\lang"
  File "..\lang\*.txt"
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\${APP}.lnk" "$INSTDIR\powernapp.exe" "" "$INSTDIR\powernapp.exe" 0 \
    SW_SHOWNORMAL "" "Shut down, restart or sleep the PC on a schedule"

  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APP}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "Denis Platonov"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\powernapp.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  ; what is installed (exe, lang\, uninstaller), in KB
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"

  ; Upgrade from PowerOff 1.x (the app's old name): carry its settings, scheduled tasks and
  ; startup entry over to PowerNapp, then remove it with its own uninstaller.
  IfFileExists "$LOCALAPPDATA\Programs\PowerOff\uninstall.exe" 0 no_poweroff
    nsExec::Exec 'taskkill /IM poweroff.exe /F'
    Pop $0
    Sleep 300
    StrCpy $0 "not run"
    ExecWait '"$INSTDIR\powernapp.exe" --migrate' $0
    ; nonzero: a scheduled task could not be moved and still runs PowerOff. Its uninstaller would
    ; delete that task, so PowerOff stays installed (PowerNapp tries the move again when it starts).
    StrCmp $0 "0" 0 poweroff_kept
    ExecWait '"$LOCALAPPDATA\Programs\PowerOff\uninstall.exe" /S _?=$LOCALAPPDATA\Programs\PowerOff'
    Delete "$LOCALAPPDATA\Programs\PowerOff\uninstall.exe"
    RMDir "$LOCALAPPDATA\Programs\PowerOff"
    Goto no_poweroff
  poweroff_kept:
    DetailPrint "PowerOff kept installed: not every scheduled task could be moved (powernapp.exe --migrate: $0)"
  no_poweroff:
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  !insertmacro CLOSE_RUNNING
  ; scheduled jobs PowerNapp created (GUI hand-off and --install-daily)
  nsExec::Exec 'schtasks /delete /tn "PowerNapp" /f'
  Pop $0
  nsExec::Exec 'schtasks /delete /tn "PowerNapp daily" /f'
  Pop $0
  nsExec::Exec 'schtasks /delete /tn "PowerNapp wake" /f'
  Pop $0
  nsExec::Exec 'schtasks /delete /tn "PowerNapp startup" /f'
  Pop $0
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "PowerNapp"

  Delete "$SMPROGRAMS\${APP}.lnk"
  Delete "$INSTDIR\powernapp.exe"
  Delete "$INSTDIR\uninstall.exe"
  Delete "$INSTDIR\lang\*.txt"
  RMDir "$INSTDIR\lang"
  RMDir "$INSTDIR"
  RMDir /r "$APPDATA\PowerNapp"   ; settings (powernapp.ini)
  DeleteRegKey HKCU "${UNINST_KEY}"
SectionEnd

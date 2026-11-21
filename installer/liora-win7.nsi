; Windows installer for the Windows 7/8 build of Liora (NSIS 3). Built by CI from the packaged
; folder and the build folder, like liora.nsi:
;
;   makensis -DVERSION=0.1.1 -DSOURCE=<package folder> -DICON=<build>/generated/liora.ico
;            -DNAMES=<build>/generated/names.nsh -DOUTFILE=<setup.exe> liora-win7.nsi
;
; The names (program, folder, settings) come from CMakeLists.txt through names.nsh.
;
; A copy of liora.nsi with its own name, folder, shortcuts and Apps entry, so it never touches an
; installed regular Liora (Windows 10/11). Keep changes to liora.nsi in step here. It also stops on
; Windows older than 7 SP1 and on 32-bit Windows, which the package doesn't run on.
;
; Installs for the current user only (no admin rights) into %LOCALAPPDATA%\Programs\<name>-Win7,
; with a Start menu entry and an optional desktop shortcut. Uninstalling removes the program, and
; the user's data (database and settings, shared with the regular Liora) only when they tick that
; option.

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"

!ifndef VERSION | SOURCE | ICON | NAMES | OUTFILE
    !error "Pass VERSION, SOURCE, ICON, NAMES and OUTFILE with -D (see the top of this file)"
!endif
!include "${NAMES}"

!define DISPLAY_NAME "${NAME} for Windows 7 and 8"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}-Win7"
!define FOLDER_NAME "${NAME}-Win7"                 ; No space: Wine quotes a /D=... with one, which NSIS ignores

Name "${DISPLAY_NAME}"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${FOLDER_NAME}"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
BrandingText "${NAME} ${VERSION}"
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${DISPLAY_NAME}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${NAME} installer for Windows 7 and 8"
VIAddVersionKey "LegalCopyright" "The ${NAME} contributors"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_DIRECTORYPAGE_TEXT_TOP "${NAME} will be installed for your user account. The folder must be named ${FOLDER_NAME}, so uninstalling can remove it safely."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${NAME}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${NAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE}/LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; This build is 64-bit and needs Windows 7 with Service Pack 1 or newer
Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "${DISPLAY_NAME} needs 64-bit Windows." /SD IDOK
        Abort
    ${EndIf}
    ${IfNot} ${AtLeastWin7}
        MessageBox MB_ICONSTOP "${DISPLAY_NAME} needs Windows 7 with Service Pack 1 or newer." /SD IDOK
        Abort
    ${EndIf}
    ${If} ${IsWin7}
    ${AndIfNot} ${AtLeastServicePack} 1
        MessageBox MB_ICONSTOP "${DISPLAY_NAME} needs Service Pack 1 for Windows 7." /SD IDOK
        Abort
    ${EndIf}
FunctionEnd

; Installing and uninstalling delete the whole folder, so it must be the program's own, never the
; Desktop or Documents: its name must be ${FOLDER_NAME}. Checked on the directory page, and again
; before deleting, since a silent install (/S /D=...) skips that page.
; $0 is "1" when $INSTDIR ends in \${FOLDER_NAME}. Measured, not counted by hand, so a new name
; needs no new number.
!macro IsOwnFolder
    StrLen $1 "\${FOLDER_NAME}"
    IntOp $1 0 - $1
    StrCpy $0 "$INSTDIR" "" $1
    ${If} $0 == "\${FOLDER_NAME}"
        StrCpy $0 "1"
    ${Else}
        StrCpy $0 "0"
    ${EndIf}
!macroend

!macro CheckOwnFolder
    !insertmacro IsOwnFolder
    ${If} $0 != "1"
        MessageBox MB_ICONSTOP "The folder must be named ${FOLDER_NAME}: $INSTDIR" /SD IDOK
        Abort
    ${EndIf}
!macroend

Function .onVerifyInstDir
    !insertmacro IsOwnFolder
    ${If} $0 != "1"
        Abort
    ${EndIf}
FunctionEnd

Section "${NAME}" SecProgram
    SectionIn RO
    !insertmacro CheckOwnFolder
    ; Remove the previous version's files first, so no outdated DLL stays behind
    RMDir /r "$INSTDIR"
    SetOutPath "$INSTDIR"
    File /r "${SOURCE}/*.*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateShortcut "$SMPROGRAMS\${DISPLAY_NAME}.lnk" "$INSTDIR\${NAME}.exe"

    ; Entry under Control Panel > Programs and Features (Settings > Apps on newer Windows)
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${DISPLAY_NAME}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "The ${NAME} contributors"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${NAME}.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" $0
SectionEnd

Section /o "Desktop shortcut" SecDesktop
    CreateShortcut "$DESKTOP\${DISPLAY_NAME}.lnk" "$INSTDIR\${NAME}.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecProgram} "The program, with a Start menu entry."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "A shortcut to ${NAME} on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Removes the program, its shortcuts and its entry in the list of programs
Section "un.${NAME}" SecUnProgram
    SectionIn RO
    !insertmacro CheckOwnFolder
    Delete "$SMPROGRAMS\${DISPLAY_NAME}.lnk"
    Delete "$DESKTOP\${DISPLAY_NAME}.lnk"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    RMDir /r "$INSTDIR"                              ; Checked above: a folder named ${FOLDER_NAME}
SectionEnd

; Only when ticked (never in a silent uninstall): the program's own data. Where it keeps it: the
; app data folder (QStandardPaths::AppDataLocation, the default database) and the registry key of
; its QSettings(ORGANIZATION, NAME). Databases saved elsewhere aren't the program's to delete. The
; regular ${NAME} uses the same data.
Section /o "un.My data" SecUnData
    RMDir /r "$APPDATA\${NAME}"
    DeleteRegKey HKCU "Software\${ORGANIZATION}\${NAME}"
    DeleteRegKey /ifempty HKCU "Software\${ORGANIZATION}"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUnProgram} "The program, its shortcuts and its entry in the list of programs."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUnData} "Also deletes your data for good: the database in $APPDATA\${NAME} and all settings, which a regular ${NAME} on this PC uses too. Databases you saved in other folders are kept."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

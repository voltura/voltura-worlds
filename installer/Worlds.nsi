Unicode true
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!ifndef PRODUCT_KEY
 !define PRODUCT_KEY "VolturaWorlds"
!endif
!ifndef SHORTCUT_NAME
 !define SHORTCUT_NAME "Voltura Worlds"
!endif
!ifndef WORLD_CLASS
 !define WORLD_CLASS "Voltura.World"
!endif
!ifndef FILE_EXTENSION
 !define FILE_EXTENSION ".world"
!endif
!ifndef PROFILE_DIR
 !define PROFILE_DIR "$LOCALAPPDATA\Voltura\Worlds"
!endif
!ifndef OUTPUT
 !define OUTPUT "..\dist\VolturaWorlds-Setup-0.1.6-win-x64.exe"
!endif
Name "Voltura Worlds"
Caption "Voltura Worlds 0.1.6 Setup"
BrandingText "Voltura AB"
ManifestDPIAware true
ManifestSupportedOS all
VIProductVersion "0.1.6.0"
VIAddVersionKey "ProductName" "Voltura Worlds"
VIAddVersionKey "CompanyName" "Voltura AB"
VIAddVersionKey "FileDescription" "Voltura Worlds Setup"
VIAddVersionKey "FileVersion" "0.1.6"
VIAddVersionKey "ProductVersion" "0.1.6"
VIAddVersionKey "OriginalFilename" "VolturaWorlds-Setup-0.1.6-win-x64.exe"
VIAddVersionKey "LegalCopyright" "Voltura AB"
!define MUI_ICON "artwork\worlds.ico"
!define MUI_UNICON "artwork\worlds.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "artwork\worlds-sidebar.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP_STRETCH "FitControl"
!define MUI_WELCOMEPAGE_TITLE "Your desktop, alive."
!define MUI_WELCOMEPAGE_TEXT "Install Voltura Worlds to bring ten animated Worlds to your Windows desktop.$\r$\n$\r$\nChoose moving artwork, add your own videos, and optionally let your Worlds react to audio.$\r$\n$\r$\nInstalls for your account. No administrator access required."
!define MUI_FINISHPAGE_TITLE "Your Worlds are ready"
!define MUI_FINISHPAGE_TEXT "Open Voltura Worlds to choose your artwork.$\r$\n$\r$\nUse the notification-area icon to open settings, pause animation, or exit."
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\Voltura Worlds"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
SetDatablockOptimize on
Var StartupCheckbox
Var StartupEnabled
Var AutomaticUpdate
Var UpdateCancelName
Var SetupMutex
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchWorlds
!define MUI_FINISHPAGE_RUN_TEXT "Start Voltura Worlds"
!define MUI_PAGE_CUSTOMFUNCTION_SHOW SmoothWelcomeArtwork
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
Page custom OptionsPage OptionsLeave
!insertmacro MUI_PAGE_INSTFILES
!define MUI_PAGE_CUSTOMFUNCTION_SHOW SmoothFinishArtwork
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!include "installer-artwork.nsh"
!insertmacro DefineSmoothArtwork ""
Function LaunchWorlds
 # Launch while setup owns foreground permission, then release the finish page.
 ExecShell "open" "$INSTDIR\VolturaWorlds.exe" "" SW_SHOWNORMAL
 ShowWindow $HWNDPARENT ${SW_HIDE}
FunctionEnd
Function .onInit
 SetRegView 64
 System::Call 'kernel32::CreateMutexW(p 0, i 0, w "Local\VolturaWorldsSetup") p.r0 ?e'
 Pop $1
 StrCpy $SetupMutex $0
 ${If} $0 == 0
 ${OrIf} $1 == 183
  SetErrorLevel 1
  Abort
 ${EndIf}
 StrCpy $AutomaticUpdate 0
 ${GetParameters} $0
 ClearErrors
 ${GetOptions} $0 "/AUTOMATICUPDATE" $1
 ${IfNot} ${Errors}
  StrCpy $AutomaticUpdate 1
  IfSilent +3
   SetErrorLevel 1
   Abort
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "InstallLocation"
  ${If} $0 != $INSTDIR
   SetErrorLevel 1
   Abort
  ${EndIf}
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/UPDATECANCEL=" $UpdateCancelName
  ${If} ${Errors}
   SetErrorLevel 1
   Abort
  ${EndIf}
  Call CheckAutomaticUpdateAllowed
 ${EndIf}
 StrCpy $StartupEnabled ${BST_CHECKED}
 ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayVersion"
 ${If} $0 != ""
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_KEY}"
  ${If} $0 == ""
   StrCpy $StartupEnabled ${BST_UNCHECKED}
  ${EndIf}
 ${EndIf}
 ${GetParameters} $0
 ClearErrors
 ${GetOptions} $0 "/NOSTARTUP" $1
 ${IfNot} ${Errors}
  StrCpy $StartupEnabled ${BST_UNCHECKED}
 ${EndIf}
 FindWindow $0 "VolturaWorldController"
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONINFORMATION "Please exit Voltura Worlds from the notification area before installing." /SD IDOK
  Abort
 ${EndIf}
FunctionEnd
Function CheckAutomaticUpdateAllowed
 ${If} $AutomaticUpdate == 1
  System::Call 'user32::GetSystemMetrics(i 0x2000) i.r0'
  ${If} $0 != 0
   SetErrorLevel 1
   Abort
  ${EndIf}
  System::Call 'kernel32::OpenEventW(i 0x100000, i 0, w "$UpdateCancelName") p.r1'
  ${If} $1 == 0
   SetErrorLevel 1
   Abort
  ${EndIf}
  System::Call 'kernel32::WaitForSingleObject(p r1, i 0) i.r0'
  System::Call 'kernel32::CloseHandle(p r1)'
  ${If} $0 != 258
   SetErrorLevel 1
   Abort
  ${EndIf}
  ReadINIStr $0 "${PROFILE_DIR}\preferences.ini" "worlds" "automaticUpdates"
  ${If} $0 != 1
   SetErrorLevel 1
   Abort
  ${EndIf}
 ${EndIf}
FunctionEnd
Function OptionsPage
 !insertmacro MUI_HEADER_TEXT "Make yourself at home" "Choose how Voltura Worlds starts."
 nsDialogs::Create 1018
 Pop $0
 ${NSD_CreateLabel} 0 0 100% 32u "Worlds runs quietly behind your desktop. You can pause it or open its library from the notification area."
 Pop $0
 ${NSD_CreateCheckbox} 0 48u 100% 16u "Start with Windows"
 Pop $StartupCheckbox
 ${NSD_SetState} $StartupCheckbox $StartupEnabled
 nsDialogs::Show
FunctionEnd
Function OptionsLeave
 ${NSD_GetState} $StartupCheckbox $StartupEnabled
FunctionEnd
Section "Voltura Worlds"
 SetShellVarContext current
 SetOutPath "$INSTDIR"
 SetOverwrite on
 File /oname=VolturaWorlds.update-new.exe "..\build\Release\VolturaWorlds.exe"
 Call CheckAutomaticUpdateAllowed
 IfFileExists "$INSTDIR\VolturaWorlds.update-old.exe" 0 +4
  Delete "$INSTDIR\VolturaWorlds.update-new.exe"
  SetErrorLevel 1
  Abort
 IfFileExists "$INSTDIR\VolturaWorlds.exe" 0 replace_worlds
  System::Call 'kernel32::ReplaceFileW(w "$INSTDIR\VolturaWorlds.exe", w "$INSTDIR\VolturaWorlds.update-new.exe", w "$INSTDIR\VolturaWorlds.update-old.exe", i 0, p 0, p 0) i.r0'
  ${If} $0 == 0
   IfFileExists "$INSTDIR\VolturaWorlds.exe" +2 0
    Rename "$INSTDIR\VolturaWorlds.update-old.exe" "$INSTDIR\VolturaWorlds.exe"
   Delete "$INSTDIR\VolturaWorlds.update-new.exe"
   SetErrorLevel 1
   Abort
  ${EndIf}
  Goto worlds_replaced
 replace_worlds:
 ClearErrors
 Rename "$INSTDIR\VolturaWorlds.update-new.exe" "$INSTDIR\VolturaWorlds.exe"
 ${If} ${Errors}
  Delete "$INSTDIR\VolturaWorlds.update-new.exe"
  SetErrorLevel 1
  Abort
 ${EndIf}
 worlds_replaced:
 Delete "$INSTDIR\VolturaWorlds.update-old.exe"
 File "..\LICENSE"
 File "..\THIRD-PARTY-NOTICES.md"
 SetOutPath "$INSTDIR\licenses"
 File "..\licenses\*.txt"
 SetOutPath "$INSTDIR\assets\orbit"
 Delete "$INSTDIR\assets\orbit\earth-january.jpg"
 Delete "$INSTDIR\assets\orbit\earth-clouds-8192.tif"
 File "..\assets\orbit\earth-night-2016.jpg"
 Delete "$INSTDIR\assets\orbit\milkyway-2020.png"
 File "..\assets\orbit\ATTRIBUTION.md"
 SetOutPath "$INSTDIR\assets\orbit\runtime"
 File "..\assets\orbit\runtime\earth-january.dds"
 File "..\assets\orbit\runtime\earth-clouds-8192.dds"
 File "..\assets\orbit\runtime\milkyway-2020.dds"
 SetOutPath "$INSTDIR\assets\aurora"
 File "..\assets\aurora\teal-silk-v1.png"
 File "..\assets\aurora\violet-silk-v1.png"
 File "..\assets\aurora\PROVENANCE.md"
 File "..\assets\aurora\approved-clean-v1.png"
 SetOutPath "$INSTDIR\shaders"
 File "..\build\Release\shaders\*.cso"
 SetOutPath "$INSTDIR\D3D12"
 File "..\build\Release\D3D12\D3D12Core.dll"
 SetOutPath "$INSTDIR"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 CreateShortcut "$SMPROGRAMS\${SHORTCUT_NAME}.lnk" "$INSTDIR\VolturaWorlds.exe"
 WriteRegStr HKCU "Software\Classes\${FILE_EXTENSION}" "" "${WORLD_CLASS}"
 WriteRegStr HKCU "Software\Classes\${WORLD_CLASS}" "" "Voltura World artwork"
 WriteRegStr HKCU "Software\Classes\${WORLD_CLASS}\DefaultIcon" "" '$\"$INSTDIR\VolturaWorlds.exe$\",0'
 WriteRegStr HKCU "Software\Classes\${WORLD_CLASS}\shell\open\command" "" '$\"$INSTDIR\VolturaWorlds.exe$\" --import $\"%1$\"'
 ${If} $StartupEnabled == ${BST_CHECKED}
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_KEY}" '$\"$INSTDIR\VolturaWorlds.exe$\" --wallpaper'
 ${Else}
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_KEY}"
 ${EndIf}
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayName" "Voltura Worlds"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayVersion" "0.1.6"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "Publisher" "Voltura AB"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "InstallLocation" "$INSTDIR"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayIcon" "$INSTDIR\VolturaWorlds.exe"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "NoModify" 1
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "NoRepair" 1
SectionEnd
Function un.onInit
 SetRegView 64
 FindWindow $0 "VolturaWorldController"
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONINFORMATION "Please exit Voltura Worlds from the notification area before uninstalling." /SD IDOK
  Abort
 ${EndIf}
FunctionEnd
Section "Uninstall"
 SetShellVarContext current
 Delete "$INSTDIR\VolturaWorlds.exe"
 Delete "$INSTDIR\LICENSE"
 Delete "$INSTDIR\THIRD-PARTY-NOTICES.md"
 Delete "$INSTDIR\licenses\Direct3D12-Agility.txt"
 Delete "$INSTDIR\licenses\Direct3D12-Agility-Code.txt"
 Delete "$INSTDIR\licenses\Direct3D12-Redistributables.txt"
 Delete "$INSTDIR\licenses\nlohmann-json.txt"
 RMDir "$INSTDIR\licenses"
 Delete "$INSTDIR\assets\orbit\earth-january.jpg"
 Delete "$INSTDIR\assets\orbit\earth-clouds-8192.tif"
 Delete "$INSTDIR\assets\orbit\earth-night-2016.jpg"
 Delete "$INSTDIR\assets\orbit\milkyway-2020.png"
 Delete "$INSTDIR\assets\orbit\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\orbit\runtime\earth-january.dds"
 Delete "$INSTDIR\assets\orbit\runtime\earth-clouds-8192.dds"
 Delete "$INSTDIR\assets\orbit\runtime\milkyway-2020.dds"
 RMDir "$INSTDIR\assets\orbit\runtime"
 RMDir "$INSTDIR\assets\orbit"
 Delete "$INSTDIR\assets\aurora\teal-silk-v1.png"
 Delete "$INSTDIR\assets\aurora\violet-silk-v1.png"
 Delete "$INSTDIR\assets\aurora\PROVENANCE.md"
 Delete "$INSTDIR\assets\aurora\approved-clean-v1.png"
 RMDir "$INSTDIR\assets\aurora"
 RMDir "$INSTDIR\assets"
 Delete "$INSTDIR\shaders\*.cso"
 RMDir "$INSTDIR\shaders"
 Delete "$INSTDIR\D3D12\D3D12Core.dll"
 RMDir "$INSTDIR\D3D12"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\${SHORTCUT_NAME}.lnk"
 DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_KEY}"
 ReadRegStr $0 HKCU "Software\Classes\${FILE_EXTENSION}" ""
 ${If} $0 == "${WORLD_CLASS}"
  DeleteRegKey HKCU "Software\Classes\${FILE_EXTENSION}"
 ${EndIf}
 DeleteRegKey HKCU "Software\Classes\${WORLD_CLASS}"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}"
 MessageBox MB_YESNO|MB_DEFBUTTON2 "Delete your downloaded Worlds, imported videos, and preferences? Choose No to keep them." /SD IDNO IDNO keepData
 RMDir /r "${PROFILE_DIR}"
 keepData:
SectionEnd

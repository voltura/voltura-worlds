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
 !define OUTPUT "..\dist\VolturaWorlds-Setup-0.1.7-win-x64.exe"
!endif
Name "Voltura Worlds"
Caption "Voltura Worlds 0.1.7 Setup"
BrandingText "Voltura AB"
ManifestDPIAware true
ManifestSupportedOS all
VIProductVersion "0.1.7.0"
VIAddVersionKey "ProductName" "Voltura Worlds"
VIAddVersionKey "CompanyName" "Voltura AB"
VIAddVersionKey "FileDescription" "Voltura Worlds Setup"
VIAddVersionKey "FileVersion" "0.1.7"
VIAddVersionKey "ProductVersion" "0.1.7"
VIAddVersionKey "OriginalFilename" "VolturaWorlds-Setup-0.1.7-win-x64.exe"
VIAddVersionKey "LegalCopyright" "Voltura AB"
!define MUI_ICON "artwork\worlds.ico"
!define MUI_UNICON "artwork\worlds.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "artwork\worlds-sidebar.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP_STRETCH "FitControl"
!define MUI_WELCOMEPAGE_TITLE "Your desktop, alive."
!define MUI_WELCOMEPAGE_TEXT "Install Voltura Worlds to bring thirteen animated Worlds to your Windows desktop.$\r$\n$\r$\nChoose moving artwork, add your own videos, and optionally let your Worlds react to audio.$\r$\n$\r$\nInstalls for your account. No administrator access required."
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
 SetOutPath "$INSTDIR\assets\earth"
 File "..\assets\earth\ATTRIBUTION.md"
 File "..\assets\earth\sources.json"
 File "..\assets\earth\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\earth\runtime"
 File "..\assets\earth\runtime\earth-color.dds"
 File "..\assets\earth\runtime\earth-material.dds"
 SetOutPath "$INSTDIR\assets\space"
 File "..\assets\space\ATTRIBUTION.md"
 File "..\assets\space\sources.json"
 File "..\assets\space\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\space\runtime"
 File "..\assets\space\runtime\planet-sky.dds"
 SetOutPath "$INSTDIR\assets\jupiter"
 File "..\assets\jupiter\ATTRIBUTION.md"
 File "..\assets\jupiter\sources.json"
 File "..\assets\jupiter\runtime-hashes.json"
 File "..\assets\jupiter\wind-profile.json"
 SetOutPath "$INSTDIR\assets\jupiter\runtime"
 File "..\assets\jupiter\runtime\jupiter-color.dds"
 File "..\assets\jupiter\runtime\jupiter-atmosphere.dds"
 SetOutPath "$INSTDIR\assets\saturn"
 File "..\assets\saturn\ATTRIBUTION.md"
 File "..\assets\saturn\sources.json"
 File "..\assets\saturn\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\saturn\runtime"
 File "..\assets\saturn\runtime\saturn-color.dds"
 SetOutPath "$INSTDIR\assets\pluto"
 File "..\assets\pluto\ATTRIBUTION.md"
 File "..\assets\pluto\sources.json"
 File "..\assets\pluto\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\pluto\runtime"
 File "..\assets\pluto\runtime\pluto-color.dds"
 SetOutPath "$INSTDIR\assets\sun"
 File "..\assets\sun\ATTRIBUTION.md"
 File "..\assets\sun\sources.json"
 File "..\assets\sun\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\sun\runtime"
 File "..\assets\sun\runtime\sun-aia171.dds"
 File "..\assets\sun\runtime\sun-prominence.dds"
 SetOutPath "$INSTDIR\assets\mercury"
 File "..\assets\mercury\ATTRIBUTION.md"
 File "..\assets\mercury\sources.json"
 File "..\assets\mercury\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\mercury\runtime"
 File "..\assets\mercury\runtime\mercury-color.dds"
 File "..\assets\mercury\runtime\mercury-normal.dds"
 SetOutPath "$INSTDIR\assets\moon"
 File "..\assets\moon\ATTRIBUTION.md"
 File "..\assets\moon\sources.json"
 File "..\assets\moon\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\moon\runtime"
 File "..\assets\moon\runtime\moon-color.dds"
 File "..\assets\moon\runtime\moon-normal.dds"
 File "..\assets\moon\runtime\moon-earth.dds"
 SetOutPath "$INSTDIR\assets\mars"
 File "..\assets\mars\ATTRIBUTION.md"
 File "..\assets\mars\sources.json"
 File "..\assets\mars\runtime-hashes.json"
 SetOutPath "$INSTDIR\assets\mars\runtime"
 File "..\assets\mars\runtime\mars-color.dds"
 File "..\assets\mars\runtime\mars-normal.dds"
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
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_KEY}" "DisplayVersion" "0.1.7"
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
 Delete "$INSTDIR\assets\space\runtime\planet-sky.dds"
 RMDir "$INSTDIR\assets\space\runtime"
 Delete "$INSTDIR\assets\space\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\space\sources.json"
 Delete "$INSTDIR\assets\space\runtime-hashes.json"
 RMDir "$INSTDIR\assets\space"
 Delete "$INSTDIR\assets\jupiter\runtime\jupiter-color.dds"
 Delete "$INSTDIR\assets\jupiter\runtime\jupiter-atmosphere.dds"
 RMDir "$INSTDIR\assets\jupiter\runtime"
 Delete "$INSTDIR\assets\jupiter\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\jupiter\sources.json"
 Delete "$INSTDIR\assets\jupiter\runtime-hashes.json"
 Delete "$INSTDIR\assets\jupiter\wind-profile.json"
 RMDir "$INSTDIR\assets\jupiter"
 Delete "$INSTDIR\assets\saturn\runtime\saturn-color.dds"
 RMDir "$INSTDIR\assets\saturn\runtime"
 Delete "$INSTDIR\assets\saturn\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\saturn\sources.json"
 Delete "$INSTDIR\assets\saturn\runtime-hashes.json"
 RMDir "$INSTDIR\assets\saturn"
 Delete "$INSTDIR\assets\pluto\runtime\pluto-color.dds"
 RMDir "$INSTDIR\assets\pluto\runtime"
 Delete "$INSTDIR\assets\pluto\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\pluto\sources.json"
 Delete "$INSTDIR\assets\pluto\runtime-hashes.json"
 RMDir "$INSTDIR\assets\pluto"
 Delete "$INSTDIR\assets\sun\runtime\sun-aia171.dds"
 Delete "$INSTDIR\assets\sun\runtime\sun-prominence.dds"
 RMDir "$INSTDIR\assets\sun\runtime"
 Delete "$INSTDIR\assets\sun\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\sun\sources.json"
 Delete "$INSTDIR\assets\sun\runtime-hashes.json"
 RMDir "$INSTDIR\assets\sun"
 Delete "$INSTDIR\assets\mercury\runtime\mercury-color.dds"
 Delete "$INSTDIR\assets\mercury\runtime\mercury-normal.dds"
 RMDir "$INSTDIR\assets\mercury\runtime"
 Delete "$INSTDIR\assets\mercury\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\mercury\sources.json"
 Delete "$INSTDIR\assets\mercury\runtime-hashes.json"
 RMDir "$INSTDIR\assets\mercury"
 Delete "$INSTDIR\assets\moon\runtime\moon-color.dds"
 Delete "$INSTDIR\assets\moon\runtime\moon-normal.dds"
 Delete "$INSTDIR\assets\moon\runtime\moon-earth.dds"
 RMDir "$INSTDIR\assets\moon\runtime"
 Delete "$INSTDIR\assets\moon\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\moon\sources.json"
 Delete "$INSTDIR\assets\moon\runtime-hashes.json"
 RMDir "$INSTDIR\assets\moon"
 Delete "$INSTDIR\assets\mars\runtime\mars-color.dds"
 Delete "$INSTDIR\assets\mars\runtime\mars-normal.dds"
 RMDir "$INSTDIR\assets\mars\runtime"
 Delete "$INSTDIR\assets\mars\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\mars\sources.json"
 Delete "$INSTDIR\assets\mars\runtime-hashes.json"
 RMDir "$INSTDIR\assets\mars"
 Delete "$INSTDIR\assets\earth\runtime\earth-color.dds"
 Delete "$INSTDIR\assets\earth\runtime\earth-material.dds"
 RMDir "$INSTDIR\assets\earth\runtime"
 Delete "$INSTDIR\assets\earth\ATTRIBUTION.md"
 Delete "$INSTDIR\assets\earth\sources.json"
 Delete "$INSTDIR\assets\earth\runtime-hashes.json"
 RMDir "$INSTDIR\assets\earth"

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

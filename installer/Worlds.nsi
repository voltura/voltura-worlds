Unicode true
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
Name "Voltura Worlds"
Caption "Voltura Worlds 0.1.3 Setup"
BrandingText "Voltura AB"
ManifestDPIAware true
ManifestSupportedOS all
VIProductVersion "0.1.3.0"
VIAddVersionKey "ProductName" "Voltura Worlds"
VIAddVersionKey "CompanyName" "Voltura AB"
VIAddVersionKey "FileDescription" "Voltura Worlds Setup"
VIAddVersionKey "FileVersion" "0.1.3"
VIAddVersionKey "LegalCopyright" "Voltura AB"
!define MUI_ICON "artwork\worlds.ico"
!define MUI_UNICON "artwork\worlds.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "artwork\worlds-sidebar.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP_STRETCH "FitControl"
!define MUI_WELCOMEPAGE_TITLE "Your desktop, alive."
!define MUI_WELCOMEPAGE_TEXT "Install Voltura Worlds to bring Aurora, Orbit, and Smoke to your Windows desktop.$\r$\n$\r$\nChoose moving artwork, add your own videos, and optionally let your Worlds react to audio.$\r$\n$\r$\nInstalls for your account. No administrator access required."
!define MUI_FINISHPAGE_TITLE "Your Worlds are ready"
!define MUI_FINISHPAGE_TEXT "Open Voltura Worlds to choose your artwork.$\r$\n$\r$\nUse the notification-area icon to open settings, pause animation, or exit."
OutFile "..\dist\VolturaWorlds-Setup-0.1.3-win-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\Voltura Worlds"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
SetDatablockOptimize on
Var StartupCheckbox
Var StartupEnabled
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
 StrCpy $StartupEnabled ${BST_CHECKED}
 ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "DisplayVersion"
 ${If} $0 != ""
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VolturaWorlds"
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
 File "..\build\Release\VolturaWorlds.exe"
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
 CreateShortcut "$SMPROGRAMS\Voltura Worlds.lnk" "$INSTDIR\VolturaWorlds.exe"
 WriteRegStr HKCU "Software\Classes\.world" "" "Voltura.World"
 WriteRegStr HKCU "Software\Classes\Voltura.World" "" "Voltura World artwork"
 WriteRegStr HKCU "Software\Classes\Voltura.World\DefaultIcon" "" '$\"$INSTDIR\VolturaWorlds.exe$\",0'
 WriteRegStr HKCU "Software\Classes\Voltura.World\shell\open\command" "" '$\"$INSTDIR\VolturaWorlds.exe$\" --import $\"%1$\"'
 ${If} $StartupEnabled == ${BST_CHECKED}
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VolturaWorlds" '$\"$INSTDIR\VolturaWorlds.exe$\" --wallpaper'
 ${Else}
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VolturaWorlds"
 ${EndIf}
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "DisplayName" "Voltura Worlds"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "DisplayVersion" "0.1.3"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "Publisher" "Voltura AB"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "InstallLocation" "$INSTDIR"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "DisplayIcon" "$INSTDIR\VolturaWorlds.exe"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "NoModify" 1
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds" "NoRepair" 1
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
 Delete "$SMPROGRAMS\Voltura Worlds.lnk"
 DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "VolturaWorlds"
 ReadRegStr $0 HKCU "Software\Classes\.world" ""
 ${If} $0 == "Voltura.World"
  DeleteRegKey HKCU "Software\Classes\.world"
 ${EndIf}
 DeleteRegKey HKCU "Software\Classes\Voltura.World"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds"
 MessageBox MB_YESNO|MB_DEFBUTTON2 "Delete your downloaded Worlds, imported videos, and preferences? Choose No to keep them." /SD IDNO IDNO keepData
 RMDir /r "$LOCALAPPDATA\Voltura\Worlds"
 keepData:
SectionEnd

param([string]$Installer = (Join-Path (Split-Path $PSScriptRoot) 'dist/VolturaWorlds-Setup-0.1.7-win-x64.exe'), [string]$UpgradeFrom = '', [string]$UpdateBaseline = '', [ValidatePattern('^VolturaWorlds-Test-[a-f0-9]{32}$')][string]$TestIdentity = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$reportDir = Join-Path $repo 'artifacts/installer-lifecycle'
$installDir = Join-Path $env:LOCALAPPDATA 'Programs/Voltura Worlds Validation'
$productKey = if ($TestIdentity) { $TestIdentity } else { 'VolturaWorlds' }
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\' + $productKey
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$shortcut = Join-Path $env:APPDATA 'Microsoft/Windows/Start Menu/Programs/Voltura Worlds.lnk'
$profileDir = Join-Path $env:LOCALAPPDATA 'Voltura/Worlds'
$worldClass = 'Voltura.World'; $worldExtension = '.world'
if ($TestIdentity) {
 $testId = $TestIdentity.Substring('VolturaWorlds-Test-'.Length)
 $installDir = Join-Path $env:LOCALAPPDATA "Programs/Voltura Worlds Validation-$testId"
 $shortcut = Join-Path $env:APPDATA "Microsoft/Windows/Start Menu/Programs/Voltura Worlds Test $testId.lnk"
 $profileDir = Join-Path $env:LOCALAPPDATA "Voltura/Worlds-InstallerTests/$testId"
 $worldClass = "Voltura.World.Test.$testId"; $worldExtension = ".world-test$testId"
}
$extensionKey = 'HKCU:\Software\Classes\' + $worldExtension
$classKey = 'HKCU:\Software\Classes\' + $worldClass
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$initialInstaller = if ($UpgradeFrom) { (Resolve-Path -LiteralPath $UpgradeFrom).Path } else { $installerPath }

# Never replace an existing installation or another application's association.
foreach ($path in @($installDir, $uninstallKey, $shortcut, $extensionKey, $classKey)) {
 if (Test-Path -LiteralPath $path) { throw "Validation requires an unused installation: $path" }
}
if ((Get-ItemProperty -LiteralPath $runKey).PSObject.Properties.Name -contains $productKey) { throw 'Existing Worlds startup entry; validation stopped.' }
if (Get-Process VolturaWorlds -ErrorAction SilentlyContinue) { throw 'Exit Worlds before installer validation.' }
$isElevated = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isElevated) { throw 'Run this check from a non-elevated session.' }

New-Item -ItemType Directory -Force $reportDir | Out-Null
$profileExisted = Test-Path -LiteralPath $profileDir
$before = @{}
if ($profileExisted) {
 Get-ChildItem -LiteralPath $profileDir -File -Recurse | ForEach-Object { $before[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash }
}
New-Item -ItemType Directory -Force $profileDir | Out-Null
$sentinel = Join-Path $profileDir ('installer-validation-' + [guid]::NewGuid().ToString('N') + '.txt')
'Retain user library on silent uninstall.' | Set-Content -LiteralPath $sentinel
$results = [ordered]@{ elevated = $isElevated; installer = $installerPath; sha256 = (Get-FileHash -LiteralPath $installerPath).Hash }
$results.initialInstallerSha256 = (Get-FileHash -LiteralPath $initialInstaller).Hash
function Run-Setup([string]$Arguments, [string]$SetupPath = $installerPath) {
 $process = Start-Process -FilePath $SetupPath -ArgumentList $Arguments -WindowStyle Hidden -Wait -PassThru
 if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
}
try {
 Run-Setup "/S /D=$installDir" $initialInstaller
 if ((Get-ItemPropertyValue -LiteralPath $uninstallKey -Name InstallLocation) -ne $installDir) { throw 'Install location was not registered.' }
 $expectedStartup = '"' + (Join-Path $installDir 'VolturaWorlds.exe') + '" --wallpaper'
 if ((Get-ItemPropertyValue -LiteralPath $runKey -Name $productKey) -ne $expectedStartup) { throw 'Default startup registration is incorrect.' }
 if (!(Test-Path -LiteralPath $shortcut)) { throw 'Start-menu entry is missing.' }
 if ((Get-Item -LiteralPath $extensionKey).GetValue('') -ne $worldClass) { throw 'World association missing.' }
 $results.install = 'passed'
 if ($UpdateBaseline) {
  $preferences = Join-Path $profileDir 'preferences.ini'
  $preferencesExisted = Test-Path -LiteralPath $preferences
  $preferencesBefore = if ($preferencesExisted) { [IO.File]::ReadAllBytes($preferences) } else { $null }
  $updateTarget = Join-Path $installDir 'VolturaWorlds.exe'
  $expectedUpdateHash = (Get-FileHash -LiteralPath $updateTarget).Hash
  $baseline = (Resolve-Path -LiteralPath $UpdateBaseline).Path
  $oldUpdateProfile = $env:WORLDS_UPDATES_TEST_PROFILE
  $oldUpdateKey = $env:WORLDS_UPDATES_TEST_PRODUCT_KEY
  if ($TestIdentity) { $env:WORLDS_UPDATES_TEST_PROFILE = $preferences; $env:WORLDS_UPDATES_TEST_PRODUCT_KEY = $TestIdentity }
  try {
   "[worlds]`nautomaticUpdates=1" | Set-Content -LiteralPath $preferences -Encoding unicode
   Copy-Item -LiteralPath $baseline -Destination $updateTarget -Force
   $updateHelper = [string](& $updateTarget --prepare-helper $updateTarget $installerPath setup)
   if ($LASTEXITCODE) { throw 'Cannot stage signed installer update fixture.' }
   $helperProcess = Start-Process -FilePath $updateHelper -ArgumentList @('--apply-update', ('"' + $updateTarget + '"'), '4294967294') -WindowStyle Hidden -PassThru
   if (!$helperProcess.WaitForExit(60000)) { $helperProcess.Kill(); throw 'Automatic installer helper timed out.' }
   if ($helperProcess.ExitCode -ne 0) { Get-Content (Join-Path (Split-Path (Split-Path $updateHelper)) 'updates.log'); throw 'Automatic installer helper failed.' }
   if ((Get-FileHash -LiteralPath $updateTarget).Hash -ne $expectedUpdateHash) { throw 'Automatic update did not install the exact signed application.' }
   $results.automaticUpdate = 'passed; signed 0.1.3 baseline to signed 0.1.7 through the real NSIS installer and helper'
  } finally {
   if ($updateHelper) {
    $ownedCache = [IO.Path]::GetFullPath((Split-Path (Split-Path $updateHelper)))
    $allowedParent = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Voltura/Worlds/Updates'))
    if ((Split-Path $ownedCache -Parent) -eq $allowedParent -and (Split-Path $ownedCache -Leaf) -match '^\d+$') {
     foreach ($part in Get-ChildItem -LiteralPath $ownedCache -Directory) {
      foreach ($name in @('payload.exe','helper.exe','download.part')) {
       $ownedFile = Join-Path $part.FullName $name
       if (Test-Path -LiteralPath $ownedFile) { Remove-Item -LiteralPath $ownedFile }
      }
      if (!(Get-ChildItem -LiteralPath $part.FullName -Force)) { Remove-Item -LiteralPath $part.FullName }
     }
     foreach ($name in @('pending','pending.tmp','updates.log')) {
      $ownedFile = Join-Path $ownedCache $name
      if (Test-Path -LiteralPath $ownedFile) { Remove-Item -LiteralPath $ownedFile }
     }
     if (!(Get-ChildItem -LiteralPath $ownedCache -Force)) { Remove-Item -LiteralPath $ownedCache }
    }
   }
   $env:WORLDS_UPDATES_TEST_PROFILE = $oldUpdateProfile
   $env:WORLDS_UPDATES_TEST_PRODUCT_KEY = $oldUpdateKey
   if ($preferencesExisted) { [IO.File]::WriteAllBytes($preferences, $preferencesBefore) }
   elseif (Test-Path -LiteralPath $preferences) { Remove-Item -LiteralPath $preferences }
  }
 }


 # An explicit opt-out must survive the next ordinary upgrade. Both upgrades
 # use the registered directory, without another /D argument.
 Run-Setup '/S /NOSTARTUP'
 Run-Setup '/S'
 if ((Get-ItemProperty -LiteralPath $runKey).PSObject.Properties.Name -contains $productKey) { throw 'Upgrade re-enabled startup.' }
 if ((Get-ItemPropertyValue -LiteralPath $uninstallKey -Name InstallLocation) -ne $installDir) { throw 'Upgrade changed the install directory.' }
 $results.upgrades = 'passed; startup opt-out and directory retained'
 foreach ($obsolete in @('earth-january.jpg','earth-clouds-8192.tif','milkyway-2020.png')) {
  if (Test-Path -LiteralPath (Join-Path $installDir "assets/orbit/$obsolete")) { throw "Upgrade retained obsolete texture: $obsolete" }
 }
 foreach ($texture in @('earth-january.dds','earth-clouds-8192.dds','milkyway-2020.dds')) {
  if (!(Test-Path -LiteralPath (Join-Path $installDir "assets/orbit/runtime/$texture"))) { throw "Upgrade omitted texture: $texture" }
 }
 $results.textureMigration = 'passed; compressed assets installed and obsolete texture copies removed'

 & python (Join-Path $PSScriptRoot 'test_desktop.py') -- (Join-Path $installDir 'VolturaWorlds.exe') --capture-dir (Join-Path $reportDir 'installed-runtime') --scene orbit --width 1280 --height 720 --frames 60 --grid 64 --validate-lifecycle
 if ($LASTEXITCODE -ne 0) { throw 'Installed renderer validation failed.' }
 $results.installedRuntime = 'passed; native private-desktop scene/resize lifecycle'
 foreach ($scene in @('silk','eclipse','lattice','drift','contour','mercury','ripples')) {
  & python (Join-Path $PSScriptRoot 'test_desktop.py') -- (Join-Path $installDir 'VolturaWorlds.exe') --capture-dir (Join-Path $reportDir "installed-$scene") --scene $scene --width 640 --height 360 --frames 1 --start-time 8 --debug-gpu
  if ($LASTEXITCODE -ne 0) { throw "Installed $scene renderer validation failed." }
 }
 $results.installedNewWorlds = 'passed; all seven procedural Worlds rendered using packaged shaders with D3D12 debug validation'
 & python (Join-Path $PSScriptRoot 'test_desktop.py') -- (Join-Path $installDir 'VolturaWorlds.exe') --capture-dir (Join-Path $reportDir 'installed-video') --scene video --video-file (Join-Path $repo 'tests/fixtures/h264.mp4') --width 800 --height 800 --frames 120 --fit
 if ($LASTEXITCODE -ne 0) { throw 'Installed video renderer validation failed.' }
 $results.installedVideo = 'passed; native private-desktop video playback with packaged shader'
 & python (Join-Path $PSScriptRoot 'test_desktop.py') -- (Join-Path $repo 'build/Release/worlds_desktop_tests.exe') (Join-Path $installDir 'VolturaWorlds.exe')
 if ($LASTEXITCODE -ne 0) { throw 'Installed desktop host recovery validation failed.' }
 $results.hostRecovery = 'passed; installed application delayed-host attach, host destruction/recovery and clean exit on a private synthetic desktop'
} finally {
 $uninstaller = Join-Path $installDir 'Uninstall.exe'
 # Fixed, preflight-checked path beneath the current user's Programs folder.
 if (Test-Path -LiteralPath $uninstaller) {
  $process = Start-Process -FilePath $uninstaller -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
  if ($process.ExitCode -ne 0) { throw "Uninstaller failed: $($process.ExitCode)" }
 }
 if (!(Test-Path -LiteralPath $sentinel)) { throw 'Uninstall deleted retained library data.' }
 foreach ($file in $before.Keys) {
  if (!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash -ne $before[$file]) { throw "User data changed: $file" }
 }
 Remove-Item -LiteralPath $sentinel
 if (!$profileExisted -and !(Get-ChildItem -LiteralPath $profileDir -Force)) { Remove-Item -LiteralPath $profileDir }
}
foreach ($path in @($installDir, $uninstallKey, $shortcut, $extensionKey, $classKey)) {
 if (Test-Path -LiteralPath $path) { throw "Uninstall left a product artifact: $path" }
}
if ((Get-ItemProperty -LiteralPath $runKey).PSObject.Properties.Name -contains $productKey) { throw 'Uninstall left startup registration.' }
$results.uninstall = 'passed; files, shortcut and registration removed; library retained'
$results.evidence = 'Non-elevated current-user install/upgrade/uninstall and private-desktop runtime. Not physical desktop interaction proof.'
$results | ConvertTo-Json | Set-Content (Join-Path $reportDir 'results.json')
$results | ConvertTo-Json

param([string]$Installer = (Join-Path (Split-Path $PSScriptRoot) 'dist/VolturaWorlds-Setup-0.1.3-win-x64.exe'), [string]$UpgradeFrom = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$reportDir = Join-Path $repo 'artifacts/installer-lifecycle'
$installDir = Join-Path $env:LOCALAPPDATA 'Programs/Voltura Worlds Validation'
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VolturaWorlds'
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$shortcut = Join-Path $env:APPDATA 'Microsoft/Windows/Start Menu/Programs/Voltura Worlds.lnk'
$profileDir = Join-Path $env:LOCALAPPDATA 'Voltura/Worlds'
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$initialInstaller = if ($UpgradeFrom) { (Resolve-Path -LiteralPath $UpgradeFrom).Path } else { $installerPath }

# Never replace an existing installation or another application's association.
foreach ($path in @($installDir, $uninstallKey, $shortcut, 'HKCU:\Software\Classes\.world', 'HKCU:\Software\Classes\Voltura.World')) {
 if (Test-Path -LiteralPath $path) { throw "Validation requires an unused installation: $path" }
}
if ((Get-ItemProperty -LiteralPath $runKey).PSObject.Properties.Name -contains 'VolturaWorlds') { throw 'Existing Worlds startup entry; validation stopped.' }
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
 if ((Get-ItemPropertyValue -LiteralPath $runKey -Name VolturaWorlds) -ne $expectedStartup) { throw 'Default startup registration is incorrect.' }
 if (!(Test-Path -LiteralPath $shortcut)) { throw 'Start-menu entry is missing.' }
 if ((Get-Item 'HKCU:\Software\Classes\.world').GetValue('') -ne 'Voltura.World') { throw 'World association missing.' }
 $results.install = 'passed'

 # An explicit opt-out must survive the next ordinary upgrade. Both upgrades
 # use the registered directory, without another /D argument.
 Run-Setup '/S /NOSTARTUP'
 Run-Setup '/S'
 if ((Get-ItemProperty -LiteralPath $runKey).PSObject.Properties.Name -contains 'VolturaWorlds') { throw 'Upgrade re-enabled startup.' }
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
foreach ($path in @($installDir, $uninstallKey, $shortcut, 'HKCU:\Software\Classes\.world', 'HKCU:\Software\Classes\Voltura.World')) {
 if (Test-Path -LiteralPath $path) { throw "Uninstall left a product artifact: $path" }
}
if ((Get-ItemProperty -LiteralPath $runKey).PSObject.Properties.Name -contains 'VolturaWorlds') { throw 'Uninstall left startup registration.' }
$results.uninstall = 'passed; files, shortcut and registration removed; library retained'
$results.evidence = 'Non-elevated current-user install/upgrade/uninstall and private-desktop runtime. Not physical desktop interaction proof.'
$results | ConvertTo-Json | Set-Content (Join-Path $reportDir 'results.json')
$results | ConvertTo-Json

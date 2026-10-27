param(
 [Parameter(Mandatory)][ValidateSet('Enable','Disable')][string]$Action,
 [Parameter(Mandatory)][string]$InstallRoot,
 [switch]$EnableCompact,
 [switch]$AllowMachineTrust,
 [switch]$Quiet
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($InstallRoot)
$shell = Join-Path $root 'shell'
$stateKey = 'HKCU:\Software\Voltura\Worlds\DesktopMenu'
$classicMenus = @(
 @{ Name='VolturaWorlds'; Clsid='{8AA13292-0832-4D9D-AB10-8637D2380E4A}' },
 @{ Name='VolturaWorldsVisualizations'; Clsid='{8AA13292-0832-4D9D-AB10-8637D2380E4E}' },
 @{ Name='VolturaWorldsSolarSystem'; Clsid='{8AA13292-0832-4D9D-AB10-8637D2380E4F}' }
)
$oldHandlerKey = 'HKCU:\Software\Classes\DesktopBackground\shellex\ContextMenuHandlers\VolturaWorlds'
$logDir = Join-Path $env:LOCALAPPDATA 'Voltura/Worlds'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir 'desktop-menu.log'
function Record([string]$message) { Add-Content -LiteralPath $log -Value "$(Get-Date -Format o) $message" }
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class WorldsShellRegistration {
 [DllImport("shell32.dll")] public static extern void SHChangeNotify(uint eventId, uint flags, IntPtr item1, IntPtr item2);
 [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern int GetPackageFullName(IntPtr process, ref uint length, StringBuilder name);
}
'@
function NotifyShell { [WorldsShellRegistration]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero) }
function StopShellSurrogates($packages) {
 # Packaged COM uses its own surrogate. Release only this user's Worlds
 # package hosts before registration changes; never close Explorer.
 $names = @($packages | ForEach-Object { $_.PackageFullName })
 $session = (Get-Process -Id $PID).SessionId
 Get-Process -Name dllhost -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq $session } | ForEach-Object {
  $process = $_
  $handle = [WorldsShellRegistration]::OpenProcess(0x1000, $false, $process.Id)
  if ($handle -eq [IntPtr]::Zero) { return }
  try {
   [uint32]$length = 0
   $result = [WorldsShellRegistration]::GetPackageFullName($handle, [ref]$length, $null)
   if ($result -ne 122 -or $length -gt 1024) { return }
   $name = [Text.StringBuilder]::new([int]$length)
   $result = [WorldsShellRegistration]::GetPackageFullName($handle, [ref]$length, $name)
   if ($result -eq 0 -and $names -contains $name.ToString()) {
    Stop-Process -Id $process.Id -Force -ErrorAction Stop
    Record "Closed Worlds shell surrogate $($process.Id) before changing its package registration."
   }
  } catch { Record "Shell surrogate release: $($_.Exception.Message)" }
  finally { [void][WorldsShellRegistration]::CloseHandle($handle) }
 }
}
function UnregisterIdentity {
 $packages = @(Get-AppxPackage -Name 'Voltura.Worlds.DesktopMenu')
 StopShellSurrogates $packages
 $packages | ForEach-Object { Remove-AppxPackage -Package $_.PackageFullName -ErrorAction Stop }
}
try {
 Remove-Item -LiteralPath $oldHandlerKey -Recurse -Force -ErrorAction SilentlyContinue
 if ($Action -eq 'Disable') {
  foreach ($menu in $classicMenus) {
   Remove-Item -LiteralPath "HKCU:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\$($menu.Name)" -Recurse -Force -ErrorAction SilentlyContinue
   Remove-Item -LiteralPath "HKCU:\Software\Classes\CLSID\$($menu.Clsid)" -Recurse -Force -ErrorAction SilentlyContinue
  }
  UnregisterIdentity
  Remove-Item -LiteralPath $stateKey -Recurse -Force -ErrorAction SilentlyContinue
  NotifyShell
  Record 'Desktop menus disabled. Public certificate trust was retained.'
  exit 0
 }
 $metadata = Get-Content -LiteralPath (Join-Path $shell 'menu.json') -Raw | ConvertFrom-Json
 if ($metadata.revision -notmatch '^[a-f0-9]{16}$') { throw 'Invalid desktop menu metadata.' }
 $dll = Join-Path $shell "$($metadata.revision)/VolturaWorldsShell.dll"
 if (!(Test-Path -LiteralPath $dll -PathType Leaf) -or !(Test-Path -LiteralPath (Join-Path $root 'VolturaWorlds.exe') -PathType Leaf)) { throw 'Desktop menu binaries are missing.' }
 foreach ($menu in $classicMenus) {
  $classKey = "HKCU:\Software\Classes\CLSID\$($menu.Clsid)"
  $handlerKey = "HKCU:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\$($menu.Name)"
  New-Item -Path "$classKey\InprocServer32" -Force | Out-Null
  Set-Item -LiteralPath "$classKey\InprocServer32" -Value $dll
  New-ItemProperty -LiteralPath "$classKey\InprocServer32" -Name ThreadingModel -Value Apartment -PropertyType String -Force | Out-Null
  New-Item -Path $handlerKey -Force | Out-Null
  Set-Item -LiteralPath $handlerKey -Value $menu.Clsid
 }
 New-Item -Path $stateKey -Force | Out-Null
 New-ItemProperty -LiteralPath $stateKey -Name Enabled -Value 1 -PropertyType DWord -Force | Out-Null
 New-ItemProperty -LiteralPath $stateKey -Name CompactRequested -Value ([int][bool]$EnableCompact) -PropertyType DWord -Force | Out-Null
 New-ItemProperty -LiteralPath $stateKey -Name CompactAvailable -Value 0 -PropertyType DWord -Force | Out-Null
 $exitCode = 0
 if (!$EnableCompact) {
  UnregisterIdentity
  Record 'Classic desktop menu registered; compact menu not requested.'
  NotifyShell
  exit 0
 }
 try {
  if (!$metadata.signed -or $metadata.thumbprint -notmatch '^[A-Fa-f0-9]{40}$') { throw 'This local unsigned build supports the classic desktop menu only.' }
  $certPath = Join-Path $shell 'publisher.cer'
  $cert = [Security.Cryptography.X509Certificates.X509Certificate2]::new($certPath)
  if ($cert.Thumbprint -ine $metadata.thumbprint) { throw 'Publisher certificate does not match the signed menu package configuration.' }
  $trustedPath = "Cert:\LocalMachine\TrustedPeople\$($cert.Thumbprint)"
  if (!(Test-Path -LiteralPath $trustedPath)) {
   if (!$AllowMachineTrust) { throw 'The compact menu needs the optional administrator certificate-trust step. Classic menu remains enabled.' }
   $helper = Join-Path $shell 'VolturaWorldsTrust.exe'
   $signature = Get-AuthenticodeSignature -LiteralPath $helper
   if (!$signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ine $cert.Thumbprint) { throw 'Certificate-trust helper publisher mismatch.' }
   & (Join-Path $shell 'verify-signature.ps1') -Path $helper -CertificateThumbprint $cert.Thumbprint -AllowSelfSigned | Out-Null
   $process = Start-Process -FilePath $helper -Verb RunAs -WindowStyle Hidden -Wait -PassThru
   if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $trustedPath)) { throw 'Certificate trust was declined or could not be completed.' }
  }
  $packages = @(Get-AppxPackage -Name 'Voltura.Worlds.DesktopMenu')
  StopShellSurrogates $packages
  $same = @($packages | Where-Object { $_.Version -eq [version]$metadata.packageVersion })
  $previousState = Get-ItemProperty -LiteralPath $stateKey
  $alreadyRegistered = $same.Count -and $previousState.InstalledRevision -eq $metadata.revision -and $previousState.InstallRoot -eq $root
  if (!$alreadyRegistered) {
   # Moving the same package between local build/install locations needs a
   # re-registration; newer package revisions use the normal update path.
   if ($same.Count) { UnregisterIdentity }
   Add-AppxPackage -Path (Join-Path $shell 'VolturaWorldsMenu.msix') -ExternalLocation $root -ForceTargetApplicationShutdown -ErrorAction Stop
  }
  New-ItemProperty -LiteralPath $stateKey -Name CompactAvailable -Value 1 -PropertyType DWord -Force | Out-Null
  New-ItemProperty -LiteralPath $stateKey -Name InstalledRevision -Value $metadata.revision -PropertyType String -Force | Out-Null
  New-ItemProperty -LiteralPath $stateKey -Name InstallRoot -Value $root -PropertyType String -Force | Out-Null
  Record 'Classic and compact desktop menus registered for the current user.'
 } catch {
  Record "Compact menu unavailable: $($_.Exception.Message)"
  if (!$Quiet) { Write-Warning "Classic desktop menu enabled. Windows 11 compact menu could not be enabled: $($_.Exception.Message). Details: $log" }
  $exitCode = 2
 }
 # Remove only unlocked old revision directories inside this installation.
 $resolvedShell = [IO.Path]::GetFullPath($shell) + [IO.Path]::DirectorySeparatorChar
 Get-ChildItem -LiteralPath $shell -Directory | Where-Object { $_.Name -match '^[a-f0-9]{16}$' -and $_.Name -ne $metadata.revision } | ForEach-Object {
  $target = [IO.Path]::GetFullPath($_.FullName)
  if ($target.StartsWith($resolvedShell, [StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue }
 }
 NotifyShell
 exit $exitCode
} catch { Record $_.Exception.Message; if (!$Quiet) { Write-Error $_ }; exit 1 }

param([string]$SigningConfig = "")
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$config = $null; $cert = $null
if ($SigningConfig) {
 $config = Get-Content -LiteralPath $SigningConfig -Raw | ConvertFrom-Json
 $cert = Get-Item -LiteralPath "Cert:\CurrentUser\My\$($config.certificateThumbprint)"
}
$sdk = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Directory |
 Where-Object { $_.Name -match '^10\.' -and (Test-Path (Join-Path $_.FullName 'x64/makeappx.exe')) } |
 Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$sdk) { throw 'Windows SDK MakeAppx is required.' }
$output = Join-Path $repo 'build/Release/shell'
New-Item -ItemType Directory -Force $output | Out-Null
$dll = Join-Path $repo 'build/Release/VolturaWorldsShell.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build VolturaWorldsShell first.' }
$signature = Get-AuthenticodeSignature -LiteralPath $dll
if ($config) {
if ($signature.Status -eq 'NotSigned') {
 & "$PSScriptRoot/sign.ps1" -Path $dll -CertificateThumbprint $config.certificateThumbprint -TimestampUrl $config.timestampUrl -AllowSelfSigned:([bool]$config.allowSelfSigned)
} else {
 & "$PSScriptRoot/verify-signature.ps1" -Path $dll -CertificateThumbprint $config.certificateThumbprint -AllowSelfSigned:([bool]$config.allowSelfSigned)
}
}
# The digest is a local DLL revision name, not a release checksum asset.
$revision = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.Substring(0,16).ToLowerInvariant()
$versionText = (Select-String -LiteralPath "$repo/CMakeLists.txt" -Pattern 'project\(VolturaWorlds VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$version = [version]$versionText
if ($version.Revision -lt 0) { $version = [version]"$versionText.0" }
# Keep local same-release DLL iterations upgradeable without uninstalling an
# identity currently used by a COM surrogate. Published app versions still
# advance the first three components in the normal release process.
$previousPath = Join-Path $output 'menu.json'
if (Test-Path -LiteralPath $previousPath) {
 $previous = Get-Content -LiteralPath $previousPath -Raw | ConvertFrom-Json
 $previousVersion = [version]$previous.packageVersion
 if ($previousVersion.Major -eq $version.Major -and $previousVersion.Minor -eq $version.Minor -and $previousVersion.Build -eq $version.Build) {
  $version = $previousVersion
  if ($previous.revision -ne $revision) {
   if ($version.Revision -ge 65535) { throw 'Advance the app version before creating another shell package revision.' }
   $version = [version]::new($version.Major, $version.Minor, $version.Build, $version.Revision + 1)
  }
 }
}
$publisher = if ($cert) { $cert.Subject } else { "CN=Voltura AB, O=Voltura AB, C=SE" }
$stage = Join-Path $repo 'build/shell-package'
New-Item -ItemType Directory -Force $stage | Out-Null
$manifest = (Get-Content -LiteralPath "$repo/installer/shell/AppxManifest.xml" -Raw).
 Replace('@PUBLISHER@', [Security.SecurityElement]::Escape($publisher)).Replace('@VERSION@', $version.ToString()).Replace('@REVISION@', $revision)
[IO.File]::WriteAllText((Join-Path $stage 'AppxManifest.xml'), $manifest, [Text.UTF8Encoding]::new($false))
$versionDir = Join-Path $output $revision
New-Item -ItemType Directory -Force $versionDir | Out-Null
Copy-Item -LiteralPath $dll -Destination $versionDir -Force
Add-Type -AssemblyName System.Drawing
$icon = [Drawing.Icon]::new("$repo/installer/artwork/worlds.ico", 256, 256)
try { $bitmap = $icon.ToBitmap(); try { $bitmap.Save((Join-Path $output 'logo.png'), [Drawing.Imaging.ImageFormat]::Png) } finally { $bitmap.Dispose() } } finally { $icon.Dispose() }
if ($cert) { Export-Certificate -Cert $cert -FilePath (Join-Path $output 'publisher.cer') -Force | Out-Null }
$package = Join-Path $output 'VolturaWorldsMenu.msix'
& (Join-Path $sdk.FullName 'x64/makeappx.exe') pack /o /nv /d $stage /p $package | Out-Null
if ($LASTEXITCODE) { throw 'Shell identity packaging failed.' }
if ($config) {
& (Join-Path $sdk.FullName 'x64/signtool.exe') sign /s My /sha1 $cert.Thumbprint /fd SHA256 /tr $config.timestampUrl /td SHA256 $package | Out-Null
if ($LASTEXITCODE) { throw 'Shell identity signing failed.' }
& "$PSScriptRoot/verify-signature.ps1" -Path $package -CertificateThumbprint $cert.Thumbprint -AllowSelfSigned:([bool]$config.allowSelfSigned) | Out-Null
}
@{ revision=$revision; thumbprint=$(if ($cert) { $cert.Thumbprint } else { '' }); packageVersion=$version.ToString(); signed=[bool]$cert } | ConvertTo-Json | Set-Content (Join-Path $output 'menu.json')
$canTrust = if ($cert) { 1 } else { 0 }
[IO.File]::WriteAllText((Join-Path $output 'shell-files.nsh'), "!define SHELL_REVISION `"$revision`"`r`n!define SHELL_CAN_TRUST $canTrust`r`n")
Copy-Item -LiteralPath "$repo/scripts/register-shell.ps1" -Destination $output -Force
Copy-Item -LiteralPath "$repo/scripts/verify-signature.ps1" -Destination $output -Force
$helper = Join-Path $repo 'build/Release/VolturaWorldsTrust.exe'
if ($config) {
if ((Get-AuthenticodeSignature -LiteralPath $helper).Status -eq 'NotSigned') {
 & "$PSScriptRoot/sign.ps1" -Path $helper -CertificateThumbprint $config.certificateThumbprint -TimestampUrl $config.timestampUrl -AllowSelfSigned:([bool]$config.allowSelfSigned)
} else {
 & "$PSScriptRoot/verify-signature.ps1" -Path $helper -CertificateThumbprint $config.certificateThumbprint -AllowSelfSigned:([bool]$config.allowSelfSigned)
}
}
Copy-Item -LiteralPath $helper -Destination $output -Force
Write-Host "Desktop menu package prepared ($revision; signed=$([bool]$cert))."

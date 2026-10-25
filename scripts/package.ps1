param([switch]$SkipBuild, [string]$SigningConfig = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') }
$binary = Join-Path $repo 'build/Release/VolturaWorlds.exe'
if ($SigningConfig) {
 $signing = Get-Content -LiteralPath $SigningConfig -Raw | ConvertFrom-Json
 & (Join-Path $PSScriptRoot 'sign.ps1') -Path $binary -CertificateThumbprint $signing.certificateThumbprint -TimestampUrl $signing.timestampUrl -AllowSelfSigned:([bool]$signing.allowSelfSigned)
}
New-Item -ItemType Directory -Force (Join-Path $repo 'dist') | Out-Null
Push-Location (Join-Path $repo 'installer')
try {
 & 'C:\Program Files (x86)\NSIS\makensis.exe' /V2 Worlds.nsi
 if ($LASTEXITCODE) { throw 'Installer build failed' }
} finally { Pop-Location }
$installer = Join-Path $repo 'dist/VolturaWorlds-Setup-0.1.3-win-x64.exe'
if ($SigningConfig) {
 & (Join-Path $PSScriptRoot 'sign.ps1') -Path $installer -CertificateThumbprint $signing.certificateThumbprint -TimestampUrl $signing.timestampUrl -AllowSelfSigned:([bool]$signing.allowSelfSigned)
}
$signature = Get-AuthenticodeSignature -LiteralPath $installer
@{ version='0.1.3'; installer=(Split-Path $installer -Leaf); sha256=(Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $installer).Length; signatureStatus=[string]$signature.Status; signer=[string]$signature.SignerCertificate.Subject; timestamped=[bool]$signature.TimeStamperCertificate; installerTesting='Reserved for user; installer not executed by package script' } | ConvertTo-Json | Set-Content (Join-Path $repo 'dist/preview-manifest.json')
Write-Host "Preview installer prepared: $installer"

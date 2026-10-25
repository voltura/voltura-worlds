param([Parameter(Mandatory)][string]$SigningConfig)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$identity = [guid]::NewGuid().ToString('N')
$productKey = "VolturaWorlds-Test-$identity"
$setup = Join-Path $repo "artifacts/VolturaWorlds-Setup-0.1.7-test-$identity.exe"
$signing = Get-Content -LiteralPath $SigningConfig -Raw | ConvertFrom-Json
& 'C:\Program Files (x86)\NSIS\makensis.exe' /V2 "/DPRODUCT_KEY=$productKey" "/DSHORTCUT_NAME=Voltura Worlds Test $identity" "/DWORLD_CLASS=Voltura.World.Test.$identity" "/DFILE_EXTENSION=.world-test$identity" ('/DPROFILE_DIR=$LOCALAPPDATA\Voltura\Worlds-InstallerTests\' + $identity) "/DOUTPUT=$setup" "$repo/installer/Worlds.nsi"
if ($LASTEXITCODE) { throw 'Isolated installer compilation failed.' }
& "$PSScriptRoot/sign.ps1" -Path $setup -CertificateThumbprint $signing.certificateThumbprint -TimestampUrl $signing.timestampUrl -AllowSelfSigned:([bool]$signing.allowSelfSigned)
& python "$PSScriptRoot/test_desktop.py" -- pwsh -NoProfile -File "$PSScriptRoot/test-installer.ps1" -Installer $setup -UpdateBaseline "$repo/build/Release/worlds_update_baseline.exe" -TestIdentity $productKey
if ($LASTEXITCODE) { throw 'Isolated installer and automatic upgrade checks failed.' }

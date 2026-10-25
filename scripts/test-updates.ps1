param([Parameter(Mandatory)][string]$SigningConfig)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$signing = Get-Content -LiteralPath $SigningConfig -Raw | ConvertFrom-Json
foreach ($name in @('worlds_update_tests.exe', 'worlds_update_fixture.exe', 'worlds_update_baseline.exe')) {
 $file = Join-Path $repo "build/Release/$name"
 if ((Get-AuthenticodeSignature -LiteralPath $file).Status -eq 'NotSigned') {
  & "$PSScriptRoot/sign.ps1" -Path $file -CertificateThumbprint $signing.certificateThumbprint -TimestampUrl $signing.timestampUrl -AllowSelfSigned:([bool]$signing.allowSelfSigned)
 } else {
  & "$PSScriptRoot/verify-signature.ps1" -Path $file -CertificateThumbprint $signing.certificateThumbprint -AllowSelfSigned:([bool]$signing.allowSelfSigned)
 }
}
& python "$PSScriptRoot/test_desktop.py" -- "$repo/build/Release/worlds_update_tests.exe" --signed-fixture "$repo/build/Release/worlds_update_fixture.exe"
if ($LASTEXITCODE) { throw 'Signed updater staging and cancellation checks failed.' }

# Verify the production signature reader rejects modified signed files, even when
# the downloaded checksum would match the modified bytes.
$tampered = Join-Path $repo 'artifacts/update-tampered.exe'
Copy-Item -LiteralPath "$repo/build/Release/worlds_update_fixture.exe" -Destination $tampered -Force
$bytes = [IO.File]::ReadAllBytes($tampered)
$bytes[512] = $bytes[512] -bxor 1
[IO.File]::WriteAllBytes($tampered, $bytes)
& "$repo/build/Release/worlds_update_tests.exe" --verify-signature $tampered
if ($LASTEXITCODE -eq 0) { throw 'Modified signed payload was accepted.' }
Write-Host 'PASS: modified signed executable rejected.'
exit 0

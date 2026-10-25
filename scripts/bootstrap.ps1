$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$deps = Join-Path $repo '.deps'
New-Item -ItemType Directory -Force $deps, (Join-Path $deps 'json') | Out-Null
$sdk = Join-Path $deps 'agility'
if (!(Test-Path (Join-Path $sdk 'build/native/include/d3d12.h'))) {
    $archive = Join-Path $deps 'agility.zip'
    Invoke-WebRequest 'https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.d3d12/1.619.6/microsoft.direct3d.d3d12.1.619.6.nupkg' -OutFile $archive
    if ((Get-FileHash $archive).Hash -ne '08F0489281401AA430FC37322D6C3FC98A8025175AACD714C10D562F4963F1E9') { throw 'Agility SDK checksum mismatch' }
    Expand-Archive -LiteralPath $archive -DestinationPath $sdk -Force
}
$json = Join-Path $deps 'json/json.hpp'
if (!(Test-Path $json)) {
    Invoke-WebRequest 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp' -OutFile $json
}
if ((Get-FileHash $json).Hash -ne 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63') { throw 'JSON dependency checksum mismatch' }
Write-Host 'Dependencies: Direct3D Agility SDK 1.619.6; nlohmann/json 3.12.0.'

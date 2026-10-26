$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$deps = Join-Path $repo '.deps'
New-Item -ItemType Directory -Force $deps, (Join-Path $deps 'json') | Out-Null
function Get-VerifiedDependency([string]$Url, [string]$Destination, [string]$Hash) {
    $pending = "$Destination.$([guid]::NewGuid().ToString('N')).pending"
    try {
        Invoke-WebRequest $Url -OutFile $pending
        if ((Get-FileHash -LiteralPath $pending).Hash -ne $Hash) { throw 'Dependency checksum mismatch' }
        Move-Item -LiteralPath $pending -Destination $Destination -Force
    } finally {
        if (Test-Path -LiteralPath $pending) { Remove-Item -LiteralPath $pending -Force }
    }
}
$sdk = Join-Path $deps 'agility'
$sdkFiles = @('build/native/include/d3d12.h', 'build/native/include/d3d12sdklayers.h',
    'build/native/bin/x64/D3D12Core.dll', 'build/native/bin/x64/d3d12SDKLayers.dll')
if (@($sdkFiles | Where-Object { !(Test-Path -LiteralPath (Join-Path $sdk $_) -PathType Leaf) }).Count) {
    $archive = Join-Path $deps 'agility.zip'
    Get-VerifiedDependency 'https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.d3d12/1.619.6/microsoft.direct3d.d3d12.1.619.6.nupkg' $archive '08F0489281401AA430FC37322D6C3FC98A8025175AACD714C10D562F4963F1E9'
    Expand-Archive -LiteralPath $archive -DestinationPath $sdk -Force
}
$json = Join-Path $deps 'json/json.hpp'
$jsonHash = 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63'
if (!(Test-Path -LiteralPath $json -PathType Leaf) -or (Get-FileHash -LiteralPath $json).Hash -ne $jsonHash) {
    Get-VerifiedDependency 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp' $json $jsonHash
}
Write-Host 'Dependencies: Direct3D Agility SDK 1.619.6; nlohmann/json 3.12.0.'

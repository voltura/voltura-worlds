$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $repo '.deps/space'
New-Item -ItemType Directory -Force $cache | Out-Null
$source = Join-Path $cache 'hiptyc_2020_4k_gal.exr'
if (!(Test-Path -LiteralPath $source)) {
 Invoke-WebRequest 'https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/hiptyc_2020_4k_gal.exr' -OutFile "$source.part"
 Move-Item -LiteralPath "$source.part" -Destination $source
}
$tool = Join-Path $repo '.deps/directxtex/texconv.exe'
if (!(Test-Path -LiteralPath $tool)) {
 New-Item -ItemType Directory -Force (Split-Path $tool) | Out-Null
 Invoke-WebRequest 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe' -OutFile $tool
}
if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06') { throw 'Unexpected DirectXTex executable hash' }
& python (Join-Path $PSScriptRoot 'build-space-textures.py')
if ($LASTEXITCODE) { throw 'Sky source conversion failed' }
$output = Join-Path $repo 'assets/space/runtime'
New-Item -ItemType Directory -Force $output | Out-Null
& $tool -nologo -y -dx10 -f BC7_UNORM_SRGB -srgb -sepalpha -if FANT -m 0 -o $output (Join-Path $cache 'planet-sky.png')
if ($LASTEXITCODE) { throw 'Sky compression failed' }
Get-ChildItem -LiteralPath $output -Filter '*.dds' | Get-FileHash -Algorithm SHA256 | Select-Object Hash,@{Name='File';Expression={Split-Path $_.Path -Leaf}} | ConvertTo-Json | Set-Content (Join-Path $repo 'assets/space/runtime-hashes.json')

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$toolDir = Join-Path $repo '.deps/directxtex'
$tool = Join-Path $toolDir 'texconv.exe'
$expectedTool = 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'
New-Item -ItemType Directory -Force $toolDir | Out-Null
if (!(Test-Path -LiteralPath $tool)) {
 Invoke-WebRequest 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe' -OutFile $tool
}
if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne $expectedTool) { throw 'Unexpected DirectXTex executable hash' }
$assets = Join-Path $repo 'assets/orbit'
$output = Join-Path $assets 'runtime'
New-Item -ItemType Directory -Force $output | Out-Null
# Color mip filtering is in linear light. Cloud coverage remains linear data.
& $tool -nologo -y -dx10 -f BC7_UNORM_SRGB -srgb -if FANT -w 4096 -h 2048 -m 0 -o $output (Join-Path $assets 'earth-january.jpg') (Join-Path $assets 'milkyway-2020.png')
if ($LASTEXITCODE) { throw 'Orbit color texture conversion failed' }
& $tool -nologo -y -dx10 -f BC4_UNORM -if FANT -m 0 -o $output (Join-Path $assets 'earth-clouds-8192.tif')
if ($LASTEXITCODE) { throw 'Orbit cloud texture conversion failed' }
Get-ChildItem -LiteralPath $output -Filter '*.dds' | Get-FileHash -Algorithm SHA256

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $repo '.deps/directxtex/texconv.exe'
$expectedTool = 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'
if (!(Test-Path -LiteralPath $tool)) {
 New-Item -ItemType Directory -Force (Split-Path $tool) | Out-Null
 Invoke-WebRequest 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe' -OutFile $tool
}
if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne $expectedTool) { throw 'Unexpected DirectXTex executable hash' }
& python (Join-Path $PSScriptRoot 'build-mercury-textures.py')
if ($LASTEXITCODE) { throw 'Mercury source conversion failed' }
$output = Join-Path $repo 'assets/mercury/runtime'
New-Item -ItemType Directory -Force $output | Out-Null
& $tool -nologo -y -dx10 -f BC7_UNORM_SRGB -srgb -if FANT -m 0 -o $output (Join-Path $repo '.deps/mercury/mercury-color.png')
if ($LASTEXITCODE) { throw 'Mercury color compression failed' }
& $tool -nologo -y -dx10 -f BC7_UNORM -if FANT -m 0 -o $output (Join-Path $repo '.deps/mercury/mercury-normal.png')
if ($LASTEXITCODE) { throw 'Mercury normal compression failed' }
Get-ChildItem -LiteralPath $output -Filter '*.dds' | Get-FileHash -Algorithm SHA256 | Select-Object Hash,@{Name='File';Expression={Split-Path $_.Path -Leaf}} | ConvertTo-Json | Set-Content (Join-Path $repo 'assets/mercury/runtime-hashes.json')

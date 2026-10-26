$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $repo '.deps/directxtex/texconv.exe'
$expectedTool = 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'
if (!(Test-Path -LiteralPath $tool)) {
 New-Item -ItemType Directory -Force (Split-Path $tool) | Out-Null
 Invoke-WebRequest 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe' -OutFile $tool
}
if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne $expectedTool) { throw 'Unexpected DirectXTex executable hash' }
& python (Join-Path $PSScriptRoot 'build-pluto-textures.py')
if ($LASTEXITCODE) { throw 'Pluto source conversion failed' }
$output = Join-Path $repo 'assets/pluto/runtime'
New-Item -ItemType Directory -Force $output | Out-Null
& $tool -nologo -y -dx10 -f BC7_UNORM_SRGB -srgb -if FANT -m 0 -o $output (Join-Path $repo '.deps/pluto/pluto-color.png')
if ($LASTEXITCODE) { throw 'Pluto color compression failed' }
Get-ChildItem -LiteralPath $output -Filter '*.dds' | Get-FileHash -Algorithm SHA256 | Select-Object Hash,@{Name='File';Expression={Split-Path $_.Path -Leaf}} | ConvertTo-Json | Set-Content (Join-Path $repo 'assets/pluto/runtime-hashes.json')

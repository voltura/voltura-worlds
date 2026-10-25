$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $repo '.deps/saturn'
$output = Join-Path $repo 'assets/saturn/runtime'
New-Item -ItemType Directory -Force $cache,$output | Out-Null
$source = Join-Path $cache '8k_saturn.jpg'
if (!(Test-Path -LiteralPath $source)) {
 & curl.exe -L --fail --output "$source.part" 'https://www.solarsystemscope.com/textures/download/8k_saturn.jpg'
 if ($LASTEXITCODE) { throw 'Saturn download failed' }
 Move-Item -LiteralPath "$source.part" -Destination $source
}
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne '0D39A4A490C87C3EDABE00A3881A29BB3418364178C79C534FE0986E97E09853') { throw 'Saturn source hash changed' }
$tool = Join-Path $repo '.deps/directxtex/texconv.exe'
if (!(Test-Path -LiteralPath $tool)) {
 New-Item -ItemType Directory -Force (Split-Path $tool) | Out-Null
 & curl.exe -L --fail --output $tool 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe'
 if ($LASTEXITCODE) { throw 'DirectXTex download failed' }
}
if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06') { throw 'Unexpected DirectXTex hash' }
Copy-Item -LiteralPath $source -Destination (Join-Path $cache 'saturn-color.jpg')
& $tool -nologo -y -dx10 -f BC7_UNORM_SRGB -srgb -if FANT -m 0 -o $output (Join-Path $cache 'saturn-color.jpg')
if ($LASTEXITCODE) { throw 'Saturn color compression failed' }
Get-ChildItem -LiteralPath $output -Filter '*.dds' | Get-FileHash -Algorithm SHA256 | Select-Object Hash,@{Name='File';Expression={Split-Path $_.Path -Leaf}} | ConvertTo-Json | Set-Content (Join-Path $repo 'assets/saturn/runtime-hashes.json')

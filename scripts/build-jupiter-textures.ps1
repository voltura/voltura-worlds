$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $repo '.deps/jupiter'
New-Item -ItemType Directory -Force $cache | Out-Null
$names = @(
 'hlsp_opal_hst_wfc3-uvis_jupiter-2025_all_v1_readme.txt',
 'hlsp_opal_hst_wfc3-uvis_jupiter-2025a_f395n-f502n-f631n_v1_globalmap.tif',
 'hlsp_opal_hst_wfc3-uvis_jupiter-2025a_f631n_v1_globalmap.fits',
 'hlsp_opal_hst_wfc3-uvis_jupiter-2025b_f631n_v1_globalmap.fits',
 'hlsp_opal_hst_wfc3-uvis_jupiter-2025a_fq889n_v1_globalmap.fits'
)
foreach ($name in $names) {
 $path = Join-Path $cache $name
 if (!(Test-Path -LiteralPath $path)) {
  # Windows validates the archive's certificate chain, including its intermediate.
  Invoke-WebRequest ('https://archive.stsci.edu/missions/hlsp/opal/cycle32/jupiter/' + $name) -OutFile "$path.part"
  Move-Item -LiteralPath "$path.part" -Destination $path
 }
}
$tool = Join-Path $repo '.deps/directxtex/texconv.exe'
if (!(Test-Path -LiteralPath $tool)) {
 New-Item -ItemType Directory -Force (Split-Path $tool) | Out-Null
 Invoke-WebRequest 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe' -OutFile $tool
}
if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06') { throw 'Unexpected DirectXTex executable hash' }
& python (Join-Path $PSScriptRoot 'build-jupiter-textures.py')
if ($LASTEXITCODE) { throw 'Jupiter source conversion failed' }
$output = Join-Path $repo 'assets/jupiter/runtime'
New-Item -ItemType Directory -Force $output | Out-Null
& $tool -nologo -y -dx10 -f BC7_UNORM_SRGB -srgb -if FANT -m 0 -o $output (Join-Path $cache 'jupiter-color.png')
if ($LASTEXITCODE) { throw 'Jupiter color compression failed' }
& $tool -nologo -y -dx10 -f BC5_UNORM -if FANT -m 0 -o $output (Join-Path $cache 'jupiter-atmosphere.png')
if ($LASTEXITCODE) { throw 'Jupiter atmosphere compression failed' }
Get-ChildItem -LiteralPath $output -Filter '*.dds' | Get-FileHash -Algorithm SHA256 | Select-Object Hash,@{Name='File';Expression={Split-Path $_.Path -Leaf}} | ConvertTo-Json | Set-Content (Join-Path $repo 'assets/jupiter/runtime-hashes.json')

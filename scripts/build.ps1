param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$buildTimer = [Diagnostics.Stopwatch]::StartNew()
try {
 $repo = Split-Path -Parent $PSScriptRoot
 & (Join-Path $PSScriptRoot 'bootstrap.ps1')
 $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
 $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
 if (!$vs) { throw 'Visual Studio C++ tools are required.' }
 $cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
 & $cmake -S $repo -B "$repo/build" -G 'Visual Studio 18 2026' -A x64
 if ($LASTEXITCODE) { throw 'Configure failed' }
 & $cmake --build "$repo/build" --config $Configuration --parallel
 if ($LASTEXITCODE) { throw 'Build failed' }
 Write-Host 'Build complete.'
} finally {
 $buildTimer.Stop()
 Write-Host ('Build elapsed: {0:c}' -f $buildTimer.Elapsed)
}

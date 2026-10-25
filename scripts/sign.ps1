param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$Path,
    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Fa-f0-9]{40}$')]
    [string]$CertificateThumbprint,
    [Parameter(Mandatory)]
    [uri]$TimestampUrl,
    [switch]$ValidateOnly,
    [switch]$AllowSelfSigned
)

$ErrorActionPreference = 'Stop'
if ($TimestampUrl.Scheme -notin @('https', 'http') -or !$TimestampUrl.Host -or $TimestampUrl.UserInfo) {
    throw 'Provide an HTTP(S) RFC 3161 timestamp URL without credentials.'
}
$certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint" -ErrorAction Stop
$codeSigning = @($certificate.EnhancedKeyUsageList | Where-Object { $_.ObjectId -eq '1.3.6.1.5.5.7.3.3' })
if (!$certificate.HasPrivateKey -or !$codeSigning -or $certificate.NotBefore -gt (Get-Date) -or $certificate.NotAfter -le (Get-Date)) {
    throw 'The selected CurrentUser certificate needs an accessible private key, Code Signing usage, and valid dates.'
}
$compiler = Get-Command signtool.exe -ErrorAction SilentlyContinue
$signTool = if ($compiler) { $compiler.Source }
if (!$signTool) {
    $sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $versions = Get-ChildItem -LiteralPath $sdk -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending
    foreach ($version in $versions) {
        $candidate = Join-Path $version.FullName 'x64\signtool.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $signTool = $candidate
            break
        }
    }
}
if (!$signTool) { throw 'Install the Windows SDK signing tools, or put signtool.exe on PATH.' }
if ($ValidateOnly) {
    Write-Host "Signing configuration found for $($certificate.Subject). No file was signed."
    return
}
$file = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).ProviderPath
if ([IO.Path]::GetExtension($file) -ine '.exe') { throw 'This signing script accepts executable files only.' }
$signature = Get-AuthenticodeSignature -LiteralPath $file
if ($signature.Status -ne 'NotSigned') {
    throw "Refusing to replace an existing or invalid signature ($($signature.Status)). Rebuild the executable first."
}
& $signTool sign /s My /sha1 $CertificateThumbprint /fd SHA256 /tr $TimestampUrl.AbsoluteUri /td SHA256 /d 'Voltura Worlds' /du 'https://voltura.se/' $file
if ($LASTEXITCODE) { throw 'Authenticode signing or timestamping failed.' }
$verified = & (Join-Path $PSScriptRoot 'verify-signature.ps1') -Path $file -CertificateThumbprint $CertificateThumbprint -AllowSelfSigned:$AllowSelfSigned
Write-Host "Signed and verified ($($verified.trust)): $file"

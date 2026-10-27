param(
    [string]$Exe = "",
    [string]$Pfx = "",
    [string]$Password = "",
    [string]$Subject = "",
    [string]$Timestamp = "http://timestamp.digicert.com",
    [switch]$NoTimestamp,
    [switch]$VerifyOnly
)
# Signs HyperSense64.exe with Authenticode so SmartScreen / AV vendors can attribute
# the binary to a publisher instead of treating it as anonymous.
#
# Examples
#   .\sign.ps1 -Pfx .\certs\hypersense.pfx -Password 'pfx-password'
#   .\sign.ps1 -Subject "HYPERSENSEGLOBAL"              # cert already in CurrentUser\My
#   .\sign.ps1 -Subject "MyPublisher" -NoTimestamp      # skip the RFC 3161 timestamp
#   .\sign.ps1 -VerifyOnly                              # check the existing signature
#
# Works with or without the Windows SDK: signtool.exe is used when present,
# otherwise the .NET Authenticode API (Set-AuthenticodeSignature) does it.
#
# The PFX password is passed on the command line here for convenience only - on a
# shared machine use an interactive prompt or a hardware token (see README).

$ErrorActionPreference = "Stop"
if (-not $Exe) { $Exe = Join-Path (Split-Path -Parent $PSScriptRoot) "HyperSense64.exe" }
if (-not (Test-Path $Exe)) { throw "executable not found: $Exe" }

$ts = ""
if (-not $NoTimestamp) { $ts = $Timestamp }

function Find-SignTool {
    $t = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
         Sort-Object FullName -Descending | Select-Object -First 1
    if ($t) { return $t.FullName }
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Invoke-SignTool([string]$tool, [string[]]$extra) {
    $params = @("sign", "/fd", "SHA256") + $extra +
              @("/d", "HYPERSENSEGLOBAL Live Usermode Memory Analysis Tool V1")
    if ($ts) { $params += @("/tr", $ts, "/td", "SHA256") }
    $params += $Exe
    & $tool @params
    if ($LASTEXITCODE -ne 0) { throw "signtool sign failed ($LASTEXITCODE)" }
}

$signtool = Find-SignTool

if (-not $VerifyOnly) {
    if ($Pfx) {
        if (-not (Test-Path $Pfx)) { throw "pfx not found: $Pfx" }
        if (-not $signtool) {
            throw "signing from a PFX needs signtool.exe (install the Windows SDK signing tools) " +
                  "or import the certificate into Cert:\CurrentUser\My and use -Subject."
        }
        Invoke-SignTool $signtool @("/f", $Pfx, "/p", $Password)
    }
    elseif ($Subject) {
        $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
                Where-Object { $_.HasPrivateKey -and $_.Subject -like "*$Subject*" } |
                Sort-Object NotAfter -Descending | Select-Object -First 1
        if (-not $cert) {
            throw "no code-signing certificate with a private key matching '$Subject' in Cert:\CurrentUser\My"
        }
        if ($cert.Subject -eq $cert.Issuer) {
            $warn = "'$($cert.Subject)' is SELF-SIGNED. It only verifies on machines that already trust that " +
                    "certificate. Other PCs, SmartScreen and AV vendors still see an untrusted file - do not " +
                    "expect a VirusTotal false positive to clear from this alone. Use a CA-issued OV/EV " +
                    "certificate for a public release."
            Write-Warning $warn
        }
        if ($signtool) {
            Invoke-SignTool $signtool @("/sha1", $cert.Thumbprint)
        }
        else {
            Write-Output "signtool.exe not found - signing with Set-AuthenticodeSignature instead"
            $signArgs = @{
                FilePath      = $Exe
                Certificate   = $cert
                HashAlgorithm = "SHA256"
            }
            if ($ts) { $signArgs.TimestampServer = $ts }
            $res = Set-AuthenticodeSignature @signArgs
            if ($res.Status -ne "Valid") {
                throw "signing failed: $($res.Status) - $($res.StatusMessage)"
            }
        }
    }
    else {
        throw "pass -Pfx <file.pfx> -Password <pw>, or -Subject <certificate subject>."
    }
}

if ($signtool) {
    & $signtool verify /pa /v $Exe
    if ($LASTEXITCODE -ne 0) { throw "signature verification failed ($LASTEXITCODE)" }
}
else {
    $sig = Get-AuthenticodeSignature $Exe
    if ($sig.Status -ne "Valid") { throw "signature verification failed: $($sig.Status) - $($sig.StatusMessage)" }
    Write-Output ("signed by : " + $sig.SignerCertificate.Subject)
}

Write-Output ("signature OK: " + $Exe)
Write-Output ("SHA-256     : " + (Get-FileHash $Exe -Algorithm SHA256).Hash)

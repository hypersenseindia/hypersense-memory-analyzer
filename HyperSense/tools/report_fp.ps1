param(
    [string]$Exe = "",
    [string]$Detection = "Program:Win32/Wacapew.Cml",
    [switch]$Open,
    [switch]$NoClipboard
)
# Prepares a Microsoft Defender false-positive report for the exact binary you are
# about to ship: hash, size, signature state, the paste text and the portal link.
#
# Examples
#   .\report_fp.ps1                  # build the report, copy the text to the clipboard
#   .\report_fp.ps1 -Open            # ... and open the WDSI submission page
#   .\report_fp.ps1 -Exe ..\..\HyperSense64.exe -NoClipboard
#
# Submit the file that hashes to the SHA-256 printed here. Every rebuild changes the
# hash, so run this after the final build, not before.

$ErrorActionPreference = "Stop"
if (-not $Exe) {
    # Prefer the signed release binary: that is what actually ships, and the hash
    # of the pre-signing file is worthless to the vendor.
    $signed = Join-Path (Split-Path -Parent $PSScriptRoot) "HXG SIGNED\HyperSense64-HXG-SIGNED.exe"
    $plain  = Join-Path (Split-Path -Parent $PSScriptRoot) "HyperSense64.exe"
    if (Test-Path $signed) { $Exe = $signed } else { $Exe = $plain }
}
if (-not (Test-Path $Exe)) { throw "executable not found: $Exe" }

$file = Get-Item $Exe
$hash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$size = $file.Length
$mib  = [math]::Round($size / 1MB, 2)
$sig  = (Get-AuthenticodeSignature $file.FullName).Status
$ver  = $file.VersionInfo.FileVersion

$text = @"
HyperSense64.exe is a usermode memory-analysis GUI that embeds the open-source
Moneta scanner (GPLv3, https://github.com/forrest-orr/moneta). The detection
$Detection is a machine-learning false positive: the tool is classified as a
PUA because it reads the memory of a process the user picks.

What it does, only after the user presses RUN: opens a user-selected process
with OpenProcess(PROCESS_VM_READ), calls VirtualQueryEx, reads PEB/TEB and module
images, and reports suspicious memory regions (private executable pages, modified
code, unsigned modules). SeDebugPrivilege is enabled only inside the hidden child
process immediately before a scan. No byte of another process is changed: only
VirtualQueryEx and ReadProcessMemory are used.

What it does not do: no persistence, no services, no drivers, no autorun, no
network traffic, no credential access, no code injection (no WriteProcessMemory,
no CreateRemoteThread, no hooks), and no files written except the report the
user explicitly saves. The import table has no clipboard calls, no key-state
polling, no crypto-provider calls and no temp-directory writes; SHA-256 is
implemented inside the program and copies use WM_COPY from the program's own
read-only edit control.

Build: x64, statically linked MinGW/UCRT, not packed, no UPX, no obfuscated or
encrypted strings, complete version resource (HYPERSENSEGLOBAL), asInvoker
manifest. The embedded engine is public GPLv3 source. File: $size bytes x64.
SHA-256: $hash

Please reclassify this file and remove the $Detection detection.
"@

$out = Join-Path $env:TEMP "hypersense_microsoft_fp.txt"
Set-Content -Path $out -Value $text -Encoding UTF8
if (-not $NoClipboard) { Set-Clipboard -Value $text }

Write-Output "File        : $($file.FullName)"
Write-Output "Version     : $ver"
Write-Output "Size        : $size bytes ($mib MiB)"
Write-Output "SHA-256     : $hash"
Write-Output "Signature   : $sig"
Write-Output "Detection   : $Detection (Microsoft Defender)"
Write-Output "Text length : $($text.Length) chars (portal limit is 1900)"
Write-Output "Text saved  : $out"
if (-not $NoClipboard) { Write-Output "Clipboard   : text copied - paste it into 'Additional information'" }
Write-Output ""
Write-Output "Portal      : https://www.microsoft.com/en-us/wdsi/filesubmission"
Write-Output "  Software developer -> Microsoft Defender Antivirus (Windows 11/10)"
Write-Output "  Company HYPERSENSEGLOBAL -> Incorrectly detected as malware/malicious"
Write-Output "  Detection name '$Detection' -> upload this exact exe -> Medium priority"
Write-Output ""
Write-Output "Do not rebuild before submitting: a new build is a new hash and a new case."
if ($sig -ne "Valid") {
    $warn = "The file is " + $sig + ". A CA-issued code-signing certificate is the durable fix " +
            "(tools\sign.ps1); the submission only clears this hash."
    Write-Warning $warn
}

if ($Open) { Start-Process "https://www.microsoft.com/en-us/wdsi/filesubmission" }

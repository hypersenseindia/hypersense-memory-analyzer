# Antivirus false positives (VirusTotal / Defender / SmartScreen)

`HyperSense64.exe` is a **memory scanner**. That is the whole point of the tool: it opens other
processes with `PROCESS_VM_READ`, walks their PEB/TEB, calls `VirtualQueryEx`, reads module
images and enables `SeDebugPrivilege` before a scan. Unsigned binaries that do this are
routinely scored as PUA / hacktool by machine-learning engines.

Typical report for this tool:

| Vendor | Verdict | What it actually is |
| --- | --- | --- |
| Microsoft | `Program:Win32/Wacapew.Cml` | PUA/ML "program" verdict, **not** a trojan |
| SecureAge | `Malicious` | generic static-ML verdict |

Everything else (Defender's real-time engine included) normally comes back clean, and the
Moneta engine it embeds is public GPLv3 source: <https://github.com/forrest-orr/moneta>.

> A 2/70 result is **not** evidence of malware, but it is worth fixing properly. Do **not**
> try to "evade" it with packers, crypters, string obfuscation or anti-VM tricks - those
> raise the ML score and get the binary classified as a hacktool for real.

---

## 1. Sign the binary (biggest single fix)

An Authenticode signature binds the file to a publisher identity. SmartScreen stops treating
it as anonymous, and AV vendors can whitelist the certificate instead of the hash.
An **EV code-signing certificate** is the fastest route to SmartScreen reputation.

```powershell
# from HyperSense\
.\tools\sign.ps1 -Pfx C:\certs\hypersense.pfx -Password 'pfx-password'

# or, if the certificate is already in CurrentUser\My:
.\tools\sign.ps1 -Subject "HYPERSENSEGLOBAL"

# check an existing signature
.\tools\sign.ps1 -VerifyOnly
```

The script uses `signtool` from the Windows SDK, SHA-256 + RFC 3161 timestamping (DigiCert),
and verifies with `/pa` when done. Sign the **final** build - every rebuild changes the hash.

## 2. Report the false positive to the vendors

Submit the **exact file/hash you ship**, once, after the final build:

```powershell
Get-FileHash .\HyperSense64.exe -Algorithm SHA256
```

* **Microsoft** - <https://www.microsoft.com/en-us/wdsi/filesubmission>
  Choose *Software developer* -> *Incorrectly detected as malware/PUA*, upload the exe and use
  the text below. Microsoft normally reclassifies within 24-72 h and pushes updated signatures.
* **SecureAge** - false-positive / support form on <https://www.secureage.com/> (include the
  same text and the SHA-256).
* Any other vendor that flags it: same message, same hash.

Submission text:

```
False positive: HYPERSENSEGLOBAL / HyperSense64.exe

HyperSense64.exe is a usermode memory-analysis front-end that embeds the open-source
Moneta scanner (GPLv3, https://github.com/forrest-orr/moneta). Reading another process's
memory is its documented function: it uses OpenProcess(PROCESS_VM_READ), VirtualQueryEx,
PEB/TEB reads and SeDebugPrivilege to map the target's regions and report suspicious
memory (private executable pages, modified code, unsigned modules).

It is not malware: no persistence, no services, no drivers, no network traffic, no
credential access, no code injection into other processes, no files written outside the
report the user explicitly saves. The GUI runs asInvoker; elevation is opt-in.

Detection name: Program:Win32/Wacapew.Cml (PUA/ML) / SecureAge "Malicious".
Please reclassify this file.

SHA-256: <paste hash>
Engine source: https://github.com/forrest-orr/moneta
```

## 3. Keep the build "boring"

Already done in this build, and worth preserving:

* statically linked, **not** packed, no UPX, no self-modifying code, no encrypted strings;
* complete version resource (company, product, description, copyright, comments) - see
  `res/resource.rc`;
* `asInvoker` manifest (no always-on admin), no service, no driver, no autorun;
* the only network-adjacent action is none - the tool never connects anywhere;
* SeDebugPrivilege is enabled only inside the hidden scan child, right before a scan.

If you add features later, avoid: process injection, `CreateRemoteThread`, keyboard hooks,
Defender tampering, hidden windows, and downloading payloads - each one moves the score in
the wrong direction.

## 4. Users who still get flagged

Only on a machine you control: add a Defender exclusion for the folder, or restore the file
from **Windows Security -> Protection history -> Allow**. Never ask end users to disable
real-time protection - report the hash instead.

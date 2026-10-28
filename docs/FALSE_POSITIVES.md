# Antivirus false positives (VirusTotal / Defender / SmartScreen)

`HyperSense64.exe` is a **memory scanner**. That is the whole point of the tool: it opens other
processes with `PROCESS_VM_READ`, walks their PEB/TEB, calls `VirtualQueryEx`, reads module
images and enables `SeDebugPrivilege` before a scan. Binaries that do this are routinely
scored as PUA / hacktool by machine-learning engines — signed or not.

Typical report for this tool:

| Vendor | Verdict | What it actually is |
| --- | --- | --- |
| Microsoft | **clean** on the unsigned build of this code | earlier builds were scored `Program:Win32/Wacapew.Cml` (PUA) and `Trojan:Win32/Wacatac.C!ml` / `Wacatac.Biml` for the same code |
| SecureAge | `Malicious` | generic static-ML verdict |

Everything else comes back clean (1/71 on VirusTotal for the unsigned build), and the
Moneta engine it embeds is public GPLv3 source: <https://github.com/forrest-orr/moneta>.
The released binary is the same code with an Authenticode signature added, so its hash —
`108160ae…` — needs its own scan before anyone quotes a count for it.

> A 1/71 result is **not** evidence of malware, but it is worth fixing properly. Do **not**
> try to "evade" it with packers, crypters, string obfuscation or anti-VM tricks - those
> raise the ML score and get the binary classified as a hacktool for real.

> The verdict name is not a property of the code. Microsoft's cloud model re-scores the same
> bytes over time and gives different answers for different builds: this project's
> `745206FD...` build went from `2/70` (`Wacapew`) to `1/70` (clean) with no rebuild and no
> submission, and later builds came back as `Wacatac.C!ml` and then `Wacapew.Cml` again.
> Rebuilding changes the hash, not the behaviour - treat a rebuild as a lottery ticket, not a
> fix. The fix is section 2 (report) and section 1 (sign).

---

## 1. Sign the binary (biggest single fix)

An Authenticode signature binds the file to a publisher identity. SmartScreen stops treating
it as anonymous, and AV vendors can whitelist the certificate instead of the hash.
An **EV code-signing certificate** is the fastest route to SmartScreen reputation.

The 1.0.1.1 release binary **is signed**: `CN=HYPERSENSEGLOBALT`, SHA-256, DigiCert RFC 3161
timestamp, `Get-AuthenticodeSignature` reports `Valid`. That certificate is **self-signed**,
which is honest but local: machines that have not installed its root still show *Unknown
publisher*, and no reputation accrues to it. Buying an OV/EV certificate from a CA and
re-signing the same bytes is the upgrade — the signature is the only thing that changes.

```powershell
# from HyperSense\
.\tools\sign.ps1 -Pfx C:\certs\hypersense.pfx -Password 'pfx-password'

# or, if the certificate is already in CurrentUser\My:
.\tools\sign.ps1 -Subject "HYPERSENSEGLOBAL"

# check an existing signature
.\tools\sign.ps1 -VerifyOnly
```

The script uses `signtool` from the Windows SDK, SHA-256 + RFC 3161 timestamping (DigiCert),
and verifies with `/pa` when done. Sign the **final** build - every rebuild changes the hash,
and signing changes it too, so hash and document the signed file, not the one before it.

## 2. Report the false positive to the vendors

Submit the **exact file/hash you ship**, once, after the final build:

```powershell
Get-FileHash .\HyperSense64.exe -Algorithm SHA256

# or let the helper print it, copy the paste text and open the portal:
.\HyperSense\tools\report_fp.ps1 -Open
```

* **Microsoft** - <https://www.microsoft.com/en-us/wdsi/filesubmission>
  Choose *Software developer* -> *Incorrectly detected as malware/malicious*, product
  *Microsoft Defender Antivirus (Windows 11/10)*, company `HYPERSENSEGLOBAL`, detection name
  `Program:Win32/Wacapew.Cml`, upload the exe and paste the text below (it fits the 1,900-char
  limit). The exact field-by-field walkthrough, with the current hash, is in
  **[MICROSOFT_FP.txt](MICROSOFT_FP.txt)**. Microsoft normally reclassifies within 24-72 h
  and pushes updated signatures.
* **SecureAge** - false-positive / support form on <https://www.secureage.com/> (include the
  same text and the SHA-256); see **[SECUREAGE_FP.txt](SECUREAGE_FP.txt)**.
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

Detection name: Program:Win32/Wacapew.Cml (ML, PUA) / SecureAge "Malicious".
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

The import table is the part machine learning reads first, and it is deliberately dull:

* **no clipboard APIs** - `Copy selection / Copy line / Copy all output` use `WM_COPY` on the
  program's own read-only edit control, so `OpenClipboard`/`SetClipboardData` are absent;
* **no key-state polling** - Ctrl+click reads `MK_CONTROL` from the mouse message instead of
  calling `GetKeyState`/`GetAsyncKeyState`, which is the keylogger-shaped part of the profile;
* **no crypto provider** - SHA-256 (used only to build a VirusTotal URL) lives in
  `src/sha256.h`; `CryptAcquireContext`/`CryptHashData` are not imported;
* **no temp-directory writes** - the opt-in RPC trace is written next to the executable.

A build that imports `ReadProcessMemory` *and* the clipboard *and* key-state polling *and* a
hash provider looks like a Discord/credential stealer to a cloud ML model, which is what the
`Wacatac` names label. Removing the last three costs nothing functionally.

That said, it is not sufficient on its own: the same build with those imports removed still
comes back as `Wacapew`/`Wacatac` from Microsoft. The residual score comes from the tool's
purpose - a user-mode memory reader is PUA-shaped by definition - so the vendor-side steps in
sections 1 and 2 are what actually clear it.

If you add features later, avoid: process injection, `CreateRemoteThread`, keyboard hooks,
clipboard readers, `GetAsyncKeyState`, `GetTempPath`, Defender tampering, hidden windows, and
downloading payloads - each one moves the score in the wrong direction.

## 4. Users who still get flagged

Only on a machine you control: add a Defender exclusion for the folder, or restore the file
from **Windows Security -> Protection history -> Allow**. Never ask end users to disable
real-time protection - report the hash instead.

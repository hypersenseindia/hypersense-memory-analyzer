# What's new — `1.0.1.1` vs the last public build (`1.0.1`)

The public `1.0.1` build was the sidebar GUI with a single live log. Everything below is what
this build adds or changes. The full item-by-item history is in
[`CHANGELOG.md`](../CHANGELOG.md).

| Area | `1.0.1` (old) | `1.0.1.1` (this build) |
| --- | --- | --- |
| Log area | one live output pane | **`OUTPUT` / `COMMAND` / `DETAILS`** tabs; the log keeps streaming while another tab is in front |
| Help | none | **tooltip on every control** — buttons, modes, filters, fields, output, shortcuts |
| Command line | generated internally, not shown | **editable `COMMAND` box** with the exact `moneta64`-equivalent line; `Enter` parses and runs it, `Esc` restores it |
| Command presets | none | **`PRESET` dropdown**: `-m ioc -p <pid>`, `-v detail`, `--filter *`, `-d`, `-v detail --option statistics --filter *`, `-m * -p <pid> -v detail` |
| Classic mode | none | **`CLASSIC MODE` toggle** — one-row `moneta64.exe`-style screen: search, process list, PID override, presets, command, `RUN` / `STOP` / `SAVE` / `CLEAR` / `REFRESH` |
| Output menu | none | **right-click**: copy selection / line / all, open path, show in folder, **Search on VirusTotal**, save TXT/HTML, clear. `Ctrl+click` opens a path directly |
| Argument coverage | fixed set | **all Moneta arguments** — `-m`, `-p`, `-v`, `-d`, `--option`, `--filter`, `--address`, `--region-size` |
| Unsigned modules | same colour as everything | **`FOCUS UNSIGNED`** toggle; unsigned modules are emphasised by default in the live view and in saved reports |
| Discord | none | **always-on Rich Presence** — scan target, mode, suspicious count and a live timer, with GitHub / Discord buttons |
| Window | fixed 1360×900 | **fits the work area**; sidebar picks one of five layouts for the real height (100 % → 150 % scaling), even frame insets, minimum size enforced |
| First paint | sidebar could clip on short screens | layout chosen before the first frame; no stray control box, no clipped `IOC FILTERS` / footer |
| Single instance | every launch opened a new window | second launch focuses the existing window; one process |
| Save confirmation | plain message | popup with `OPEN FILE` / `OPEN FOLDER` / `CLOSE` and wrapped paths |
| Clipboard | Win32 clipboard API (`OpenClipboard` / `SetClipboardData`) | the log control's own `WM_COPY` |
| Ctrl+click | `GetKeyState` / `GetAsyncKeyState` polling | `MK_CONTROL` from the mouse message |
| SHA-256 (VirusTotal) | CryptoAPI (`CryptAcquireContext` / `CryptHashData`) | local FIPS 180-4 implementation, `src/sha256.h` |
| RPC debug log | `%TEMP%\hypersense_rpc.log` | `hypersense_rpc.log` next to the executable |
| File details | `1.0.0.0`, partial metadata | **`1.0.1.1`**, full Details tab: company, product, description, copyright, trademark, original filename, comments |
| Signature | none (unsigned) | **Authenticode-signed**, `CN=HYPERSENSEGLOBALT`, SHA-256, DigiCert RFC 3161 timestamp |
| SecureAge | `Malicious` | `Malicious` (the only remaining flag on the unsigned build) |
| Microsoft | `2/70` flag (`Program:Win32/Wacapew.Cml`) | **clean — 1/71 on VirusTotal** (unsigned build of the same code) |

## Why the import table changed

A memory scanner that also imports the clipboard, key-state polling and a crypto provider reads
like an infostealer to cloud ML — that is what produced the `Wacatac`/`Wacapew` names. The three
features were re-implemented without those imports, with no functional loss:

* copy actions → `WM_COPY` on the program's own read-only log control;
* `Ctrl+click` → the `MK_CONTROL` flag already present in `WM_LBUTTONDOWN`;
* file hashing for the VirusTotal link → `src/sha256.h`.

## Verifying a build

```powershell
(Get-Item .\HyperSense64.exe).VersionInfo | Format-List CompanyName,ProductName,FileDescription,FileVersion,LegalCopyright,Comments
Get-FileHash .\HyperSense64.exe -Algorithm SHA256
Get-AuthenticodeSignature .\HyperSense64.exe | Format-List Status,@{n='SignedBy';e={$_.SignerCertificate.Subject}}
```

The signed release binary — `HyperSense\HXG SIGNED\HyperSense64 HXG SIGNED LATEST.exe`:

```
SHA-256  108160ae2315ebe110cac9bca6156e1ddeb81677f857e21cff6f7067d7eb9d7c
Size     1,853,096 bytes
Signed   CN=HYPERSENSEGLOBALT (self-signed), SHA-256, DigiCert RFC 3161 timestamp
```

The signature is the only difference from the unsigned `a35eaa0e…` build: 6 bytes of PE
checksum plus the certificate table, byte-identical everywhere else. A rebuild changes the
SHA-256 — and, with Microsoft's cloud ML, the verdict. Ship the build you scanned; after
signing, re-scan, because the hash is what the engines score.

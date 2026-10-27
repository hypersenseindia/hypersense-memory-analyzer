# Build and verification scripts

These are developer tools, not part of the shipped executable. They drive the real UI
through Win32 messages so a release can be verified without clicking.

## Signing

| Script | Purpose |
| --- | --- |
| `sign.ps1` | Authenticode sign + verify `HyperSense64.exe`. Works with the Windows SDK `signtool.exe`, or falls back to `Set-AuthenticodeSignature`. Warns when the certificate is self-signed. |

```powershell
.\sign.ps1 -Subject "HYPERSENSEGLOBAL"          # cert in Cert:\CurrentUser\My
.\sign.ps1 -Pfx .\cert.pfx -Password 'pw'       # PFX file
.\sign.ps1 -VerifyOnly
```

## UI verification

| Script | Purpose |
| --- | --- |
| `test_search.ps1` | Types a filter into **TARGET PROCESS**, reports the match count and optionally runs the scan. `-Filter expl`, `-Pick explorer.exe`, `-NoRun`. |
| `test_allstop.ps1` | Selects **ALL PROCESSES**, starts a run, presses **STOP** after a few seconds and asserts the UI survived. |
| `savereport2.ps1` | Clicks **SAVE TXT** / **SAVE HTML**, types a path into the common dialog and confirms the file was written. `-Format txt\|html -OutFile <path>`. |
| `savereport.ps1` | Earlier single-button version of the same test, kept for reference. |
| `test_stop.ps1`, `test_region.ps1`, `test_dialog.ps1` | Focused checks for STOP, REGION mode and the themed message boxes. |
| `comboinfo.ps1`, `autotest.ps1` | Enumerate the process combo; run a scripted end-to-end pass. |
| `capture.ps1` | Screenshot the app window: `.\capture.ps1 HyperSense64 out.png`. |

All of them expect the app to be running (`Start-Process .\HyperSense64.exe`) and launch it
with the same integrity level as the target if a scan is involved.

## Assets

| Script | Purpose |
| --- | --- |
| `make_icon.py` | Regenerates `res/icon.ico` from scratch (only needed when the icon is missing). |
| `ico_preview.py` | Renders the icon to a PNG preview. |

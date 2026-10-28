# Discord announcement — v1.0.2

Copy everything between the rules and paste it into your Discord channel. Discord renders the
markdown (`**bold**`, `##`, lists, links) automatically.

---

📢 **HYPERSENSEGLOBAL™ — V1.0.2 IS LIVE**

Live usermode memory analysis for Windows. **One file. No installer.** Now in two editions:
the full GUI and a **CLASSIC** build that opens straight into the `moneta64.exe`-style screen.

🔍 **What it does**
• Pick any running process — search by name, path or PID
• Scans its memory for injected or suspicious regions
• Live colour-coded output while it works
• Save the result as **TXT** or **HTML**

🆕 **NEW IN 1.0.2**
• **CLASSIC edition** — a one-row layout (search, process list, PID override, PRESET, editable COMMAND, RUN / STOP / SAVE / CLEAR / REFRESH) that starts that way, not hidden behind a toggle. Both editions can still switch layouts.
• **OUTPUT / COMMAND / DETAILS** tabs — the run, the exact engine command line, the resolved settings
• **Tooltips on every control** — buttons, modes, filters, fields, output, shortcuts
• **Editable COMMAND box + PRESET dropdown** — every Moneta argument (`-m`, `-p`, `-v`, `-d`, `--option`, `--filter`, `--address`, `--region-size`)
• **Right-click output menu** — copy, open path, show in folder, search the file's SHA-256 on VirusTotal, save TXT/HTML; `Ctrl+click` opens a path
• **FOCUS UNSIGNED** — unsigned modules stand out in the live view and in saved reports
• **Discord Rich Presence** — target, mode, suspicious count and a live timer
• **Adaptive window** — fits your work area from 100 % to 150 % scaling, one instance only
• **Signed and documented** — Authenticode (`HYPERSENSEGLOBALT`), full file details at `1.0.2`

⚡ **Highlights**
• Single .exe — engine built in, nothing else to install
• Modes: IOC / ALL / REGION / REFERENCED
• Filters: unsigned modules, metadata, CLR heap + JIT, WOW64
• STOP kills a scan instantly — the window never dies
• Dark UI, no drivers, no services, no telemetry

🖥️ Windows 10 / 11 · x64

⬇️ **DOWNLOAD** — pick one of four
https://github.com/hypersenseindia/hypersense-memory-analyzer/releases/tag/v1.0.2

**GUI edition**
• `HyperSense64.exe` — signed *(recommended)*
• `HyperSense64-unsigned.exe` — no signature

**CLASSIC edition** *(starts in the one-row layout)*
• `HyperSense64Classic.exe` — signed *(recommended)*
• `HyperSense64Classic-unsigned.exe` — no signature

🔐 **VERIFY**
```
HyperSense64.exe                   bcd48830528cb21166c4370c7a7e90b93e74ee12b0ede23d96e10fa15deb4df0
HyperSense64-unsigned.exe          568a1b428ddf4e46529d503a2dcd4b25fbaac708ed8312e0da426201d31b5f7a
HyperSense64Classic.exe            eb3764631f6d314f88271af0c039989307cc5da2ed9dc65e1a3cc7938f278a4a
HyperSense64Classic-unsigned.exe   53d1e04483a9ec76baf2012e9d01c15904efe09be86458b6bc56d8f4b9a69542
```

🛡️ **VirusTotal:** the last scanned build came back **1/71 — Microsoft clean**, SecureAge the
only flag. 1.0.2 changes the hashes, so scan the file you download. The certificate is
self-issued, so Windows may still show *Unknown publisher*; the unsigned files are there if
your endpoint protection dislikes untrusted signatures. Full old-vs-new table:
`docs/WHATS_NEW.md`

📦 **SOURCE** (GPLv3)
https://github.com/hypersenseindia/hypersense-memory-analyzer

⚠️ Use only on systems you own or are authorised to test.

────────────────────
**CREDITS**
🧠 Engine — Moneta
Forrest Orr — github.com/forrest-orr
Gabriel Landau — github.com/gabriellandau
🔧 Front end & branding — AG HYDRAXFF (@hydraxff_yt)
**HYPERSENSEGLOBAL™**
────────────────────

---

## Ultra-short version (status / ping)

📢 **HYPERSENSEGLOBAL™ V1.0.2 is out** — one .exe that scans a live process's memory and
reports injected regions. **NEW: CLASSIC edition** (one-row `moneta64`-style screen) next to the
full GUI · OUTPUT/COMMAND/DETAILS tabs · tooltips everywhere · editable command line + presets ·
right-click VirusTotal lookup · signed + unsigned · Windows x64.

⬇️ https://github.com/hypersenseindia/hypersense-memory-analyzer/releases/tag/v1.0.2

🧠 Engine: Moneta — Forrest Orr & Gabriel Landau · 🔧 Front end: AG HYDRAXFF (@hydraxff_yt)

---

## Previous release — v1.0.1

📢 **HYPERSENSEGLOBAL™ — V1 IS LIVE**

Live usermode memory analysis for Windows. **One file. No installer.**

• Pick any running process — search by name, path or PID
• Scans its memory for injected or suspicious regions
• Live colour-coded output while it works
• Save the result as **TXT** or **HTML**
• Modes: IOC / ALL / REGION / REFERENCED
• Filters: unsigned modules, metadata, CLR heap + JIT, WOW64

⬇️ https://github.com/hypersenseindia/hypersense-memory-analyzer/releases/tag/v1.0.1

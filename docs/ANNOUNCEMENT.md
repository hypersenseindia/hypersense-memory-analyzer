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

⬇️ **DOWNLOAD** — three files
https://github.com/hypersenseindia/hypersense-memory-analyzer/releases/tag/v1.0.2

• `HyperSense64.BY.HXG.1.0.1.exe` — signed, built 1.0.1.0
• `HyperSense64.Classic.LATEST.1.0.2.exe` — signed, built 1.0.1.1
• `HyperSense64Classic-unsigned.exe` — no signature, built 1.0.2

🔐 **VERIFY**
```
HyperSense64.BY.HXG.1.0.1.exe          c8dfaf8aa311dd46276e2349420e5788290fb2e942d2ba070007c4ca727274a6
HyperSense64.Classic.LATEST.1.0.2.exe  108160ae2315ebe110cac9bca6156e1ddeb81677f857e21cff6f7067d7eb9d7c
HyperSense64Classic-unsigned.exe       53d1e04483a9ec76baf2012e9d01c15904efe09be86458b6bc56d8f4b9a69542
```

Read the Details tab, not the file name: only `HyperSense64Classic-unsigned.exe` reports `1.0.2`,
the other two are older signed builds.

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

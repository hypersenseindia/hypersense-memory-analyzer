# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions use
[Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Added
- **Tooltips on every control.** Hovering any button, mode, filter, field or the output
  explains what it does, including the keyboard and mouse shortcuts.
- **`OUTPUT` / `COMMAND` / `DETAILS` tabs** above the log. `COMMAND` shows the exact
  `moneta64`-equivalent command line for the current run, `DETAILS` shows the resolved
  target, options, filters, limits and timings. The live log keeps streaming while another
  tab is in front.
- **Classic "one row" mode**, matching `moneta64.exe`: hides the sidebar, shows the target
  search plus the full process list, an optional PID/name override, then
  `RUN` / `STOP` / `SAVE TXT` / `SAVE HTML` / `CLEAR` / `REFRESH`. The selected process
  fills `-p` automatically.
- **Command presets in classic mode.** A `PRESET` dropdown fills the `COMMAND` box with a
  ready-to-run `moneta64.exe` line — `-m ioc -p <pid>`, `-v detail`, `--filter *`, `-d`,
  `-v detail --option statistics --filter *` and `-m * -p <pid> -v detail`. `<pid>` is
  replaced by the process chosen in the list, and the preset follows the settings when they
  are changed by hand (it falls back to *Custom* when nothing matches).
- **Editable command line.** The `COMMAND` box is a real edit field: change any argument and
  press **Enter** to parse it (`-m`, `-p`, `-v`, `-d`, `--option`, `--filter`, `--address`,
  `--region-size`), apply it to the GUI and start the scan. **Esc** restores the command
  generated from the current settings.
- **Full Moneta argument control** — `-m` mode, `-p` process, `-v` verbosity, `-d` debug
  level, `--option`, `--filter`, `--address` and `--region-size` are all exposed.
- **Right-click menu in the output** (and in `COMMAND` / `DETAILS`): copy selection, copy
  line, copy all, open path, show in folder, **Search on VirusTotal**, save TXT/HTML and
  clear. `Ctrl+click` opens a path directly; plain clicking shows the path tooltip.
- **VirusTotal lookup.** With a DLL or file path selected, the menu hashes the file with
  SHA-256 and opens `https://www.virustotal.com/gui/file/<hash>` in the default browser.
  Non-files (module names, PIDs, addresses) are greyed out.
- **`FOCUS UNSIGNED`** recolours new output: unsigned modules are emphasised, signed and
  routine lines are dimmed. Toggling it rebuilds the visible view from the retained log.
- **Discord Rich Presence.** Always on — when the tool runs, Discord shows what is being
  scanned (`Scanning notepad.exe (PID 4242)`), the mode and suspicious count, plus a live
  elapsed timer that turns into the final result (`12 suspicious regions` / `no suspicious
  memory regions`). The card carries **GitHub** and **Join our Discord** buttons. No toggle
  and no extra panel: it connects to the Discord desktop app automatically and disappears
  when the tool closes. The Client ID can be overridden with `[discord] client_id` in
  `HyperSense.ini` or the `HYPERSENSE_DISCORD_CLIENT_ID` environment variable.

### Changed
- Unsigned modules stand out by default in the live view and in saved reports.
- Output is no longer rebuilt from scratch when switching tabs, so streaming stays smooth.
- `COMMAND` omits `-v surface` because surface is the engine default, so the shown line is
  exactly what runs.
- The import table no longer contains the clipboard, key-state or crypto-provider APIs, which
  were the only differences from the build Microsoft scored clean and, next to the memory
  reads, made the binary look like a Discord/credential stealer (`Trojan:Win32/Wacatac.Biml`):
  copy actions now use `WM_COPY` on the log control, Ctrl+click reads `MK_CONTROL` from the
  mouse message, SHA-256 for VirusTotal is implemented in `src/sha256.h`, and the opt-in
  Discord protocol trace is written next to the executable instead of `%TEMP%`.
- Build metadata is completed and versioned `1.0.1.1`: file/product version, manifest
  assembly identity, company, product name, description, copyright, trademark, original
  filename and a `Comments` field that states what the tool reads and writes, all in the
  Explorer *Details* tab. This is the build to scan and to submit to the antivirus vendors.

### Fixed
- The main menu now fits the window it is given. The sidebar picks one of five vertical
  layouts (nominal, compact, two-column IOC filters, tight, ultra) for the real client
  height, so a short screen or a 125%/150% display scale no longer clips `IOC FILTERS` or
  pushes the footer out of view on the first frame. Frame padding is now even on all sides
  (18 px left/right, 10 px under the header and above the footer), and the window opens
  inside the work area instead of at the nominal 1360x900.
- The saved-report confirmation wraps long paths onto the next line instead of cutting them
  off at the dialog edge, and offers `OPEN FILE` / `OPEN FOLDER` / `CLOSE`.
- Classic-mode text jitter: the status line, the timer and the owner-drawn dropdown each
  repainted a stale or wrong region, which made the labels shimmer while a scan was running.
  Each control now invalidates only its own rectangle and the closed combo is painted in a
  single pass.
- `DEV` block no longer overlaps the credits when the window is at its minimum width, and
  the `FOCUS UNSIGNED` toggle has enough room for its label.
- Switching between classic mode and the main menu no longer leaves ghost text or old
  panels behind: hidden controls are parked outside the client area and the switch repaints
  the frame and every child in one pass.
- A second launch activates the window that is already open instead of stacking a new one
  on top of it (which showed a half-covered header), and resizing repaints the frame and
  children together so no strip of the previous layout survives.
- The first paint of the main menu no longer shows a stray filled box at the top-left: the
  classic-only fields (`PID` override, `PRESET`, `COMMAND`) were created hidden at `0,0`,
  and the background painter drew a field frame for them. Hidden fields are now skipped by
  the painter and parked outside the client area from creation, so the first frame matches
  what switching classic → main used to repair.

### Docs
- Added **`docs/WHATS_NEW.md`**, a single old-vs-new table for the `1.0.1` → `1.0.1.1` changes
  (tabs, tooltips, classic mode, presets, editable command, import table, metadata, AV status).
- Added **`docs/MICROSOFT_FP.txt`**, a field-by-field Microsoft Defender false-positive
  submission (WDSI portal) for the current build, including the exact 1,900-char-limit text,
  the SHA-256 and the reason the verdict is machine-learning noise. `docs/FALSE_POSITIVES.md`
  and the README now list the live VirusTotal result — **1/71, Microsoft clean**, SecureAge
  `Malicious` only — instead of claiming a clean scan or hiding the vendors that flag it.
  `HyperSense/tools/report_fp.ps1` prints the hash for the exact build, copies the paste
  text to the clipboard and opens the submission portal.

---

## [1.0.1] — 2026-10-27

### Added
- **Target process search.** The picker filters live by image name, full path or PID,
  shows the match count and selects the first hit. Empty filter restores the full list.
- **Separate `SAVE TXT` and `SAVE HTML` buttons.** Each opens the save dialog preset to its
  format; a typed extension overrides the preset.
- HXG branded banner in the engine output, with the Moneta authors credited alongside.
- `Subregion`, thread, process, PE and statistics members are now value-initialised
  (`NOTICE.md` §3) — removes random "primary image base" IOCs and garbage statistics.
- `HyperSense/tools/sign.ps1` — Authenticode signing and verification, with or without the
  Windows SDK installed.
- `docs/FALSE_POSITIVES.md` — vendor submission text and signing guidance.

### Changed
- The engine is compiled into `HyperSense64.exe`; no `Moneta64.exe` is required.
- Version resource completed: company, product, description, copyright, trademarks and a
  comments field naming the GPLv3 engine and its source.
- Manifest: `asInvoker` retained, `longPathAware` added.

### Fixed
- Heap corruption (`0xC0000374`) at process teardown — `FileBase` released a `new[]`
  buffer with scalar `delete` and left `FileData` uninitialised.
- Whole scan aborted when a thread disappeared mid-enumeration ("failed to map address
  space of *pid* (error 4)"). Vanished threads are now skipped.
- Intermittent "ended early" report caused by the heap bug above.

---

## 1.0.0 — 2026-10-26

### Added
- First GUI release: dark Win32 interface, process picker, IOC/ALL/REGION/REFERENCED
  modes, IOC filter toggles, live output, TXT/HTML reports, elevation helper.
- Built-in Moneta v1.0.1 engine, GPLv3, no external executable.

---

[Unreleased]: https://github.com/hypersenseindia/hypersense-memory-analyzer/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/hypersenseindia/hypersense-memory-analyzer/releases/tag/v1.0.1

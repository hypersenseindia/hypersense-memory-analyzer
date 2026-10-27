# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions use
[Semantic Versioning](https://semver.org/).

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

[1.0.1]: https://github.com/hypersenseindia/hypersense-memory-analyzer/releases/tag/v1.0.1

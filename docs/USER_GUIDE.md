# HYPERSENSEGLOBAL™ — Live Usermode Memory Analysis Tool V1

Manual for the HYPERSENSEGLOBAL front end - see the [project README](../README.md) for the overview and downloads.

A dark, single-window front end for the [Moneta](https://github.com/forrest-orr/moneta)
usermode memory scanner. No command line, no flags to memorise: pick a target, flip the
toggles, press **RUN ANALYSIS**, read the live output, save the report.

```
ENGINE CREDITS : Forrest Orr & Gabriel Landau  (Moneta, GPLv3)
TOOL BUILD     : HYPERSENSEGLOBAL V1 — zero-crash build
DEVELOPED BY   : AG HYDRAXFF  (@hydraxff_yt)
INSTAGRAM      : https://www.instagram.com/hydraxff_yt/
```

---

## What it does

Moneta walks the virtual address space of a live process (or every process) looking for
memory that behaves like injected code: private executable regions, modules that do not
match their on-disk image, CLR JIT artefacts, unsigned modules, WOW64 transition stubs, and
so on. HYPERSENSEGLOBAL drives that engine and turns its console text into a readable,
colour-coded live feed plus a shareable report.

* Target a running process by name/PID, or scan **ALL PROCESSES**
* **SEARCH** box filters the target list live by process name, path or PID
* `SELECT .EXE` finds an executable on disk and auto-selects the matching running process
* **Moneta is compiled into the tool** — one standalone `HyperSense64.exe`, no external engine file
* Memory selection: **IOC**, **ALL**, **REGION**, **REFERENCED** (with address/size fields)
* Verbosity: **SURFACE**, **DETAIL**, **DEBUG**
* Options: permission statistics, scan-from-allocation-base, memory dump (`-d`)
* IOC filters: all filters, unsigned modules, metadata modules, CLR heap, CLR prvx, Wow64 init
* Live streaming output, 30 000-line viewer cap (the full output is always kept for the report)
* Moneta-style colouring: turquoise process metadata, cyan addresses, gold memory types,
  yellow descriptors (Heap/TEB/Stack/.NET) and **red IOC descriptions**
* Live IOC hit counter (viewer header, status bar) and a red `[ RESULT ]` summary when a scan ends
* **STOP** terminates a running scan instantly without freezing the window
* **SAVE TXT** / **SAVE HTML** write a report with host, SID, timestamps, flags, result and full output

---

## Requirements

* Windows 10 / 11, x64
* To build: MSYS2 with the **UCRT64** toolchain (`g++`), or any MinGW-w64 g++ 13+
* The Moneta engine sources (`../Source`, `../Headers`) are compiled directly into the
  executable, so **no `Moneta64.exe` is required** next to the tool.

The tool runs **asInvoker** — no administrator prompt unless you press **ELEVATE**.
Elevation is only needed to inspect protected / elevated processes.

---

## Build

```bat
cd HyperSense
build.bat
```

Output: `HyperSense\HyperSense64.exe`, automatically copied to the repository root as `HyperSense64.exe`.
The binary contains both the GUI and the Moneta engine — copy that single file anywhere and
it works.

The build script:

1. adds `C:\msys64\ucrt64\bin` to `PATH` (the compiler needs its own DLLs),
2. compiles `res\resource.rc` (icon + manifest + version info) with `windres`,
3. compiles `src\main.cpp` plus every `..\Source\*.cpp` engine translation unit,
4. statically links everything — no runtime dependencies beyond the Windows system DLLs,
5. regenerates `res\icon.ico` with `tools\make_icon.py` if the icon is missing.

> The repository path contains `!!`. `build.bat` therefore keeps cmd's *delayed expansion*
> disabled, otherwise cmd would strip those characters from every expanded path.

### Signing / antivirus

A memory scanner that reads other processes is a magnet for ML false positives. Sign the
final build and report the hash to the vendors that flag it - full procedure, submission
text and `HyperSense/tools/sign.ps1` usage in **[FALSE_POSITIVES.md](FALSE_POSITIVES.md)**.

---

## Usage

1. Start the target program you want to inspect.
2. Launch `HyperSense64.exe`.
3. Choose the target in **TARGET PROCESS**, or press **REFRESH** after starting a new program.
   Type in **SEARCH** to narrow the list by process name, full path or PID; the header shows
   how many entries match.
4. Pick the memory selection and verbosity, adjust the option/filter toggles.
5. **RUN ANALYSIS** — output streams live into the right-hand pane. IOC region lines are
   printed in red, memory types in gold and descriptors in yellow, mirroring Moneta's console.
6. **SAVE TXT** or **SAVE HTML** - each button opens a save dialog already set to that format.

When the engine finishes, the viewer adds a `[ RESULT ]` line: red with the number of flagged
regions when IOCs were found, green when the target is clean. The same verdict is written to
the report as the `RESULT` row.

### Built-in engine

There is nothing to locate or install. The Moneta engine is compiled into the executable and
runs as a hidden child of the tool itself:

```
HyperSense64.exe --engine -m ioc -p <PID> -v detail --option statistics --filter *
```

Running the engine in a child process keeps a scan failure away from the UI: even if the
engine hits a bug, the window stays responsive and reports the failure instead of crashing.

### Flags the GUI builds

| Control | Command line |
| --- | --- |
| Memory selection IOC / ALL / REGION / REFERENCED | `-m ioc` / `-m *` / `-m region` / `-m referenced` |
| Target | `-p <PID>` or `-p *` |
| Verbosity SURFACE / DETAIL / DEBUG | `-v surface` / `-v detail` / `-v debug` |
| Dump selected memory | `-d` |
| Permission statistics | `--option statistics` (comma-combined with `from-base`) |
| Scan from allocation base | `--option from-base` |
| All filters / individual filters | `--filter *` or `--filter unsigned-modules clr-heap ...` |
| REGION / REFERENCED | `--address 0x...` `--region-size 0x...` |

The exact command line used for a run is echoed into the viewer and stored in the report.

### Memory dump

With **Dump selected memory (-d)** enabled, output files land in `HYPERSENSE_DUMPS\` next to
the launcher, so the working folder stays clean.

---

## Report contents

```
HYPERSENSEGLOBAL(TM) — Live Usermode Memory Analysis Tool | V1

REPORT GENERATED : 2026-10-27 12:56:29.587  (UTC+05:30)
UTC TIME         : 2026-10-27 07:26:29.587 UTC
COMPUTER NAME    : MASTER-PC
ACCOUNT          : MASTER-PC\HIMAXKA
USER SID         : S-1-5-21-...-1001
OPERATING SYSTEM : Windows 10 Pro 22H2 (10.0.19045, x64)
ELEVATION        : ADMINISTRATOR
ENGINE           : Built-in - Moneta v1.0.1 (compiled into this tool)

TARGET / SCAN STARTED / SCAN FINISHED / SCAN DURATION
MEMORY SELECTION / VERBOSITY / OPTIONS / FILTERS / EXIT CODE
RESULT           : 11 suspicious regions flagged
COMMAND LINE     : ...

--- full engine output ---

Engine credits, tool build, developer, Instagram
```

The HTML variant is a self-contained dark-themed page (no external assets) suitable for
attaching to a ticket or a chat. It uses the same colour scheme as the live viewer, so IOC
descriptions stay red and memory types stay gold.

---

## Notes & limits

* **Moneta returns exit code 1 on success.** The exit code is reported as-is.
* A scan of *every* process at **DETAIL** verbosity produces a very large amount of text;
  the viewer stops rendering at 30 000 lines (and says so), while the report keeps everything
  up to 64 MB.
* The engine is a separate 64-bit console process. It cannot inspect 64-bit processes if you
  replace it with a 32-bit build.
* Region / Referenced modes require a valid address; the tool validates the field before
  launching the engine.
* Some protected processes are only accessible when the tool runs elevated.

---

## Project layout

```
HyperSense/
  src/main.cpp          entire application (UI, process list, runner, report writer)
  res/resource.rc       icon, manifest, version info
  res/app.manifest      asInvoker, common controls v6, DPI aware
  res/icon.ico          HYPERSENSEGLOBAL brand icon (multi-size)
  build.bat             UCRT64 build script
  tools/make_icon.py    fallback icon generator (only used if res/icon.ico is missing)
  tools/sign.ps1        Authenticode signing + verification helper
  tools/*.ps1           UI automation used to verify builds
```

---

## Licence

The Moneta engine (`Source/`, `Headers/`, compiled directly into this executable) is
Copyright © 2020 Forrest Orr and Gabriel Landau, licensed under **GPLv3** — see `../LICENSE`.
This front end runs that engine in an embedded hidden child process and is distributed
under the same licence. HYPERSENSEGLOBAL™ is a branding layer only; all memory-analysis
logic belongs to the Moneta authors.

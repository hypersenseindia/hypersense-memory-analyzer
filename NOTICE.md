# NOTICE — attribution and statement of changes

This repository contains a modified version of **Moneta**.

```
Moneta — Live usermode memory analysis
Copyright (C) 2020 Forrest Orr and Gabriel Landau
  Forrest Orr     — https://github.com/forrest-orr
  Gabriel Landau  — https://github.com/gabriellandau
https://github.com/forrest-orr/moneta
Licensed under the GNU General Public License v3.0
```

The upstream project is distributed under **GPLv3**. This derivative work is distributed
under the same licence (see [LICENSE](LICENSE)), and the complete corresponding source —
both the original engine and every modification listed below — is contained in this
repository under `Source/`, `Headers/` and `HyperSense/`.

`HYPERSENSEGLOBAL™` is a branding and packaging layer by **AG HYDRAXFF**
([@hydraxff_yt](https://www.instagram.com/hydraxff_yt/)). It adds a GUI, a build system and
defect fixes. It does **not** claim authorship of the memory-analysis engine.

---

## Changes made to Moneta in this build

Required by GPLv3 §5(a): the modified files carry prominent notices of the change, and this
file records them in one place.

### 1. Packaging — engine embedded in the GUI

* The engine entry point is exposed as `MonetaMain(int, const wchar_t *[])` in
  `Source/Console.cpp`, so it can be linked into the GUI executable instead of being a
  standalone `Moneta64.exe`.
* The GUI re-launches **itself** with `--engine <moneta arguments>` as a hidden child
  (`CREATE_NO_WINDOW`) and reads its standard output through a pipe.
* Added a branded banner in `Source/Console.cpp` (replaces the upstream ASCII art). Moneta
  and its authors are credited in the banner and in every report.

### 2. Portability — building the engine with MinGW-w64

Upstream targets MSVC. The following changes are needed for the MinGW/UCRT64 toolchain and
are behaviour-neutral on MSVC:

* `Headers/PEB.h` — added a `HANDLE64` typedef and guarded the MSVC-only declarations.
* `Headers/StdAfx.h` — include `<memory>` and `<locale>` explicitly (MSVC pulls them in
  transitively, MinGW does not).
* `Source/PeFile.cpp`, `Source/Regions.cpp` — explicit `uintptr_t` / `uint8_t *` casts that
  MSVC accepted implicitly.
* `Source/Console.cpp` — `MAKEINTRESOURCEA(10)` for the console menu resource.
* `HyperSense/build.bat` — single-command build with `g++` + `windres`.

### 3. Defect fixes

These are genuine upstream bugs that MSVC's runtime happened to mask. Each is listed below
so a reviewer can diff against upstream Moneta v1.0.1.

| File | Defect | Effect |
| --- | --- | --- |
| `Source/FileIo.cpp` (`FileBase`) | `new[]` buffer released with scalar `delete`; `FileData` never initialised | Heap corruption (`0xC0000374`) at teardown |
| `Headers/Memory.hpp`, `Headers/FileIo.hpp` | `Subregion::Flags`, `Subregion::PrivateSize`, `PermissionRecord::TotalRegions`, `IocRecord::TotalIoc` uninitialised | Random "primary image base" hits, garbage statistics, `-nan%` |
| `Source/Thread.cpp` | Thread `TEB`/stack addresses uninitialised; constructor threw `1/2/3/4` when a thread vanished mid-enumeration | Whole scan aborted with "failed to map address space (error 4)" |
| `Source/Process.cpp` | `Process::ImageBase` / `Wow64` uninitialised | Wrong image base for the first scanned process |
| `Source/PeFile.cpp` | `PeVm` body fields uninitialised | Spurious module hits |
| `Headers/PEB.h` | `PebModule::Missing` uninitialised | Non-deterministic module verdicts |
| `Source/Subregions.cpp`, `Source/Statistics.cpp` | Counters not zeroed before accumulation | Skewed statistics |

Vanished threads are now skipped instead of aborting the run; allocations are consistently
`new[]`/`delete[]`.

### 4. Front end

`HyperSense/` is new code (GPLv3, same licence): Win32 GUI, process picker with search,
mode/filter controls, live output view, TXT/HTML report writer, elevation helper, build
script, icon, version resource and manifest.

---

## Source availability

If you received a binary of this project, the corresponding source is this repository at
the matching tag. Redistributions must keep this notice, the upstream copyright notices and
the GPLv3 licence text.

## No warranty

The software is provided "as is", without warranty of any kind. Memory analysis can crash
or hang the inspected process and, on a misconfigured system, the machine. Use it only on
systems you own or are authorised to test.

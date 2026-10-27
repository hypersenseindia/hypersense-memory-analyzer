# Contributors

HYPERSENSEGLOBAL exists because of other people's work. This page records who did what, so
credit is unambiguous and survives the next rewrite of the README.

## Upstream — the analysis engine

| | |
| --- | --- |
| <img src="https://github.com/forrest-orr.png" width="64" alt="Forrest Orr"> | **Forrest Orr** — [@forrest-orr](https://github.com/forrest-orr)<br>Author of [Moneta](https://github.com/forrest-orr/moneta): PEB/TEB walking, region enumeration, IOC classification, statistics, console UI. |
| <img src="https://github.com/gabriellandau.png" width="64" alt="Gabriel Landau"> | **Gabriel Landau** — [@gabriellandau](https://github.com/gabriellandau)<br>Co-author of Moneta: memory-region heuristics, PE parsing, IOC filters, verification and testing. |

Moneta is licensed under **GPLv3**. Everything in `Source/` and `Headers/` originates there;
this repository modifies it only as described in [NOTICE.md](../NOTICE.md).

If you use this tool, star the original project: <https://github.com/forrest-orr/moneta>

## This project — front end and packaging

| | |
| --- | --- |
| <img src="https://github.com/hypersenseindia.png" width="64" alt="AG HYDRAXFF"> | **AG HYDRAXFF** — [@hydraxff_yt](https://www.instagram.com/hydraxff_yt/)<br>Win32 GUI, process picker and search, mode/filter mapping, out-of-process scan child, live output view, TXT/HTML report writers, elevation flow, build system, branding, and the engine defect fixes (heap corruption, uninitialised state, vanished-thread abort). |

## How to be added here

1. Open a pull request that is merged, or report a bug that leads to a fix.
2. Add yourself to this file in the same PR (or ask in the PR and a maintainer will).
3. Keep it factual: name, handle, one line describing the contribution.

There is no CLA. Contributions are accepted under GPLv3, matching the project licence.

## Thanks

* Everyone who reported false positives back to the vendors and tested the signed builds.
* The MinGW-w64 and MSYS2 maintainers — the engine build in `HyperSense/build.bat` is theirs
  as much as ours.

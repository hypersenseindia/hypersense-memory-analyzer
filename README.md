<p align="center">
  <img src="docs/assets/logo.png" alt="HYPERSENSEGLOBAL" width="140">
</p>

<h1 align="center">HYPERSENSEGLOBAL&trade;</h1>

<p align="center">
  <b>Live usermode memory analysis for Windows</b><br>
  <sub>Single executable &middot; no installer &middot; no kernel driver &middot; no telemetry</sub>
</p>

<p align="center">
  <a href="https://github.com/hypersenseindia/hypersense-memory-analyzer/releases"><img alt="Release" src="https://img.shields.io/github/v/release/hypersenseindia/hypersense-memory-analyzer?color=22d3ee&label=release"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20x64-0ea5e9">
  <img alt="Language" src="https://img.shields.io/badge/C%2B%2B-17-00599c">
  <img alt="Licence" src="https://img.shields.io/badge/licence-GPL--3.0-blue">
  <img alt="Size" src="https://img.shields.io/badge/binary-1.7%20MB-22d3ee">
</p>

---

HYPERSENSEGLOBAL is a Windows front end for the [Moneta](https://github.com/forrest-orr/moneta)
memory scanner. It inspects a running process and reports memory that looks like an
injection: private executable pages, modified code sections, unsigned or metadata-less
modules, CLR heaps and JIT regions.

The scanner is **compiled into the executable** — there is no second binary to ship and no
`Moneta64.exe` to keep next to it. The GUI talks to it as a hidden child process, so a
crash inside the scanner can never take the window down with it.

<p align="center">
  <img src="docs/assets/scan-results.png" alt="HYPERSENSEGLOBAL scanning explorer.exe" width="880">
</p>

## Features

| | |
| --- | --- |
| **One file** | `HyperSense64.exe`, ~1.7 MB, statically linked. Copy it anywhere and run it. |
| **Target search** | Filter the process list live by name, full image path or PID. Shows the match count and falls back to the first hit. |
| **Four memory modes** | `IOC` (suspicious regions only), `ALL`, `REGION` (one address range / region size) and `REFERENCED` (regions that reference a given address). |
| **IOC filters** | Apply all, unsigned modules, metadata modules, CLR heap, CLR JIT (`prvx`), WOW64 initialisation. |
| **Live output** | The scanner streams to the window as it works; 30 000 lines are rendered, the full log is always kept for the report. |
| **Reports** | `SAVE TXT` and `SAVE HTML` write host name, account, SID, OS build, elevation, timestamps, options, result and the complete engine output. |
| **Isolation** | The scanner runs out of process (`--engine`, no console window). `STOP` terminates it immediately; the UI stays responsive. |
| **No surprises** | `asInvoker` manifest, opt-in elevation button, no drivers, no services, no autorun, no network access, no telemetry. |

## Quick start

1. Download `HyperSense64.exe` from [Releases](https://github.com/hypersenseindia/hypersense-memory-analyzer/releases).
2. Right-click → **Run as administrator** (or use the **ELEVATE** button) to inspect protected processes.
3. Pick a target — type in **TARGET PROCESS** to filter, or choose **ALL PROCESSES** for a system-wide run.
4. Press **RUN ANALYSIS** and watch **LIVE ANALYSIS OUTPUT**. **STOP** ends the scan at any time.
5. **SAVE TXT** or **SAVE HTML** exports the result.

A typical run against `explorer.exe`:

```
  [ 2026-10-27 15:54:40.687 ]  ANALYSIS COMPLETE  |  exit 1 (scan complete)  |  15.109 s
  [ RESULT ]  18 suspicious regions flagged
```

> The engine exits with code **1 on a successful scan** — that is Moneta's convention, not a failure.

## Build from source

Requirements: Windows 10/11 x64, [MSYS2](https://www.msys2.org/) with the `ucrt64` toolchain.

```bat
cd HyperSense
build.bat
```

Output: `HyperSense\HyperSense64.exe`, copied to the repository root. Full details,
troubleshooting and the CI recipe are in **[docs/BUILDING.md](docs/BUILDING.md)**.

## How it works

```
HyperSense64.exe  (GUI, Win32/Direct2D, asInvoker)
   └─ CreateProcessW(self, "--engine <moneta args>", CREATE_NO_WINDOW)
         └─ Moneta engine  ──stdout pipe──▶  live view + report
```

The GUI re-launches **itself** with `--engine` and the equivalent Moneta command line.
That keeps the analysis in a disposable process: a fault in the scanner costs you the
scan, not the session. The design and the safety fixes applied to the engine while
porting it to MinGW are described in **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**.

## Antivirus and false positives

A memory scanner opens other processes with `PROCESS_VM_READ`, walks `PEB`/`TEB` and
enables `SeDebugPrivilege` before a scan. Unsigned binaries doing that are regularly
scored as PUA/hacktool by machine-learning engines.

The current release build is clean on Microsoft Defender and on the overwhelming
majority of VirusTotal engines. If a vendor flags your copy, do **not** disable your
antivirus — read **[docs/FALSE_POSITIVES.md](docs/FALSE_POSITIVES.md)**, which includes the
submission text and the signing procedure.

## Intended use

HYPERSENSEGLOBAL is a diagnostic tool for **systems you own or are authorised to test**
— malware triage, incident response, reverse engineering, detection engineering.

It does not inject code, does not modify the target, does not persist anything and does
not communicate with any network endpoint. It reads memory and writes the report you ask
for.

## Contributors

<table>
  <tr>
    <td align="center" width="170">
      <a href="https://github.com/forrest-orr">
        <img src="https://github.com/forrest-orr.png" width="72" alt="Forrest Orr"><br>
        <sub><b>Forrest Orr</b></sub>
      </a><br>
      <sub>Moneta engine</sub>
    </td>
    <td align="center" width="170">
      <a href="https://github.com/gabriellandau">
        <img src="https://github.com/gabriellandau.png" width="72" alt="Gabriel Landau"><br>
        <sub><b>Gabriel Landau</b></sub>
      </a><br>
      <sub>Moneta engine</sub>
    </td>
    <td align="center" width="170">
      <a href="https://github.com/hypersenseindia">
        <img src="https://github.com/hypersenseindia.png" width="72" alt="AG HYDRAXFF"><br>
        <sub><b>AG HYDRAXFF</b></sub>
      </a><br>
      <sub>Front end &middot; branding</sub>
    </td>
  </tr>
</table>

* **Forrest Orr** ([@forrest-orr](https://github.com/forrest-orr)) and
  **Gabriel Landau** ([@gabriellandau](https://github.com/gabriellandau)) wrote the
  [Moneta](https://github.com/forrest-orr/moneta) engine — every piece of memory-analysis
  logic in this tool. This project is a front end and packaging layer on top of their work.
* **AG HYDRAXFF** ([@hydraxff_yt](https://www.instagram.com/hydraxff_yt/)) — GUI, build
  system, report writers, branding, and the engine defect fixes listed in
  [NOTICE.md](NOTICE.md).

Full attribution and the change log against upstream are in
**[docs/CONTRIBUTORS.md](docs/CONTRIBUTORS.md)** and **[NOTICE.md](NOTICE.md)**. Contributions
of any size are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

[GNU General Public License v3.0](LICENSE). This project is a derivative work of Moneta and
is distributed under the same terms. No warranty; use at your own risk.

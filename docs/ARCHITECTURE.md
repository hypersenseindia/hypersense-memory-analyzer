# Architecture

## Why a GUI around Moneta

Moneta is a console scanner: you give it a PID, a memory selection and a verbosity, and it
prints findings. HYPERSENSEGLOBAL keeps that engine verbatim and adds the parts an analyst
actually wants during triage — a process picker, mode switches, filters, a live stream and
a report you can hand to someone else.

## Process model

```
┌──────────────────────────────┐
│ HyperSense64.exe             │
│  GUI thread  (Win32, D2D)    │
│  reader thread ── pipe ──┐   │
└──────────────────────────┼───┘
                           │  CreateProcessW(self, "--engine …", CREATE_NO_WINDOW)
                           ▼
                 ┌─────────────────────────┐
                 │ HyperSense64.exe        │
                 │  --engine  (Moneta)     │
                 │  SeDebugPrivilege ↑     │
                 └─────────────────────────┘
```

`WinMain` inspects its own command line with `CommandLineToArgvW`. When the first argument is
`--engine`, it calls `MonetaMain(argc - 1, argv + 1)` and exits; otherwise it starts the GUI.

The GUI spawns **itself** with `--engine` and the Moneta equivalent of the current UI
settings. Consequences:

* **Fault isolation.** The scanner touches third-party memory for a living. If it faults,
  the exit code lands in the log and the window stays alive — no lost session.
* **Clean cancellation.** `STOP` calls `TerminateProcess` on the child; there is no global
  state to unwind and no half-freed engine heap to reason about.
* **No second binary.** Shipping is one file. The equivalent command line is echoed into the
  live view and stored in the report, so a run is reproducible from a terminal.

The child is created with an inherited write handle on an anonymous pipe, `CREATE_NO_WINDOW`
and `STARTF_USESTDHANDLES`; a reader thread turns the byte stream into lines and posts them
to the UI. Mapping UI state to engine arguments is a pure function, which keeps the CLI and
the GUI in agreement.

## Source layout

| Path | Contents |
| --- | --- |
| `HyperSense/src/main.cpp` | Entire front end: window, custom controls, theme, process enumeration and search, argument mapping, child process handling, report writers. |
| `HyperSense/build.bat` | Single-command build (MinGW-w64 + windres). |
| `HyperSense/res/` | Icon, manifest, version resource. |
| `HyperSense/tools/` | Signing helper and PowerShell UI-automation tests used to verify releases. |
| `Source/` | Moneta engine translation units (upstream + the fixes listed in `NOTICE.md`). |
| `Headers/` | Moneta headers. |
| `Resources/`, `Tests/`, `*.vcxproj` | Upstream MSVC project files, kept for reference. |

## Engine life cycle

1. `MonetaMain` parses the argument vector (`-m`, `-p`, `-v`, `-o`, `-f` …).
2. `SeDebugPrivilege` is granted to the child only.
3. The target's PEB, module list and threads are enumerated; regions are walked with
   `VirtualQueryEx` and filtered by the selected memory mode.
4. Each region is examined for IOC patterns (private executable memory, modified image
   pages, unsigned/metadata-less modules, CLR heaps, WOW64 init stubs).
5. Statistics and hits are printed as they are found.
6. Exit code `1` means "scan finished".

The UI never interprets engine output beyond line classification (surface / info / warning /
hit) and the `[ RESULT ]` line used for the summary.

## Reports

`BuildReportText()` and `BuildReportHtml()` render the same model: host, account, SID, OS
build, elevation, engine identification, target, options, timings, exit code, the `RESULT`
row and the complete (uncapped) engine log. The HTML report is a single self-contained file
with inline CSS — no external assets, safe to email.

## Threading

* GUI thread: window procedure, painting, all control state.
* Reader thread: blocking reads from the child pipe, line splitting, `PostMessage` to the
  window.
* Scan-child lifetime is owned by the GUI; `g_scanGen` invalidates late messages from a
  previous run so a stale reader can never paint into a new session.
